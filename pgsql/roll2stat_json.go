/*
 *  Synopsis:
 *	Summarize a roll of blob request records as json blob.
 *  Usage:
 *	roll2stat_json <blobio-service> <roll-set-udig> 
 *	roll2stat_json <blobio-service> <roll-set-udig> --verbose
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
	"errors"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
)

var udig_re = regexp.MustCompile(`^[a-z][a-z0-9]{0,7}:[[:graph:]]{32,128}$`)
var SERVICE_re = regexp.MustCompile(`^[a-z][a-z3-9]{0,7}:[[:graph:]]{1,128}$`)

//  match 2021-12-12T06:39:58.528973000+00:00

var RFC3339Nano_re = regexp.MustCompile(
   	`^[23]\d\d\d-[01]\d-[0123]\d`  	+  //  year
	`T`				+  //  start of time of day
	`[0-2]\d:`			+  //  hour
	`[0-6]\d:`			+  //  minute
	`[0-6]\d\.\d{1,9}`		+  //  second
	`[+-][0-2]\d:[0-6]\d$`,		   // timezone
)

var transport_re = regexp.MustCompile(
	`^[a-z][a-z0-9]{0,7}~[[:graph:]]{1,128}$`,
)

var verb_re = regexp.MustCompile(
	`^(get|put|give|take|eat|wrap|roll)$`,
)

var chat_history_re = regexp.MustCompile(
	`((ok|no)|((ok,ok)|(ok,no))|(ok,ok,ok)|(ok,ok,no))$`,
)
var blob_size_re = regexp.MustCompile(`^0|([1-9][0-9]{0,18})$`)
var wall_duration_re = regexp.MustCompile(`[0-9]{1,10}(\.[0-9]{0,9})$`)

var verbose bool

type blob_stream struct {
	what	string
	out	*bufio.Scanner
	cmd	*exec.Cmd
}

type brr_duration struct {
	duration	time.Duration
}

func (d brr_duration) MarshalJSON() ([]byte, error) {
	return json.Marshal(d.duration.String())
}

func (d *brr_duration) UnmarshalJSON(b []byte) error {
	var v interface{}

	if err := json.Unmarshal(b, &v); err != nil {
		return err
	}

	switch value := v.(type) {
	case float64:
		d.duration = time.Duration(value)
		return nil
	case string:
		var err error
		d.duration, err = time.ParseDuration(value)
		if err != nil {
		    return err
		}
		return nil
	default:
		return errors.New(fmt.Sprintf("invalid duration: %s", d))
    }
}

type BRR struct {
	StartTime	time.Time       `json:"start_time"`
	Transport	string       	`json:"transport"`
	Verb		string		`json:"verb"`
	Blob		string		`json:"blob"`
	ChatHistory	string		`json:"chat_history"`
	BlobSize	uint64		`json:"blob_size"`
	WallDuration	time.Duration	`json:"wall_duration"`
}

type stat struct {
	RollBRRCount		uint64		`json:"roll_brr_count"`
	BRRCount		uint64		`json:"brr_count"`

	PrevRoll		*BRR		`json:"prev_roll,omitempty"`

	UDigCount		uint64		`json:"udig_count"`

	MinBRRStartTime		time.Time	`json:"min_brr_start_time"`
	MaxBRRStartTime		time.Time	`json:"max_brr_start_time"`

	MaxBRRWallDuration	brr_duration	`json:"max_brr_wall_duration"`
	MaxBRRBlobSize		uint64		`json:"max_brr_blob_size"`

	EatOkCount		uint64		`json:"eat_ok_count"`
	EatNoCount		uint64		`json:"eat_no_count"`

	GetOkCount		uint64		`json:"get_ok_count"`
	GetNoCount		uint64		`json:"get_no_count"`
	GetBlobSize		uint64		`json:"get_blob_size"`

	TakeOkCount		uint64		`json:"take_ok_count"`
	TakeNoCount		uint64		`json:"take_no_count"`
	TakeBlobSize		uint64		`json:"take_blob_size"`

	PutOkCount		uint64		`json:"put_ok_count"`
	PutNoCount		uint64		`json:"put_no_count"`
	PutBlobSize		uint64		`json:"put_blob_size"`

	GiveOkCount		uint64		`json:"give_ok_count"`
	GiveNoCount		uint64		`json:"give_no_count"`
	GiveBlobSize		uint64		`json:"give_blob_size"`

	WrapOkCount		uint64		`json:"wrap_ok_count"`
	WrapNoCount		uint64		`json:"wrap_no_count"`

	RollOkCount		uint64		`json:"roll_ok_count"`
	RollNoCount		uint64		`json:"roll_no_count"`
}

type roll2stat struct {
	Argc			int		`json:"argc"`
	Argv			[]string 	`json:"argv"`
	Env			[]string	`json:"environment"`

	RollBlob		string		`json:"roll_blob"`
	BLOBIO_SERVICE		string		`json:"BLOBIO_SERVICE"`
	Stat			stat		`json:"roll2stat.blob.io"`

	mux			sync.Mutex
	udig_set		map[string]bool
}
var r2s *roll2stat

func ERROR(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...)
}

func leave(exit_status int) {

	if r2s == nil {			//  no hello, world
		os.Exit(exit_status)
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
	argc := len(os.Args)
	argc--;
	if argc != 2 && argc != 3 {
		die("wrong arg count: got %d, expected 2 or 3", argc)
	}

	if !SERVICE_re.MatchString(os.Args[1]) {
		die("not blobio service: %s", os.Args[1])
	}

	//  extract roll_udig as second argument.
	if !udig_re.MatchString(os.Args[2]) {
		die("not a roll udig: %s", os.Args[2])
	}
	if argc == 4 {
		if os.Args[3] == "--verbose" {
			verbose = true
		} else {
			die("unknown command line argument: %s", os.Args[3])
		}
	}
	r2s = &roll2stat{
			Argc:		len(os.Args),
			Argv:		os.Args,
			Env:		os.Environ(),
			BLOBIO_SERVICE:	os.Args[1],
			RollBlob:	os.Args[2],

			udig_set:	make(map[string]bool),
		  }
}

func is_empty_udig(blob string) bool {
	return blob == `sha:da39a3ee5e6b4b0d3255bfef95601890afd80709` ||
	       blob == `bc160:b472a266d0bd89c13706a4132ccfb16f7c3b9fcb` ||
	       blob == `btc20:fd7b15dc5dc2039556693555c2b81b36c8deec15`
}

func scan_brr_log(brr_log string, done chan interface{}) {

	lino := uint64(0)

	_die := func(format string, args ...interface{}) {
		fmt := fmt.Sprintf("scan_brr_log: line %d: %s", lino, format)
		die(fmt, args...)
	}

	_bdie := func(what, fld string) {
		_die("brr: not a " + what + ": %s", fld)
	}

	_cdie := func(verb, chat_history string) {
		_die("impossible %s chat history: %s", verb, chat_history)
	}

	bs := open_stream(brr_log, `scan_brr_log`)

	in := bs.out
	for in.Scan() {
		lino++

		/*
		 *  parse the single blob request record
		 *
		 *  start_time		#  YYYY-MM-DDThh:mm:ss.ns[+-]HH:MM
		 *  transport		#  [a-z][a-z0-9]{0,7}~[[:graph:]]{1,128}
		 *  verb		#  get/put/take/give/eat/wrap/roll
		 *  algorithm:digest	#  [a-z][a-z0-9]{0,7}:		\
		 *		 	#	[[:graph:]]{32,128}
		 *  chat_history	#  ok/no handshake between server&client
		 *  blob_size		#  unsigned 63 bit long <= 2^63
		 *  wall_duration	#  request wall duration in sec.ns>=0
		 */

		brr := in.Text()

		fld := strings.Split(brr, "\t")
		if len(fld) != 7 {
			_bdie("len(tab brr) != 7", string(len(fld)))
		}
		if !RFC3339Nano_re.MatchString(fld[0]) {
			_bdie(`start time`, fld[0])
		}

		if !transport_re.MatchString(fld[1]) {
			_bdie(`transport`, fld[1])
		}

		if !verb_re.MatchString(fld[2]) {
			_bdie(`verb`, fld[2])
		}

		if !udig_re.MatchString(fld[3]) {
			_bdie(`udig`, fld[3])
		}

		if !chat_history_re.MatchString(fld[4]) {
			_bdie(`chat_history`, fld[4])
		}
		chat_history := fld[4]

		if !blob_size_re.MatchString(fld[5]) {
			_bdie(`udig`, fld[5])
		}
		blob_size, err := strconv.ParseUint(fld[5], 10, 63)
		if err != nil {
			_bdie(`blob_size`, fld[5])
		}

		if !wall_duration_re.MatchString(fld[6]) {
			_bdie(`wall_duration`, fld[6])
		}

		r2s.mux.Lock()
		r2s.Stat.BRRCount++
		r2s.mux.Unlock()

		//  properly parse start_time
		start_time, err := time.Parse(time.RFC3339Nano, fld[0])
		if err != nil {
			_die("can not parse start time: %s", err)
		}

		//  check Stat.MinBRRStartTime
		r2s.mux.Lock()
		if r2s.Stat.MinBRRStartTime.IsZero() {
			r2s.Stat.MinBRRStartTime = start_time
		} else if r2s.Stat.MinBRRStartTime.After(start_time) {
			r2s.Stat.MinBRRStartTime = start_time
		}
		r2s.mux.Unlock()

		//  check Stat.MaxBRRStartTime
		r2s.mux.Lock()
		if r2s.Stat.MaxBRRStartTime.IsZero() {
			r2s.Stat.MaxBRRStartTime = start_time
		} else if r2s.Stat.MaxBRRStartTime.Before(start_time) {
			r2s.Stat.MaxBRRStartTime = start_time
		}
		r2s.mux.Unlock()

		//  parse the wall duration
		wall_duration, err := time.ParseDuration(fld[6] + "s")
		if err != nil {
			_die("time.ParseDuration(wall) failed: %s", err)
		}

		r2s.mux.Lock()
		switch fld[2] {
		case "eat":
			switch chat_history {
			case "ok":
				r2s.Stat.EatOkCount++
			case "no":
				r2s.Stat.EatNoCount++
			default:
				_cdie("eat", chat_history)
			}
		case "get":
			switch chat_history {
			case "ok":
				r2s.Stat.GetOkCount++
				r2s.Stat.GetBlobSize += blob_size
			case "no":
				r2s.Stat.GetNoCount++
			default:
				_cdie("get", chat_history)
			}
		case "put":
			switch chat_history {
			case "ok,ok":
				r2s.Stat.PutOkCount++
			case "no":
				r2s.Stat.PutNoCount++
			case "ok,no":
				r2s.Stat.PutNoCount++
			default:
				_cdie("put", chat_history)
			}
			r2s.Stat.PutBlobSize += blob_size
		case "give":
			switch chat_history {
			case "ok,ok,ok":
				r2s.Stat.GiveOkCount++
			case "ok,no":
			case "ok,ok,no":
			case "no":
				r2s.Stat.GiveNoCount++
			default:
				_cdie("give", chat_history)
			}
			r2s.Stat.GiveBlobSize += blob_size
		case "take":
			switch chat_history {
			case "ok,ok,ok":
				r2s.Stat.TakeOkCount++
				r2s.Stat.TakeBlobSize += blob_size
			case "ok,no":
			case "ok,ok,no":
				r2s.Stat.TakeNoCount++
				r2s.Stat.TakeBlobSize += blob_size
			case "no":
				r2s.Stat.TakeNoCount++
			default:
				_cdie("take", chat_history)
			}
		case "roll":
			if r2s.Stat.PrevRoll != nil {
				die("roll blob seen twice: %s", fld[5])
			}
			r2s.Stat.PrevRoll = &BRR{
				StartTime:	start_time,
				Transport:	fld[1],
				Verb:		fld[2],
				Blob:		fld[3],
				ChatHistory:	fld[4],
				BlobSize:	blob_size,
				WallDuration:	wall_duration,
			}
			switch chat_history {
			case "ok":
				r2s.Stat.RollOkCount++
			case "no":
				r2s.Stat.RollNoCount++
			default:
				_cdie("roll", chat_history)
			}
		case "wrap":
			switch chat_history {
			case "ok":
				r2s.Stat.WrapOkCount++
			case "no":
				r2s.Stat.WrapNoCount++
			default:
				_cdie("wrap", chat_history)
			}
		default:
			_die("impossible brr verb: %s", fld[2])
		}
		r2s.mux.Unlock()

		//  unique blob udigs
		r2s.mux.Lock()
		if r2s.udig_set[fld[3]] == false {
			r2s.udig_set[fld[3]] = true
		}
		r2s.mux.Unlock()

		//  max wall duration
		r2s.mux.Lock()
		if r2s.Stat.MaxBRRWallDuration.duration < wall_duration {
			r2s.Stat.MaxBRRWallDuration.duration = wall_duration
		}
		r2s.mux.Unlock()

		r2s.mux.Lock()
		if blob_size > r2s.Stat.MaxBRRBlobSize {
			r2s.Stat.MaxBRRBlobSize = blob_size
		}
		r2s.mux.Unlock()
	}

	if bs.wait(1) == 1 {
		ERROR("no roll blob: %s", r2s.RollBlob)
		leave(1)
	}

	//  cheap sanity tests
	r2s.mux.Lock()
	if r2s.Stat.MinBRRStartTime.After(r2s.Stat.MaxBRRStartTime) {
		_die("min_brr_start_time > max_brr_start_time")
	}
	r2s.mux.Unlock()

	done <- interface{}(nil)
}

