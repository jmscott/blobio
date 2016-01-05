//  Synopsis:
//	Exec commands as separate processes
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
//  Note:
//	A clean shutdown can trigger i/o panic() when flowd-execv catches INT
//	signal.
//
package main

import (
	"bufio"
	"errors"
	"io"
	"os/exec"
	"strconv"
	"strings"
	"syscall"

	. "fmt"
	. "time"
)

//  a command as defined in a flowd configuration file
type command struct {

	//  tag name flow configuration, unique across all named entities
	name string

	//  relative file system path to executable
	path string

	//  expanded path to executable
	full_path string

	//  static, prepended argv[], before call evaluated
	argv []string

	//  Bit map of exit status codes classified as "OK" in xdr record.
	//  Codes not in the bitmap are classifed as "ERR" in the xdr record.
	//
	//  Note:
	//	Why not an [32]byte.  Are arrays faster to index than slices.

	OK_exit_status []byte

	called bool

	//  count of references to this command from either the 'when'
	//  clause or the argument list or call() or query()

	depend_ref_count uint8
}

type os_exec_reply struct {
	started         bool
	output_256      []byte //  process output
	exit_status     uint8
	signal          uint8
	err             error
	system_duration Duration
	user_duration   Duration
}

//  a static call statement in the configuration file
type call struct {
	command *command
}

type os_exec_request struct {
	*command
	argv []string

	reply chan os_exec_reply
}

type os_exec_chan chan os_exec_request

//  answer requests to exec a command with a particular
func (in os_exec_chan) worker() {

	for req := range in {

		//  the first argument must be the command path
		gcmd := &exec.Cmd{
			Path: req.command.full_path,
			Args: req.argv,
		}

		reply := os_exec_reply{}

		//  capture first 256 bytes of output of process
		//
		//  Note:
		//	Runaway output will hose flowd!!!
		//
		//	Need to verify output is proper utf8.
		//	Think about adding an 'is_utf8' flag to xdr_value

		reply.output_256, reply.err = gcmd.CombinedOutput()

		//  Catch panicy startup errors.  Filter out CombinedOutput's
		//  return of an error when either the process exit status > 0
		//  or a signal interupted the process
		//
		//  Note:
		//	This filtering of the output string is goofy.
		//	For example, the 'signal ' pattern appears to have
		//	changed to 'signal: ', breaking code.
		//
		//	Eventually need to not depend on CombinedOutput()

		if reply.err != nil &&
			!strings.HasPrefix(reply.err.Error(), "exit status") &&
			!strings.HasPrefix(reply.err.Error(), "signal") {

			req.reply <- reply
			continue
		}
		reply.err = nil

		//  Fetch the process exit/signal state&codes and
		//  system/user duration

		ps := gcmd.ProcessState
		if ps == nil {
			reply.err = errors.New("os.exec.ProcessState is nil")
			req.reply <- reply
			continue
		}
		reply.started = true

		reply.system_duration = ps.SystemTime()
		reply.user_duration = ps.UserTime()

		sys := ps.Sys()
		ws := uint16(sys.(syscall.WaitStatus))
		if sys.(syscall.WaitStatus).Signaled() {
			reply.signal = uint8(ws)
		} else {
			reply.exit_status = uint8(ws >> 8)
		}
		req.reply <- reply
	}
}

