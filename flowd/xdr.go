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

const (
	//  field offsets into exec detail record (xdr)
	xDR_START_TIME = iota
	xDR_FLOW_SEQUENCE
	xDR_CALL_NAME
	xDR_TERMINATION_CLASS
	xDR_UDIG
	xDR_TERMINATION_CODE
	xDR_WALL_DURATION
	xDR_SYSTEM_DURATION
	xDR_USER_DURATION
)

type xdr struct {
	start_time        Time
	flow_sequence     uint64
	call_name         string
	termination_class string
	udig              string
	termination_code  uint64
	wall_duration     Duration
	system_duration   Duration
	user_duration     Duration
}

const xdr_LOG_FORMAT = "%s\t%d\t%s\t%s\t%s\t%d\t%.9f\t%.9f\t%.9f\n"

func (xdr *xdr) debug() string {
	if xdr == nil {
		return "xdr(nil)"
	}
	if xdr.termination_class == "OK" {
		return Sprintf("%s(OK)", xdr.call_name)
	}
	return Sprintf("%s(%s:%d)",
		xdr.call_name,
		xdr.termination_class,
		xdr.termination_code,
	)
}
