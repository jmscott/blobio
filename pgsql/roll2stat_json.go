/*
 *  Synopsis:
 *	Summarize a roll of blob request records as json blob.
 *  Usage:
 *	roll2stat_json <roll-set-udig>
 *	roll2stat_json --verbose <roll-set-udig>
 */

package main;

import (
	"fmt"
	"os"
	"regexp"
	"encoding/json"
)

var udig_re = regexp.MustCompile("^[a-z][a-z0-9]{0,7}:[[:graph:]]{32,128}$")
var verbose bool

type roll2stat struct {
	Argc		int		`json:"argc"`
	Argv		[]string 	`json:"argv"`
	Env		[]string	`json:"environment"`
	BLOBIO_SERVICE	string

	RollBlob	string		`json:"roll_blob"`
	WorkDir		string		`json:"work_dir"`
}
var r2s *roll2stat

func ERROR(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "ERROR: " + format, args...)
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
		fmt.Fprintf(os.Stderr, format, args...)
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
		verbose = true
	}
	if os.Getenv("BLOBIO_SERVICE") == "" {
		die("env not defined: BLOBIO_SERVICE")
	}
}

func main() {

	info("hello, world")

	r2s = &roll2stat{
			Argc:		len(os.Args),
			Argv:		os.Args,
			Env:		os.Environ(),
			BLOBIO_SERVICE:	os.Getenv("BLOBIO_SERVICE"),
			RollBlob:	os.Args[1],
		  }
	//  go to temporary work directory
	r2s.WorkDir = fmt.Sprintf(
				"%s/roll2stat_json-%d.d",
				os.TempDir(),
				os.Getpid(),
	)
	info("work dir: %s", r2s.WorkDir)

	//  write json version of stats to standard output
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(false)
	enc.SetIndent("", "\t")
	enc.Encode(r2s)

	leave(0)
}
