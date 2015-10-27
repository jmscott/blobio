//  Synopsis:
//	Front end routines to os.File package.  Most panic on error.
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com

package main

import (
	"os"
	"syscall"

	. "fmt"
	. "time"
)

type file struct {
	path string
	file *os.File
	info os.FileInfo
}

type file_byte_chan chan []byte

//  communicate both before and after rolling files.

type file_roll_notify struct {

	//  path of file currently opened

	open_path string

	//  path of either file about to be rolled to or the just rolled file.
	//  closed, rolled files are never explictly opened again by logger.

	roll_path string

	//  write final or first entry to log before/after rolling
	entries [][]byte
}

func (f *file) stat() {

	fi, err := f.file.Stat()
	if err != nil {
		f.panic(err)
	}
	f.info = fi
}

func (f *file) close() {

	if f == nil || f.file == nil {
		return
	}

	file := f.file
	f.file = nil
	f.info = nil

	err := file.Close()
	if err != nil {
		f.panic(err)
	}
}

//  open a file for reading
func (*file) open(path string, must_exist bool) *file {

	f, err := os.OpenFile(path, os.O_RDONLY, 0)
	if err == nil {
		return &file{
			path: path,
			file: f,
		}
	}
	if os.IsNotExist(err) && !must_exist {
		return nil
	}
	panic(err)
}

func (f *file) tell() (position int64) {

	var err error

	position, err = f.file.Seek(0, 1)
	if err != nil {
		f.panic(err)
	}
	return position
}

func (f *file) is_named_pipe() bool {

	return ((f.info.Mode() & os.ModeType & os.ModeNamedPipe) != 0)
}

func (f *file) panic(err error) {
	panic(Sprintf("%s: %s", f.path, err.Error()))
}

func (*file) open_append(path string) *file {
	f, err := os.OpenFile(
		path,
		syscall.O_APPEND|syscall.O_CREAT|syscall.O_WRONLY,
		06666,
	)
	if err == nil {
		return &file{
			path: path,
			file: f,
		}
	}
	panic(err)
}

func (*file) open_truncate(path string) *file {
	f, err := os.OpenFile(
		path,
		syscall.O_TRUNC|syscall.O_CREAT|syscall.O_WRONLY,
		06666,
	)
	if err == nil {
		return &file{
			path: path,
			file: f,
		}
	}
	panic(err)
}

func (f *file) write(bytes []byte) {

	//  Note: can Write return < len(bytes) without error?
	_, err := f.file.Write(bytes)
	if err != nil {
		f.panic(err)
	}
}

func (f *file) rename(new_path string) {
	err := os.Rename(f.path, new_path)
	if err != nil {
		panic(err)
	}
	f.path = new_path
}

func (f *file) unlink(path string) {

	err := syscall.Unlink(path)
	if err == nil || os.IsNotExist(err) {
		return
	}
	panic(err)
}

//  Map input of file_byte_channel onto a file, rolling the file
//  after a certain duration has expired and the time stamp in the file
//  path has changed.  Optionally, the starting and ending roll events
//  may be communicated over two channels returned to the caller.
//
//  To close the file just close the input channel.
//
//  The active file may be named either <prefix>.<suffix> or as
//
//		<prefix>-Dow.<suffix>
//		<prefix>-YYMMDD.<suffix>
//		<prefix>-YYYYMMDD_hhmmss.<suffix>
//		<prefix>-epoch.<suffix>
//
//  depending upon the values of stamp_format and stamp_active.
//
//  Log roll events are communicated over the 'start' and 'end' channels, which
//  may be null.  See file_roll_event{}.
//
//  Writes are - nay, must be - unbuffered.
//
//  To terminate the logging, close the 'in' channel.
//
//  Log roll events are communicated over the 'start' and 'end' channels, which
//  may be null.

func (in file_byte_chan) map_roll(
	prefix, stamp_format, suffix string,
	roll_duration Duration,
	stamp_active, notify_roll bool,
) (start, end chan *file_roll_notify) {

	if notify_roll {
		start = make(chan *file_roll_notify)
		end = make(chan *file_roll_notify)
	}

	make_stamp := func() string {
		switch stamp_format {
		case "Dow":
			return (Now().Weekday().String())[0:3]
		case "YYYYMMDD":
			return Now().Format("20060102")
		case "epoch":
			return Sprintf("%d", Now().Unix())
		case "YYYYMMDD_hhmmss":
			return Now().Format("20060102_150405")
		default:
			panic("unknown time stamp format: " + stamp_format)
		}
	}

	make_active_path := func() string {
		if stamp_active {
			return Sprintf("%s-%s.%s", prefix, make_stamp(), suffix)
		}
		return Sprintf("%s.%s", prefix, suffix)
	}

	go func() {
		defer func() {
			if start != nil {
				close(start)
				close(end)
			}
		}()
		stamp := make_stamp()
		open_path := make_active_path()

		//  log file
		var out *file

		// first open is append
		out = out.open_append(open_path)
		defer func() {
			out.close()
		}()

		roll := NewTicker(roll_duration).C

		for {
			select {

			case bytes := <-in:
				if bytes == nil {
					return
				}
				out.write(bytes)

			case <-roll:
				//  did the time stamp change
				st := make_stamp()
				if st == stamp {
					continue
				}
				roll_path := make_active_path()

				var fr *file_roll_notify

				//  notify the caller of the start of the roll

				if notify_roll {
					fr = &file_roll_notify{
						open_path: open_path,
						roll_path: roll_path,
					}

					//  inform caller of start of log roll
					start <- fr
					fr = <-start

					//  write a final entries before rolling
					for _, e := range fr.entries {
						out.write(e)
					}
					fr.entries = nil
				}
				out.close()

				//  since active file has no stamp we must
				//  rename the active with the previous
				//  timestamp

				if !stamp_active {
					out.rename(Sprintf("%s-%s.%s",
						prefix, stamp, suffix))
				}
				stamp = st

				//  new log is always truncated on roll

				out = out.open_truncate(roll_path)
				closed_path := open_path
				open_path = roll_path

				//  notify the caller of the finished roll
				if notify_roll {
					//  send post roll event to watcher
					fr.open_path = open_path
					fr.roll_path = closed_path

					//  inform caller of end of roll event
					end <- fr
					fr = <-end
					for _, e := range fr.entries {
						out.write(e)
					}
				}
			}
		}
	}()

	return start, end
}
