/*
 *  Synopsis:
 *	Summarize a roll of blob request records as json blob.
 *  Usage:
 *	roll2stat_json <roll-set-udig>
 *	roll2stat_json --verbose <roll-set-udig>
 *  Exit Status:
 *	0	json written, all blobs fetched, no errors
 *	1	roll or brr log blob does not exist, no json written
 *	2	unexpected error
 *  Note:
 *	Think about "space" command-line argument.
 */

package main;

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"
	"sync"
	"time"
)

var udig_re = regexp.MustCompile(`^[a-z][a-z0-9]{0,7}:[[:graph:]]{32,128}$`)
var SERVICE_re = regexp.MustCompile(`^[a-z][a-z0-9]{0,7}:[[:graph:]]{1,128}$`)

var verbose bool
var BLOBIO_SERVICE string

type blob_stream struct {
	out	*bufio.Scanner
	cmd	*exec.Cmd
}

type stat struct {
	BRRLogCount		uint64		`json:"brr_log_count"`
	BRRCount		uint64		`json:"brr_count"`

	PrevRollUDig		string		`json:"prev_roll_udig"`
	PrevRollStartTime	string		`json:"prev_roll_start_time"`
	PrevRollWallTime	string		`json:"prev_roll_wall_time"`

	UDigCount		uint64		`json:"udig_count"`

	MinBRRStartTime		time.Time	`json:"min_brr_start_time"`
	MaxBRRStartTime		time.Time	`json:"max_brr_start_time"`
	MaxBRRWallDuration	time.Duration	`json:"max_brr_wall_duration"`
	MaxBRRBlobSize		uint64		`json:"max_brr_blob_size"`

	EatOkCount		uint64		`json:"eat_ok_count"`
	EatNoCount		uint64		`json:"eat_no_count"`

	GetCount		uint64		`json:"get_count"`
	GetByteCount		uint64		`json:"get_byte_count"`

	TakeCount		uint64		`json:"take_count"`
	TakeByteCount		uint64		`json:"take_byte_count"`

	PutCount		uint64		`json:"get_count"`
	PutByteCount		uint64		`json:"put_byte_count"`

	GiveCount		uint64		`json:"give_count"`
	GiveByteCount		uint64		`json:"give_byte_count"`

	OkCount			uint64		`json:"ok_count"`
	NoCount			uint64		`json:"no_count"`
}

type roll2stat struct {
	Argc			int		`json:"argc"`
	Argv			[]string 	`json:"argv"`
	Env			[]string	`json:"environment"`

	RollBlob		string		`json:"roll_blob"`
	Stat			stat		`json:"roll2stat.blob.io"`

	WorkDir			string		`json:"work_dir"`

	stat_mux		sync.Mutex
}
var r2s *roll2stat

func ERROR(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...)
}

func leave(exit_status int) {

	if r2s == nil {			//  no hello, world
		os.Exit(exit_status)
	}
	if r2s.WorkDir != "" {
		err := os.Chdir(os.TempDir())
		if err == nil {
			err := os.RemoveAll(r2s.WorkDir)
			if err != nil {
				ERROR("os.RemoveAll(work_dir) failed: %s", err)
				exit_status = 1
			}
		} else {
			ERROR("os.Chdir(TempDir) failed: %s", err)
			exit_status = 1
		}
	}
	info("good bye, cruel world")
	os.Exit(exit_status)
}

func die(format string, args ...interface{}) {

	ERROR(format, args...)
	leave(2)
}

func info(format string, args ...interface{}) {
	if verbose {
		fmt.Fprintf(os.Stderr, format + "\n", args...)
	}
}