func open_stream(blob, what string) *blob_stream {

	_die := func(format string, args ...interface{}) {
		die(fmt.Sprintf(
			"open_stream: " + what + ": " + format,
			args...,
		))
	}

	cmd := exec.Command(
			"blobio",
			"get",
			"--udig",
			blob,
			"--service",
			r2s.BLOBIO_SERVICE,
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
		what:	what,
	}
}

func (bs *blob_stream) wait(ok_exit int) (ex int) {

	_die := func(format string, args ...interface{}) {
		die("wait: " + bs.what + ": "  + format, args...)
	}

	cmd := bs.cmd

	ps, err := cmd.Process.Wait()
	if err != nil {
		_die("cmd.Process.Wait() failed: %s", err)
	}
	ex = ps.ExitCode()
	if ex > 0 && ex != ok_exit {
		_die("cmd.ProcessState.ExitCode > 0 or != %d: %d", ex, ok_exit)
	}
	return
}

func scan_roll(done chan interface{}) {

	_die := func(format string, args ...interface{}) {
		die("scan_roll: " + format, args...)
	}

	bs := open_stream(r2s.RollBlob, `scan_roll`)
	in := bs.out
	for in.Scan() {
		r2s.Stat.RollBRRCount++
		ud := in.Text()
		if !udig_re.MatchString(ud) {
			_die(
				"line %d: not udig in brr set: %s",
				r2s.Stat.RollBRRCount,
				r2s.RollBlob,
			)
		}
		go scan_brr_log(ud, done)
	}

	if bs.wait(1) == 1 {
		ERROR("no roll blob: %s", r2s.RollBlob)
		leave(1)
	}
}

