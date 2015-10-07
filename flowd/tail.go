//
//  Synopsis:
//	Channelized poll for lines of text from a typical append only log file,
//	in the manner of the 'tail -f' unix command line tool.
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
//  Note:
//	Investigate rolling algorithm.  On linux I (jmscott) witnessed a
//	a tail on a wrapped log file that never rolled to spool/biod.brr.
//
//	Investigate adding "ignore_empty" to either the tail or the command.
//
//	Should tail create a file which does not appear?  Probably not.

package main

import (
	"bufio"
	"io"
	"os"
	"strings"

	. "fmt"
	. "time"
)

type tail struct {
	name string
	path string

	eof_pause        Duration
	max_reopen_pause Duration
	output_capacity  uint16
}

const (
	tAIL_MAX_REOPEN_PAUSE = 8 * Second
	tAIL_EOF_PAUSE        = 250 * Millisecond
)

func (t *tail) open() *file {

	var f *file

	for pause := t.eof_pause; pause < t.max_reopen_pause; pause *= 2 {
		f = f.open(t.path, false)
		if f != nil {
			return f
		}
		Sleep(pause)
	}
	panic(Sprintf("expected file never appeared after %s: %s",
		t.max_reopen_pause, t.path))
}

//  Poll for lines of text from a file in the manner of the 'tail -f'
//  unix command line tool.

func (t *tail) forever() (out chan string) {

	if t.max_reopen_pause <= 0 {
		t.max_reopen_pause = tAIL_MAX_REOPEN_PAUSE
	}
	if t.eof_pause <= 0 {
		t.eof_pause = tAIL_EOF_PAUSE
	}
	if t.eof_pause > t.max_reopen_pause {
		panic(Sprintf("eof_pause(%s) > max_reopen_pause(%s)",
			t.eof_pause, t.max_reopen_pause))
	}

	//  open the source file referenced in current tail
	open := func() (f *file) {

		pause := t.eof_pause
		for pause < t.max_reopen_pause {
			f = f.open(t.path, false)
			if f != nil {
				return f
			}
			Sleep(pause)
			pause *= 2
		}
		panic(Sprintf("expected file never appeared after %s: %s",
			t.max_reopen_pause, t.path))
	}

	out = make(chan string, t.output_capacity)

	//  handle an eof condition on the tailed log file.
	//
	//  first determine if the file has rolled by comparing the inodes
	//  of reopened file with same path to the currently opened file.
	//  if the inodes are the same, then nothing rolled, so just sleep
	//  briefly and return first file.  if the inodes are different, then
	//  the file rolled, so check if any unread data exists on the old,
	//  rolled file.  if data exists then return so then caller can drain.
	//  if no data exists then close old file and return newly opened file.

	eof := func(f1 *file, draining bool) (f2 *file, _ bool) {

		if f1.info == nil {
			f1.stat()
		}
		f2 = open()
		f2.stat()

		//  no change in file inode
		if os.SameFile(f1.info, f2.info) {
			f2.close()
			return f1, false
		}

		//  now we know the inode underlying the f1 file is not
		//  referenced by orginal path.  in other words, the f1 file
		//  has rolled. this implies that the amount of data queued up
		//  in f1 is both finite and will never grow from this point on.

		//  draining indicates that we have seen the second eof on
		//  rolled file, so it's time to close f1 for good and
		//  point the caller to f2

		if draining {
			//  the file path
			//  reference the inode of f1,
			//  so tis safe to close without draining
			f1.close()
			return f2, false
		}

		//  file has rolled but f1 may not be drained.
		//
		//  Note:
		//	unfortunatly a tolerable race condition exists: f2 may
		//	roll away while f1 still draining.  while this probably
		//	won't happen, the problem still exists.  might consider
		//	a merkle tree type fix similar to how biod rolls brr
		//	logs

		f2.close()
		return f1, true
	}

	//  watch the file and write lines to output
	go func(src *file) {
		defer close(out)
		defer func() {
			src.close()
		}()

		in := bufio.NewReader(src.file)

		//  draining means the input file has rolled or disappeared
		//  and that possible input remains to be read by the tail.
		//  we also assume that a draining file implies that the next
		//  end-of-file is the final end of file.

		var draining bool

		for {
			line, err := in.ReadString('\n') //  maximum?

			if err == nil {
				out <- strings.TrimRight(line, "\n")
				continue
			}
			if err != io.EOF {
				panic(err.Error())
			}

			//  at end of file
			src, draining = eof(src, draining)

			//  sleep briefly before rebuilding the buffered
			//  reader
			Sleep(t.eof_pause)

			//  Note:
			//	bufio.NewReader must NOT reset the current
			//	file position.  I (jmscott) find no mention
			//	of the status of the file position in the
			//	docs.
			in = bufio.NewReader(src.file)
		}
	}(open())	//  should the open be asynchronous?

	return out
}