func init() {
	if len(os.Args) < 2 {
		die("must be at least command line args")
	}

	//  extract roll_udig as first argument.
	if !udig_re.MatchString(os.Args[1]) {
		die("roll udig not a udig: %s", os.Args[1])
	}

	var opt string

	_eo := func(msg string) {
		die(opt + ": %s", msg)
	}

	_eo2 := func() {
		_eo("given twice")
	}
	argc := len(os.Args)
	for i := 2;  i < argc;  i++ {
		opt = os.Args[i]
		if opt == "--verbose" {
			if verbose {
				_eo2()
			}
			verbose = true
		} else if opt == "--BLOBIO_SERVICE" {
			if BLOBIO_SERVICE != "" {
				_eo2()
			}
			i++
			if i == argc {
				_eo("missing service url")
			}
			BLOBIO_SERVICE = os.Args[i]
			if !SERVICE_re.MatchString(BLOBIO_SERVICE) {
				_eo("not a BLOBIO_SERVICE: " + BLOBIO_SERVICE)
			}
		} else if strings.HasPrefix(opt, "--") {
			die("unknown option: %s", opt)
		} else {
			die("unknown command line arg: %s", opt)
		}
	}
	if BLOBIO_SERVICE == "" {
		die("missing required option: --BLOBIO_SERVICE <service url>")
	}
}

func goto_work_dir() {
	r2s.WorkDir = fmt.Sprintf(
				"%s/roll2stat_json-%d.d",
				os.TempDir(),
				os.Getpid(),
	)
	info("work dir: %s", r2s.WorkDir)
	err := os.Mkdir(r2s.WorkDir, 0700)
	if err != nil {
		die("os.Mkdir(work) failed: %s", err)
	}
	err = os.Chdir(r2s.WorkDir)
	if err != nil {
		die("os.Chdir(work) failed: %s", err)
	}
}

func scan_brr_log(brr_log string, done chan interface{}) {
	done <- interface{}(nil)
}

func open_stream(blob, what string) *blob_stream {

	_df := what + ": " + "stream_blob: "
	_die := func(format string, args ...interface{}) {
		die(_df + format, args...)
	}

	cmd := exec.Command(
			"blobio",
			"get",
			"--udig",
			blob,
			"--service",
			BLOBIO_SERVICE,
		)
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		_die("exec.cmd.StdoutPipe() failed: %s", err)
	}
	err = cmd.Start()
	if err != nil {
		_die("exec.cmd.Start() failed: %s", err)
	}
	return &blob_stream{
		out:	bufio.NewScanner(stdout),
		cmd:	cmd,
	}
}

func scan_roll(done chan interface{}) {

	_die := func(format string, args ...interface{}) {
		die("scan_roll: " + format, args...)
	}

	rs := open_stream(r2s.RollBlob, `scan_roll`)
	in := rs.out
	for in.Scan() {
		r2s.Stat.BRRLogCount++
		ud := in.Text()
		if !udig_re.MatchString(ud) {
			_die(
				"line %d: not udig in brr set: %s",
				r2s.Stat.BRRLogCount,
				r2s.RollBlob,
			)
		}
		go scan_brr_log(ud, done)
	}
	cmd := rs.cmd

	ps, err := cmd.Process.Wait()
	if err != nil {
		_die("cmd.Process.Wait() failed: %s", err)
	}
	ex := ps.ExitCode()
	if ex > 1 {
		_die("cmd.ProcessState.ExitCode > 0")
	}
	if ex == 1 {
		ERROR("no roll blob: %s", r2s.RollBlob)
		leave(1)
	}
}

func main() {

	info("hello, world")
	info("PATH=%s", os.Getenv("PATH"))

	r2s = &roll2stat{
			Argc:		len(os.Args),
			Argv:		os.Args,
			Env:		os.Environ(),
		  }
	r2s.RollBlob = os.Args[1]
	info("roll blob: %s", r2s.RollBlob)
	info("BLOBIO_SERVICE=%s", os.Getenv("BLOBIO_SERVICE"))

	goto_work_dir()

	done := make(chan interface{})
	scan_roll(done)

	for i := uint64(0);  i < r2s.Stat.BRRLogCount;  i++ {
		<- done
	}

	//  write json version of stat to standard output
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "\t")
	enc.Encode(r2s)

	leave(0)
}
