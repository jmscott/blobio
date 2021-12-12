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
 *	Think about a per verb unique udig count
 *
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
var SERVICE_re = regexp.MustCompile(`^[a-z][a-z0-9]{0,7}:[[:graph:]]{1,128}$`)

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
var BLOBIO_SERVICE string

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


type stat struct {
	BRRLogCount		uint64		`json:"brr_log_count"`
	BRRCount		uint64		`json:"brr_count"`

	PrevRollUDig		string		`json:"prev_roll_udig"`
	PrevRollStartTime	time.Time	`json:"prev_roll_start_time"`

	PrevRollWallDuration	brr_duration	`json:"prev_roll_wall_time"`

	UDigCount		uint64		`json:"udig_count"`

	MinBRRStartTime		time.Time	`json:"min_brr_start_time"`
	MaxBRRStartTime		time.Time	`json:"max_brr_start_time"`


	MaxBRRWallDuration	brr_duration	`json:"max_brr_wall_duration"`
	MaxBRRBlobSize		uint64		`json:"max_brr_blob_size"`
	SumBRRBlobSize		uint64		`json:"sum_brr_blob_size"`

	EatOkCount		uint64		`json:"eat_ok_count"`
	EatNoCount		uint64		`json:"eat_no_count"`

	GetCount		uint64		`json:"get_count"`
	GetByteCount		uint64		`json:"get_byte_count"`

	TakeCount		uint64		`json:"take_count"`
	TakeByteCount		uint64		`json:"take_byte_count"`

	PutCount		uint64		`json:"put_count"`
	PutByteCount		uint64		`json:"put_byte_count"`

	GiveCount		uint64		`json:"give_count"`
	GiveByteCount		uint64		`json:"give_byte_count"`

	WrapCount		uint64		`json:"wrap_count"`

	RollCount		uint64		`json:"roll_count"`
	RollOkCount		uint64		`json:"roll_ok_count"`
	RollNoCount		uint64		`json:"roll_no_count"`

	OkCount			uint64		`json:"ok_count"`
	NoCount			uint64		`json:"no_count"`
}

type roll2stat struct {
	Argc			int		`json:"argc"`
	Argv			[]string 	`json:"argv"`
	Env			[]string	`json:"environment"`

	RollBlob		string		`json:"roll_blob"`
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

func scan_brr_log(brr_log string, done chan interface{}) {

	lino := uint64(0)

	_die := func(format string, args ...interface{}) {
		fmt := fmt.Sprintf("scan_brr_log: line %d: %s", lino, format)
		die(fmt, args...)
	}

	_bdie := func(what, fld string) {
		_die("brr: not a " + what + ": %s", fld)
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
				panic(
					"impossible eat chat history: " +
					chat_history,
				)
			}
		case "get":
			if chat_history != "no" && chat_history != "ok" {
				die(
					"get: impossible chat history: %s",
					chat_history,
				)
			}
			r2s.Stat.GetCount++
			r2s.Stat.GetByteCount += blob_size
		case "put":
			switch chat_history {
			case "ok,ok":
			case "no":
			case "ok,no":
			default:
				panic(fmt.Sprintf(
					"put: impossible chat_history: %s",
					chat_history,
				))
			}
			r2s.Stat.PutCount++
			r2s.Stat.PutByteCount += blob_size
		case "give":
			switch chat_history {
			case "ok,ok,ok":
			case "ok,no":
			case "ok,ok,no":
			case "no":
			default:
				panic(fmt.Sprintf(
					"give: impossible chat_history: %s",
					chat_history,
				))
			}
			r2s.Stat.GiveCount++
			r2s.Stat.GiveByteCount += blob_size
		case "take":
			switch chat_history {
			case "ok,ok,ok":
			case "ok,no":
			case "ok,ok,no":
			case "no":
			default:
				panic(fmt.Sprintf(
					"take: impossible chat_history: %s",
					chat_history,
				))
			}
			r2s.Stat.TakeCount++
			r2s.Stat.TakeByteCount += blob_size
		case "roll":
			if r2s.Stat.PrevRollUDig != "" {
				die("multiple roll udig seen")
			}
			r2s.Stat.PrevRollUDig = fld[5]
			r2s.Stat.PrevRollStartTime = start_time
			r2s.Stat.PrevRollWallDuration.duration = wall_duration
			switch chat_history {
			case "ok":
				r2s.Stat.RollOkCount++
			case "no":
				r2s.Stat.RollNoCount++
			default:
				panic("impossible chat_history: " +chat_history)
			}
		case "wrap":
			r2s.Stat.WrapCount++
		default:
			panic(fmt.Sprintf("impossible brr verb: %s", fld[2]))

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
		r2s.Stat.SumBRRBlobSize += blob_size
		r2s.mux.Unlock()

		r2s.mux.Lock()
		if strings.HasPrefix(chat_history, "ok") {
			r2s.Stat.OkCount++
		} else if strings.HasPrefix(chat_history, "no") {
			r2s.Stat.NoCount++
		} else {
			panic("impossible chat_history: " + chat_history)
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
		what:	what,
	}
}

func (bs *blob_stream) wait(ok_exit int) (ex int) {

	cmd := bs.cmd

	ps, err := cmd.Process.Wait()
	if err != nil {
		die("cmd.Process.Wait() failed: %s", err)
	}
	ex = ps.ExitCode()
	if ex > 0 && ex != ok_exit {
		die(bs.what + ": cmd.ProcessState.ExitCode > 0")
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

	if bs.wait(1) == 1 {
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
			udig_set:	make(map[string]bool),
		  }
	r2s.RollBlob = os.Args[1]
	info("roll blob: %s", r2s.RollBlob)
	info("BLOBIO_SERVICE=%s", os.Getenv("BLOBIO_SERVICE"))

	done := make(chan interface{})
	scan_roll(done)

	for i := uint64(0);  i < r2s.Stat.BRRLogCount;  i++ {
		<- done
	}

	//  cheap sanity checks
	if r2s.Stat.BRRCount < r2s.Stat.UDigCount {
		die("brr BRRCount < UDigCount")
	}
	if r2s.Stat.MaxBRRBlobSize > r2s.Stat.SumBRRBlobSize {
		die("brr MaxBRRBlobSize > SumBRRBlobSize")
	}
	r2s.Stat.UDigCount = uint64(len(r2s.udig_set))

	//  write json version of stat to standard output
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "\t")
	enc.Encode(r2s)

	leave(0)
}