func sanity() {
	//  cheap sanity checks
	if r2s.Stat.BRRCount < r2s.Stat.UDigCount {
		die("brr BRRCount < UDigCount")
	}
	r2s.Stat.UDigCount = uint64(len(r2s.udig_set))

	verb_count := r2s.Stat.GetOkCount + r2s.Stat.GetNoCount +
	              r2s.Stat.PutOkCount + r2s.Stat.PutNoCount +
	              r2s.Stat.GiveOkCount + r2s.Stat.GiveNoCount +
	              r2s.Stat.TakeOkCount + r2s.Stat.TakeNoCount +
	              r2s.Stat.WrapOkCount + r2s.Stat.WrapNoCount +
	              r2s.Stat.RollOkCount + r2s.Stat.RollNoCount +
		      r2s.Stat.EatOkCount + r2s.Stat.EatNoCount
	if verb_count != r2s.Stat.BRRCount {
		die("verb_count != brr_count")
	}
}

func main() {

	info("hello, world")
	info("BLOBIO_SERVICE=%s", r2s.BLOBIO_SERVICE)
	info("roll blob: %s", r2s.RollBlob)
	info("PATH=%s", os.Getenv("PATH"))

	done := make(chan interface{})
	scan_roll(done)

	//
	//  wait for the scanners of each blob request log in the roll set
	//
	//  Note:
	//	invoke a blob stream process for each brr log in the set.
	//	that could be problematic for large roll sets.  probably
	//	ought to limit to runtime.NumCPU()
	//
	for i := uint64(0);  i < r2s.Stat.RollBRRCount;  i++ {
		<- done
	}

	sanity()

	//  write json version of stat to standard output
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "\t")
	enc.Encode(r2s)

	leave(0)
}
