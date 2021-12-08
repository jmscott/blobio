/*
 *  Synopsis:
 *	Summarize a blob request "wrap" set in json.
 *  Usage:
 *	wrap2stat_json <wrap-set-udig>
 */

package main;

import (
	"fmt"
	"os"
	"regexp"
)

var udig_re = regexp.MustCompile("^[a-z][a-z0-9]{0,7}:[[:graph:]]{32,128}$")
var verbose bool
var wrap_udig string
var work_dir string

func ERROR(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "ERROR: " + format, args...)
}

func leave(exit_status int) {

	if work_dir != "" {
		err := os.Chdir(os.TempDir())
		if err == nil {
			err := os.RemoveAll(work_dir)
			if err != nil {
				ERROR("os.RemoveAll(work_dir) failed: %s", err)
				exit_status = 1
			}
		} else {
			ERROR("os.Chdir(TempDir) failed: %s", err)
			exit_status = 1
		}
	}
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

	//  extract wrap_udig as first argument.

	wrap_udig = os.Args[1]
	if !udig_re.MatchString(wrap_udig) {
		die("wrap udig not a udig: %s", wrap_udig)
	}
	if len(os.Args) == 3 {
		if os.Args[2] != "--verbose" {
			die("unknown option: %s", os.Args[2])
		}
		verbose = true
	}

	//  setup and go to work directory
	work_dir = fmt.Sprintf(
			"%s/wrap2stat_json-%d.d",
			os.TempDir(),
			os.Getpid(),
		)
	info("work dir: %s", work_dir)
}

func main() {
	
	if work_dir != "" {
		
	}
	leave(0)
}
