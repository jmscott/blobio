//Synopsis:
//	Query Detail Record

package main

import (
	. "time"
)

type qdr_field uint8

const (
	//  field offsets into query detail record (qdr)
	qdr_START_TIME = iota
	qdr_FLOW_SEQUENCE
	qdr_QUERY_NAME
	qdr_TERMINATION_CLASS
	qdr_UDIG
	qdr_SQLSTATE
	qdr_ROWS_AFFECTED
	qdr_WALL_DURATION
	qdr_QUERY_DURATION
)

type qdr struct {
	start_time        Time
	flow_sequence     uint64
	query_name        string
	termination_class string
	udig              string
	sqlstate          string
	rows_affected     int64
	wall_duration     Duration
	query_duration    Duration
}

func (qf qdr_field) String() string {

	switch qf {
	case qdr_START_TIME:
		return "start_time"
	case qdr_FLOW_SEQUENCE:
		return "flow_sequence"
	case qdr_QUERY_NAME:
		return "query_name"
	case qdr_TERMINATION_CLASS:
		return "termination_class"
	case qdr_UDIG:
		return "udig"
	case qdr_SQLSTATE:
		return "sqlstate"
	case qdr_ROWS_AFFECTED:
		return "rows_affected"
	case qdr_WALL_DURATION:
		return "wall_duration"
	case qdr_QUERY_DURATION:
		return "query_duration"
	default:
		panic("impossible qdr field value")
	}
}

//  Note: needs to be YYYY-MM-DD!
const qdr_LOG_FORMAT = "%s\t%d\t%s\t%s\t%s\t%s\t%d\t%.9f\t%.9f\n"
