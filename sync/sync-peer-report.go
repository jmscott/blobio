/*
 *  Synopsis:
 *	Summarize blobs missing across a set of peer blobio servers.
 */

package main

import (
	"fmt"
	"os"
)

const usage = "usage: sync-peer-report <config-file-path>\n"

func die(format string, args ...interface{}) {

	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...);
	os.Stderr.Write([]byte(usage))
	os.Exit(1)
}

func main() {

	if (len(os.Args) - 1 != 1) {
		die(
			"wrong number of arguments: got %d, expected 1",
			len(os.Args) - 1,
		)
	}
}
