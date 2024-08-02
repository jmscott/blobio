//Synopsis:
//	Flow Detail Record

package main

import (
	. "time"
)

type fdr struct {
	//  timestamp earliest time the flow started
	start_time Time

	//  blob beinging flowed over
	udig string

	//  count of termination class "OK" in all rules that fired in flow
	ok_count uint8

	//  count of termination class not "OK" in all rules that fired in flow
	fault_count uint8

	//  wall clock elapsed time for firing of all rules
	wall_duration Duration

	//  server sequence of flow, sets to 0 when server boots
	sequence int64
}

const fdr_LOG_FORMAT = "%s\t%s\t%d\t%d\t%.9f\t%d\n"
