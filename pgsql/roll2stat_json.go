/*
 *  Synopsis:
 *	Summarize a roll of blob request records as json blob.
 *  Usage:
 *	roll2stat_json <roll-set-udig>
 *	roll2stat_json --verbose <roll-set-udig>
 *  Note:
 *	Think about "space" field.
 */

package main;

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"time"
)

var udig_re = regexp.MustCompile("^[a-z][a-z0-9]{0,7}:[[:graph:]]{32,128}$")
var verbose bool

type stats struct {
	WrapSetCount		uint64		`json:"wrap_set_count"`
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
	Stats			stats		`json:"roll2stat.blob.io"`

	WorkDir			string		`json:"work_dir"`
}
var r2s *roll2stat

func ERROR(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...)
}

func leave(exit_status int) {

	if  r2s.WorkDir != "" {
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
	leave(1)
}

func info(format string, args ...interface{}) {
	if verbose {
		fmt.Fprintf(os.Stderr, format + "\n", args...)
	}
}

func init() {
	//  parse command line arguments
	if len(os.Args) != 2 && len(os.Args) != 3 {
		die(
			"wrong number of arguments: got %d, expected 1 or 2",
			len(os.Args) - 1,
		)
	}

	//  extract roll_udig as first argument.

	if !udig_re.MatchString(os.Args[1]) {
		die("roll udig not a udig: %s", os.Args[1])
	}
	if len(os.Args) == 3 {
		if os.Args[2] != "--verbose" {
			die("unknown option: %s", os.Args[2])
		}
		if verbose {
			die("option --verbose: called more than once")
		}
		verbose = true
	}
	if os.Getenv("BLOBIO_SERVICE") == "" {
		die("env not defined: BLOBIO_SERVICE")
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

func scan_roll_blob() {
	cmd := exec.Command(
		"blobio",
		"get",
		"--udig",
		r2s.RollBlob,
		"--service",
		os.Getenv("BLOBIO_SERVICE"),
		"--output-path",
		r2s.RollBlob + ".blob",
	)
	err := cmd.Run()
	if err != nil {
		die("scan_roll_blob: blobio get failed: %s", err)
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

	scan_roll_blob()

	//  write json version of stats to standard output
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "\t")
	enc.Encode(r2s)

	leave(0)
}