//  answer requests to exec a command with a particular
func (in os_exec_chan) worker_flowd_execv() {

	//  start a flowd-execv process

	flowd_execv_path, err := exec.LookPath("flowd-execv")
	if err != nil {
		panic(err)
	}

	cmd := &exec.Cmd{
		Path: flowd_execv_path,
		Args: []string{"flowd-execv"},
	}

	cmd_pipe_in, err := cmd.StdinPipe()
	if err != nil {
		panic(err)
	}

	cmd_pipe_out, err := cmd.StdoutPipe()
	if err != nil {
		panic(err)
	}
	cmd_out := bufio.NewReader(cmd_pipe_out)

	cmd_pipe_err, err := cmd.StderrPipe()
	if err != nil {
		panic(err)
	}
	cmd_err := bufio.NewReader(cmd_pipe_err)

	err = cmd.Start()
	if err != nil {
		panic(err)
	}

	//  listen on stderr for a dieing flowd-execv

	go func() {

		out, err := cmd_err.ReadString('\n')
		if err != nil && err != io.EOF {
			panic(err)
		}
		if len(out) > 0 {
			panic(Sprintf("unexpected exit of flowd-execv: %s", out))
		}

		//  no output on stderr indicates a clean close due to
		//  flowd-execv being cleanly shutdown.
	}()

	for req := range in {

		reqs := strings.Join(req.argv, "\t") + "\n"
		//  write request to exec command to flowd-execv process
		_, err := cmd_pipe_in.Write([]byte(reqs))
		if err != nil {
			if err == io.EOF {
				return
			}
			panic(err)
		}

		reps, err := cmd_out.ReadString('\n')
		if err != nil {
			panic(err)
		}
		reps = strings.TrimSuffix(reps, "\n")

		rep := strings.Split(reps, "\t")
		reply := os_exec_reply{}

		if rep[0] == "ERROR" {
			//  Note:  reply ought to see more than on line of
			//         error output.
			reply.err = errors.New(rep[1])
			req.reply <- reply
			continue
		}

		reply.user_duration, err = ParseDuration(rep[2] + "s")
		if err != nil {
			panic(err)
		}

		reply.system_duration, err = ParseDuration(rep[3] + "s")
		if err != nil {
			panic(err)
		}
		if strings.Contains(rep[0], "+") {
			reps, err = cmd_out.ReadString('\n')
			if err != nil {
				panic(err)
			}
			reply.output_256 = []byte(reps)
			if len(reply.output_256) > 255 {
				reply.output_256 = reply.output_256[:256]
			}
		}

		reply.started = true

		tc, err := strconv.ParseUint(rep[1], 10, 8)
		if err != nil {
			panic(err)
		}
		if strings.HasPrefix(rep[0], "EXIT") {
			reply.exit_status = uint8(tc)
		} else {
			reply.signal = uint8(tc)
		}
		req.reply <- reply
	}
}

func (cmd *command) call(argv []string, osx_q os_exec_chan) (xv *xdr_value) {

	xdr := &xdr{
		call_name: cmd.name,
	}

	//  final argument vector sent to command looks like
	//
	//	argv[0]:	full path to command
	//	argv[1...m]:	static args defined in command{} block
	//	argv[m+1...]:	dynamic args bound to call()
	//
	//  Note:
	//	why can't the argv[] be allocated by the caller!!?
	//
	cargc := len(cmd.argv)
	xargv := make([]string, 1+cargc+len(argv))

	//  the first argument must be the command path
	xargv[0] = cmd.full_path
	copy(xargv[1:cargc+1], cmd.argv[:])
	copy(xargv[cargc+1:], argv[:])

	xv = &xdr_value{
		xdr: xdr,
	}

	req := os_exec_request{
		command: cmd,
		argv:    xargv[:],
		reply:   make(chan os_exec_reply),
	}

	//  send a request to exec the command
	osx_q <- req

	//  wait for reply from os_exec worker
	//
	//  Note:
	//	need to grumble about long running processes!
	//	unfortunatly a timeout appears to be expensive in select{}

	reply := <-req.reply

	//  capture first 256 bytes of any process output
	len := len(reply.output_256)
	if len > 0 {
		if len > 256 {
			xv.output_256 = reply.output_256[:256]
		} else {
			xv.output_256 = reply.output_256
		}
	}

	//  record system and user duration as time consumed
	xdr.system_duration = reply.system_duration
	xdr.user_duration = reply.user_duration

	//  error occured on process startup error occured
	if reply.err != nil {

		//  Note:
		//	Seems like NOPS will always be the code

		if reply.started {
			xdr.termination_class = "ERR"
		} else {
			xdr.termination_class = "NOPS"
		}
		xv.err = reply.err
		return
	}

	//  process was interrupted with a signal

	if reply.signal > 0 {
		xdr.termination_class = "SIG"
		xdr.termination_code = uint64(reply.signal)
		return
	}

	//  process ran and exited.  classify the exit code as OK or ERR

	xdr.termination_code = uint64(reply.exit_status)

	//  OK bit map identifies which exit codes are to be classified as OK

	if cmd.OK_exit_status != nil {
		if cmd.OK_exit_status[xdr.termination_code/8]&
			(0x1<<(xdr.termination_code%8)) != 0 {
			xdr.termination_class = "OK"
		} else {
			xdr.termination_class = "ERR"
		}
	} else if xdr.termination_code == 0 {
		xdr.termination_class = "OK"
	} else {
		xdr.termination_class = "ERR"
	}

	return xv
}
