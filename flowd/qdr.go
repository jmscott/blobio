//Synopsis:
//	Query Detail Record

package main

import (
	. "time"
)

type qdr struct {
	start_time        Time
	flow_sequence     int64
	query_name        string
	termination_class string
	udig              string
	sqlstate          string
	rows_affected     int64
	wall_duration     Duration
	query_duration    Duration
}

//  Note: needs to be YYYY-MM-DD!
const qdr_LOG_FORMAT = "%s\t%d\t%s\t%s\t%s\t%s\t%d\t%.9f\t%.9f\n"
