//Synopsis:
//	Process Executation Detail Record
//Note:
//	Think about adding byte count for std{in,out,err} to xdr records.
//	Not sure what such byte counts would mean for a long running
//	process.

package main

import (
	. "fmt"

	. "time"
)

type xdr struct {
	start_time        Time
	flow_sequence     int64
	call_name         string
	exit_class	  string
	udig              string
	exit_status	  uint8
	wall_duration     Duration
	system_duration   Duration
	user_duration     Duration
}

const xdr_LOG_FORMAT = "%s\t%d\t%s\t%s\t%s\t%d\t%.9f\t%.9f\t%.9f\n"

func (xdr *xdr) debug() string {
	if xdr == nil {
		return "xdr(nil)"
	}
	if xdr.exit_class == "OK" {
		return Sprintf("%s(OK)", xdr.call_name)
	}
	return Sprintf("%s(%s:%d)",
		xdr.call_name,
		xdr.exit_class,
		xdr.exit_status,
	)
}
