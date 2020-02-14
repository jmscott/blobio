//Synopsis:
//	OpCodes for flow engine
//  See:
//
//	For rumsfeld's source see:
//
//		https://en.m.wikipedia.org/wiki/Four_stages_of_competence

package main

import (
	"database/sql"
	"regexp"
	"strconv"
	"sync"
	"unicode/utf8"
	"encoding/hex"

	. "fmt"
	. "time"
)

//  A rummy records for temporal + tri-state logic: true, false, null, waiting
//  There are known knowns, there are known unknowns ...

type rummy uint8

const (
	//  end of stream
	rum_NIL = rummy(0)

	//  will eventually resolve to true, false or null
	rum_WAIT = rummy(0x1)

	//  known to be null in the sql sense
	rum_NULL = rummy(0x2)

	//  known to be false
	rum_FALSE = rummy(0x4)

	//  known to be true
	rum_TRUE = rummy(0x8)
)

//  state tables for logical and/or
var and = [137]rummy{}
var or = [137]rummy{}

func init() {

	//  some shifted constants for left hand bits

	const lw = rummy(rum_WAIT << 4)
	const ln = rummy(rum_NULL << 4)
	const lf = rummy(rum_FALSE << 4)
	const lt = rummy(rum_TRUE << 4)

	//  PostgrSQL logical 'and' semantics for discovered values,
	//  applied sequentially
	//
	//  false and *	=>	false
	//  * and false	=>	false
	//  null and *	=>	null
	//  * and null	=>	null
	//  *		=>	true

	//  left value is false

	and[lf|rum_WAIT] = rum_FALSE
	and[lf|rum_NULL] = rum_FALSE
	and[lf|rum_FALSE] = rum_FALSE
	and[lf|rum_TRUE] = rum_FALSE

	//  right value is false

	and[lw|rum_FALSE] = rum_FALSE
	and[ln|rum_FALSE] = rum_FALSE
	and[lt|rum_FALSE] = rum_FALSE
	and[lf|rum_FALSE] = rum_FALSE

	//  left value is true

	and[lt|rum_WAIT] = rum_WAIT
	and[lt|rum_NULL] = rum_NULL
	and[lt|rum_FALSE] = rum_FALSE
	and[lt|rum_TRUE] = rum_TRUE

	//  right value is true

	and[lw|rum_TRUE] = rum_WAIT
	and[ln|rum_TRUE] = rum_NULL
	and[lt|rum_TRUE] = rum_TRUE
	and[lf|rum_TRUE] = rum_FALSE

	//  left value is null

	and[ln|rum_NULL] = rum_NULL
	and[ln|rum_TRUE] = rum_NULL
	and[ln|rum_FALSE] = rum_FALSE
	and[ln|rum_WAIT] = rum_WAIT

	//  right value is null

	and[lt|rum_NULL] = rum_NULL
	and[lf|rum_NULL] = rum_FALSE
	and[ln|rum_NULL] = rum_NULL
	and[lw|rum_NULL] = rum_WAIT

	//  left value is waiting

	and[lw|rum_NULL] = rum_WAIT
	and[lw|rum_TRUE] = rum_WAIT
	and[lw|rum_FALSE] = rum_FALSE
	and[lw|rum_WAIT] = rum_WAIT

	//  right value is waiting

	and[lt|rum_WAIT] = rum_WAIT
	and[lf|rum_WAIT] = rum_FALSE
	and[ln|rum_WAIT] = rum_WAIT
	and[lw|rum_WAIT] = rum_WAIT

	//  PostgrSQL logical 'or' semantics for discovered values,
	//  applied sequentially
	//
	//  true or *	=>	true
	//  * or true	=>	true
	//  null or *	=>	null
	//  * or null	=>	null
	//  *		=>	false

	//  left value is true

	or[lt|rum_WAIT] = rum_TRUE
	or[lt|rum_NULL] = rum_TRUE
	or[lt|rum_FALSE] = rum_TRUE
	or[lt|rum_TRUE] = rum_TRUE

	//  right value is true

	or[lw|rum_TRUE] = rum_TRUE
	or[ln|rum_TRUE] = rum_TRUE
	or[lf|rum_TRUE] = rum_TRUE
	or[lt|rum_TRUE] = rum_TRUE

	//  left value is false

	or[lf|rum_WAIT] = rum_WAIT
	or[lf|rum_NULL] = rum_NULL
	or[lf|rum_FALSE] = rum_FALSE
	or[lf|rum_TRUE] = rum_TRUE

	//  right value is false

	or[lw|rum_FALSE] = rum_WAIT
	or[ln|rum_FALSE] = rum_NULL
	or[lf|rum_FALSE] = rum_FALSE
	or[lt|rum_FALSE] = rum_TRUE

	//  left value is null

	or[ln|rum_WAIT] = rum_WAIT
	or[ln|rum_NULL] = rum_NULL
	or[ln|rum_FALSE] = rum_NULL
	or[ln|rum_TRUE] = rum_TRUE

	//  right value is null

	or[ln|rum_NULL] = rum_NULL
	or[lt|rum_NULL] = rum_TRUE
	or[lf|rum_NULL] = rum_NULL
	or[lw|rum_NULL] = rum_WAIT

	//  left value is waiting

	or[lw|rum_WAIT] = rum_WAIT
	or[lw|rum_NULL] = rum_WAIT
	or[lw|rum_TRUE] = rum_TRUE
	or[lw|rum_FALSE] = rum_WAIT

	//  right value is waiting
	or[lw|rum_WAIT] = rum_WAIT
	or[lt|rum_WAIT] = rum_TRUE
	or[lf|rum_WAIT] = rum_WAIT
	or[ln|rum_WAIT] = rum_WAIT
}

// bool_value is result of AND, OR and relational operations
type bool_value struct {
	bool
	is_null bool

	*flow
}

func (bv *bool_value) String() string {
	if bv == nil {
		return "NIL"
	}
	if bv.is_null {
		return "<NULL>"
	}
	return Sprintf("%t", bv.bool)
}

// bool_chan is channel of *bool_values;  nil indicates closure of channel
type bool_chan chan *bool_value

// string_value is result or projection.  See project_brr()
type string_value struct {
	string
	is_null bool

	*flow
}

//  string_chan is channel of *string_values;  nil indicates closure
type string_chan chan *string_value

//  argv_value represents a function or query string argument vector
type argv_value struct {
	argv    []string
	is_null bool

	*flow
}

//  argv_chan is channel of *argv_values;  nil indicates closure
type argv_chan chan *argv_value

//  xdr_value represents a process execution description record.
//  the field output_4095 may be null if no output occured during the processes
//  execution.  err is error related to a failed execution of the process
type xdr_value struct {
	//  Note: a pointer to xdr?
	*xdr
	is_null bool

	output_4095 []byte

	*flow
}
type xdr_chan chan *xdr_value

type qdr_value struct {
	//  Note: a pointer to qdr?
	*qdr
	is_null bool

	//  query result bound to this qdr record
	results []interface{}

	//  error from sql.QueryRow().

	err        error
	err_driver string

	*flow
}
type qdr_chan chan *qdr_value

type fdr_value struct {
	*fdr

	*flow
}
type fdr_chan chan *fdr_value

type uint64_value struct {
	uint64
	is_null bool

	*flow
}
type uint64_chan chan *uint64_value

//  a flow tracks the firing of rules over a single blob described in a
//  blob request record (brr).  See brr.go

type flow struct {

	//  request a new flow from this channel,
	//  reading reply on sent side-channel

	next chan flow_chan

	//  channel is closed when all call()/queries have been can make no
	//  further progress

	resolved chan struct{}

	//  first, second, ... n-th brr flow
	seq uint64

	//  the blob request record being "flowed"
	brr *brr

	//  count of go routines still flowing expressions
	confluent_count int
}

//  the river of blobs
type flow_chan chan *flow

func (rum rummy) String() string {

	//  describe rummy states
	var rum2string = [16]string{
		"NIL",
		"WAIT",
		"NULL",
		"3",
		"FALSE",
		"5", "6", "7",
		"TRUE",
		"9", "10", "11", "12", "13", "14", "15",
	}
	if rum < 16 {
		return rum2string[rum]
	}
	return rum2string[rum>>4] + "|" + rum2string[rum&0x0F]
}

func (bv *bool_value) rummy() rummy {

	switch {
	case bv == nil:
		return rum_WAIT
	case bv.is_null:
		return rum_NULL
	case bv.bool:
		return rum_TRUE
	}
	return rum_FALSE
}

//  Note:
//	why have a special get() function?  why not change the flow struct into
//	a channel?
//
//		for flow := range flow {
//			...
func (flo *flow) get() *flow {

	<-flo.resolved

	//  next active flow arrives on this channel
	reply := make(flow_chan)

	//  request another flow, sending reply channel to mother
	flo.next <- reply

	//  return next flow
	return <-reply
}

//  project a field in the brr of this flow
func (flo *flow) project_brr(field brr_field) (out string_chan) {

	out = make(string_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			out <- &string_value{
				string:  (*(flo.brr))[field],
				is_null: false,
				flow:    flo,
			}
		}
	}()
	return out
}

//  project the exit status of in an xdr record

func (flo *flow) project_xdr_exit_status(
	in xdr_chan,
) (out uint64_chan) {

	out = make(uint64_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {
			xv := <-in
			if xv == nil {
				return
			}

			var is_null bool
			var ui uint64

			if xv.is_null || xv.xdr == nil {
				is_null = true
			} else {
				ui = xv.xdr.termination_code
			}

			out <- &uint64_value{
				uint64:  ui,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()

	return out
}

//  project the boolean value bound of a qdr record.

func (flo *flow) project_sql_query_row_bool(
	in qdr_chan,
	field uint8,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			qv := <-in
			if qv == nil {
				return
			}

			var is_null bool
			var b bool

			if qv.qdr != nil && qv.qdr.rows_affected > 0 {
				br := *(qv.results[field]).(*sql.NullBool)
				if br.Valid {
					b = br.Bool
				} else {
					is_null = true
				}
			} else {
				is_null = true
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()

	return out
}

//  project the uint64 rows_affected value of a query detail record.

func (flo *flow) project_qdr_rows_affected(
	in qdr_chan,
) (out uint64_chan) {

	out = make(uint64_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			qv := <-in
			if qv == nil {
				return
			}

			var is_null bool
			var ui uint64

			if qv.is_null || qv.qdr == nil {
				is_null = true
			} else {
				ui = uint64(qv.qdr.rows_affected)
			}

			out <- &uint64_value{
				uint64:  ui,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()

	return out
}

//  project the string 'sqlstate' value of a query detail record.

func (flo *flow) project_qdr_sqlstate(
	in qdr_chan,
) (out string_chan) {

	out = make(string_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			qv := <-in
			if qv == nil {
				return
			}

			var is_null bool
			var s string

			if qv.is_null || qv.qdr == nil {
				is_null = true
			} else {
				s = qv.qdr.sqlstate
			}

			out <- &string_value{
				string:  s,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()

	return out
}

func (flo *flow) eq_string(
	s_const string,
	in string_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}

			var b, is_null bool

			if sv.is_null {
				is_null = true
			} else {
				b = (s_const == sv.string)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}

		}
	}()
	return out
}

func (flo *flow) match_string(
	rex_const *regexp.Regexp,
	in string_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}

			var b, is_null bool

			if sv.is_null {
				is_null = true
			} else {
				b = rex_const.MatchString(sv.string)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) no_match_string(
	rex_const *regexp.Regexp,
	in string_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}

			var b, is_null bool

			if sv.is_null {
				is_null = true
			} else {
				b = rex_const.MatchString(sv.string) == false
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) eq_bool(
	b_const bool,
	in bool_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			bv := <-in
			if bv == nil {
				return
			}

			var b, is_null bool

			if bv.is_null {
				is_null = true
			} else {
				b = (b_const == bv.bool)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

//  cast string to uint64, panicing upon error

func (flo *flow) cast_uint64(in string_chan) (out uint64_chan) {

	out = make(uint64_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}

			var is_null bool
			var ui64 uint64

			if sv.is_null {
				is_null = true
			} else {
				var err error
				ui64, err = strconv.ParseUint(sv.string, 10, 64)
				if err != nil {
					panic(err)
				}
			}
			out <- &uint64_value{
				uint64:  ui64,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) neq_string(
	string string,
	in string_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}

			var b, is_null bool

			if sv.is_null {
				is_null = true
			} else {
				b = (string != sv.string)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) eq_uint64(
	ui64 uint64,
	in uint64_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			uv := <-in
			if uv == nil {
				return
			}

			var b, is_null bool

			if uv.is_null {
				is_null = true
			} else {
				b = (ui64 == uv.uint64)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) neq_uint64(
	ui64 uint64,
	in uint64_chan,
) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			uv := <-in
			if uv == nil {
				return
			}

			var b, is_null bool

			if uv.is_null {
				is_null = true
			} else {
				b = (ui64 != uv.uint64)
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

func (flo *flow) wait_bool2(
	op [137]rummy,
	in_left, in_right bool_chan,
) (
	next rummy,
) {
	var lv, rv *bool_value

	next = rum_WAIT
	for next == rum_WAIT {

		select {
		case l := <-in_left:
			if l == nil {
				return rum_NIL
			}

			// cheap sanity test.  will go away soon
			if lv != nil {
				panic("left hand value out of sync")
			}
			lv = l

		case r := <-in_right:
			if r == nil {
				return rum_NIL
			}

			// cheap sanity test.  will go away soon
			if rv != nil {
				panic("right hand value out of sync")
			}
			rv = r
		}
		next = op[(lv.rummy()<<4)|rv.rummy()]
	}

	//  drain unread channel.
	//
	//  Note: reading in the background causes a mutiple read of
	//        same left/right hand side.  Why?  Shouldn't the flow
	//	  block on current sequence until all qualfications converge?

	if lv == nil {
		<-in_left
	} else if rv == nil {
		<-in_right
	}
	return next
}

/*
 *  Apply PostgreSQL 9.3 SQL logical semantics to two bool streams.
 */
func (flo *flow) bool2(
	op [137]rummy,
	in_left, in_right bool_chan,
) (out bool_chan) {

	out = make(bool_chan)

	//  logical bool binary operator

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			var b, is_null bool

			//  Note: op can go away
			rum := flo.wait_bool2(op, in_left, in_right)
			switch rum {
			case rum_NIL:
				return
			case rum_NULL:
				is_null = true
			case rum_TRUE:
				b = true
			}
			out <- &bool_value{
				bool:    b,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

//  empty non-null argv sends immediatly

func (flo *flow) argv0() (out argv_chan) {

	out = make(argv_chan)

	go func() {
		defer close(out)

		var argv [0]string

		for flo = flo.get(); flo != nil; flo = flo.get() {

			out <- &argv_value{
				is_null: false,
				argv:    argv[:],
				flow:    flo,
			}
		}
	}()
	return out
}

//  direct answer of argv with single string

func (flo *flow) argv1(in string_chan) (out argv_chan) {

	out = make(argv_chan)

	go func() {
		defer close(out)

		var argv [1]string

		for flo = flo.get(); flo != nil; flo = flo.get() {

			sv := <-in
			if sv == nil {
				return
			}
			argv[0] = sv.string
			out <- &argv_value{
				is_null: sv.is_null,
				argv:    argv[:],
				flow:    flo,
			}
		}
	}()
	return out
}

//  read strings from multiple input channels and write assmbled argv[]
//  any null value renders the whole argv[] null

func (flo *flow) argv(in_args []string_chan) (out argv_chan) {

	//  track a received string and position in argv[]
	type arg_value struct {
		*string_value
		position uint8
	}

	out = make(argv_chan)
	argc := uint8(len(in_args))

	//  called func has arguments, so wait on multple string channels
	//  before sending assembled argv[]

	go func() {

		defer close(out)

		//  merge() many string channels onto a single channel of
		//  argument values.

		merge := func() (mout chan arg_value) {

			var wg sync.WaitGroup
			mout = make(chan arg_value)

			io := func(sc string_chan, p uint8) {
				for sv := range sc {
					mout <- arg_value{
						string_value: sv,
						position:     p,
					}
				}
				wg.Done()
			}

			wg.Add(len(in_args))
			for i, sc := range in_args {
				go io(sc, uint8(i))
			}

			//  Start a goroutine to close 'mout' channel
			//  once all the output goroutines are done.

			go func() {
				wg.Wait()
				close(mout)
			}()
			return
		}()

		for flo = flo.get(); flo != nil; flo = flo.get() {

			av := make([]string, argc)
			ac := uint8(0)
			is_null := false

			//  read until we have an argv[] for which all elements
			//  are also non-null.  any null argv[] element makes
			//  the whole argv[] null

			for ac < argc {

				a := <-merge

				//  Note: compile generates error for
				//        arg_value{}

				if a == (arg_value{}) {
					return
				}

				sv := a.string_value
				pos := a.position

				//  any null element forces entire argv[]
				//  to be null

				if a.is_null {
					is_null = true
				}

				//  cheap sanity test tp insure we don't
				//  see the same argument twice
				//
				//  Note:
				//	technically this implies an empty
				//	string is not allowed which is probably
				//	unreasonable

				if av[pos] != "" {
					panic("argv[] element not \"\"")
				}
				av[pos] = sv.string
				ac++
			}

			//  feed the hungry world our new, boundless argv[]
			out <- &argv_value{
				argv:    av,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()

	return out
}

//  send a string constant

func (flo *flow) const_string(s string) (out string_chan) {

	out = make(string_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {
			out <- &string_value{
				string:  s,
				is_null: false,
				flow:    flo,
			}
		}
	}()

	return out
}

func (flo *flow) const_bool(b bool) (out bool_chan) {

	out = make(bool_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {
			out <- &bool_value{
				bool:    b,
				is_null: false,
				flow:    flo,
			}
		}
	}()

	return out
}

//  send a uint64 constant

func (flo *flow) const_uint64(ui64 uint64) (out uint64_chan) {

	out = make(uint64_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			out <- &uint64_value{
				uint64:  ui64,
				is_null: false,
				flow:    flo,
			}
		}
	}()

	return out
}

func (flo *flow) call(
	cmd *command,
	osx_q os_exec_chan,
	in_argv argv_chan,
	in_when bool_chan,
) (out xdr_chan) {

	out = make(xdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			argv, when := flo.wait_fire(in_argv, in_when)
			if argv == nil {
				return
			}

			//  don't fire command if either argv is null or
			//  when is false or null

			var xv *xdr_value

			switch {

			//  xdr is null when either argv or when is null

			case argv.is_null || when.is_null:
				xv = &xdr_value{
					is_null: true,
					flow:    flo,
				}

			//  when is true and argv exists, so fire command

			case when.bool:
				//  synchronous call to command{} process
				//  always returns partially built xdr value

				start_time := Now()
				xv = cmd.call(argv.argv, osx_q)
				xv.start_time = start_time
				xv.wall_duration = Since(start_time)
				xv.flow = flo
				xv.udig = flo.brr[brr_UDIG]
				xv.flow_sequence = flo.seq

			//  when clause is false.
			//
			//  since resolution requires that an xdr be generated
			//  inside a flow, we send an xdr_value that is both
			//  non null but the xdr_value.xdr field is nil.
			//
			//  ugly.

			default:
				xv = &xdr_value{
					is_null: false,
					flow:    flo,
				}
			}

			//  the flow never resolves until all call()s
			//  generate an xdr record.

			out <- xv
		}
	}()
	return out
}

//  fire an xdr record for an optimized command that always has an exit
//  status of 0.

func (flo *flow) call0(
	name string,
	in_argv argv_chan,
	in_when bool_chan,
) (out xdr_chan) {

	out = make(xdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			argv, when := flo.wait_fire(in_argv, in_when)
			if argv == nil {
				return
			}

			var xv *xdr_value

			switch {

			//  xdr is null when either argv or when is null

			case argv.is_null || when.is_null:
				xv = &xdr_value{
					is_null: true,
					flow:    flo,
				}

			//  when is rule optimized "true" which does not invoke
			//  a process

			case when.bool:
				udig := flo.brr[brr_UDIG]
				xv = &xdr_value{
					xdr: &xdr{
						start_time:        Now(),
						call_name:         name,
						udig:              udig,
						flow_sequence:     flo.seq,
						termination_class: "OK",
						termination_code:  0,
					},
					flow: flo,
				}
				xv.wall_duration = Since(xv.start_time)

			//  when clause is false.
			//
			//  since resolution requires that an xdr be generated
			//  inside a flow, we send an xdr_value that is both
			//  non null but the xdr_value.xdr field is nil.
			//
			//  ugly.

			default:
				xv = &xdr_value{
					is_null: false,
					flow:    flo,
				}
			}

			//  the flow never resolves until all call()s
			//  generate an xdr record.

			out <- xv
		}
	}()
	return out
}

func (flo *flow) log_xdr_error(
	log_ch file_byte_chan,
	in xdr_chan,
) (out xdr_chan) {

	out = make(xdr_chan)

	go func() {
		defer close(out)

		who := func(xdr *xdr) string {

			return Sprintf("%s: flow #%d: %s",
				xdr.call_name,
				xdr.flow_sequence,
				xdr.udig,
			)
		}

		for xv := range in {

			if xv.xdr != nil && xv.xdr.termination_class != "OK" {
				log_ch.ERROR("%s: termination class: %s",
					who(xv.xdr),
					xv.xdr.termination_class,
				)
				log_ch.ERROR("%s: termination code: %d",
					who(xv.xdr),
					xv.xdr.termination_code,
				)
				//  only OK or ERR are valid xdr
				if xv.termination_class != "ERR" {
					xv.xdr = nil
				}
			}

			//  burp out process output to log file

			//  Note: why is xv.xdr ever nil!!!

			if xv.xdr != nil && xv.output_4095 != nil {
				who := who(xv.xdr)

				//  the output is framed with BEGIN: and END:
				//  for easy searching in log file

				BEGIN := ([]byte(
						"\nBEGIN OUTPUT: " +
						who +
						"\n",
				))[:]
				END := ([]byte(
						"\nEND OUTPUT: " + 
						who +
						"\n",
				))[:]

				msg := append([]byte(nil), BEGIN...)

				encoding := "utf8"

				if utf8.Valid(xv.output_4095) {
					msg = append(msg, xv.output_4095[:]...)
				} else {
					encoding = "hexdump"

					//  encode the first 32 bytes.
					//  replace with human readable
					//  hexdumper!

					src := xv.output_4095
					if (len(src) > 32) {
						src = src[:32]
					}
					dst := make([]byte, hex.EncodedLen(32))
					hex.Encode(dst, src)
					msg = append(msg, dst[:]...)
				}
				log_ch.ERROR(
					"%s: output: %s: %d bytes",
					who,
					encoding,
					len(xv.output_4095),
				)
				log_ch <- append(msg, END...)
			}
			out <- xv
		}
	}()

	return out
}

func (flo *flow) log_qdr_error(
	log_ch file_byte_chan,
	in qdr_chan,
) (out qdr_chan) {

	out = make(qdr_chan)

	go func() {
		defer close(out)

		who := func(qdr *qdr) string {

			return Sprintf("%s: flow #%d: %s",
				qdr.query_name,
				qdr.flow_sequence,
				qdr.udig,
			)
		}

		for qv := range in {

			if qv.qdr != nil && qv.qdr.termination_class != "OK" {

				//  Note:  need to distinguish ERROR/WARN

				log_ch.ERROR("%s: termination class: %s",
					who(qv.qdr),
					qv.qdr.termination_class,
				)
				log_ch.ERROR("%s: sqlstate: %s",
					who(qv.qdr),
					qv.qdr.sqlstate,
				)
			}
			if qv.err != nil {
				log_ch.ERROR("%s: %s", who(qv.qdr), qv.err)
			}

			out <- qv
		}
	}()

	return out
}

func (flo *flow) log_xdr(
	log_ch chan []byte,
	in xdr_chan,
) (out xdr_chan) {

	out = make(xdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			xv := <-in
			if xv == nil {
				return
			}

			if !xv.is_null && xv.xdr != nil {
				xdr := xv.xdr

				//  write the exec detail record down the pipe.

				log_ch <- []byte(
					Sprintf(xdr_LOG_FORMAT,
						xdr.start_time.Format(
							RFC3339Nano),
						xdr.flow_sequence,
						xdr.call_name,
						xdr.termination_class,
						xdr.udig,
						xdr.termination_code,
						xdr.wall_duration.Seconds(),
						xdr.system_duration.Seconds(),
						xdr.user_duration.Seconds(),
					))
			}
			out <- xv
		}
	}()

	return out
}

func (flow *flow) log_qdr(
	log_ch chan []byte,
	in qdr_chan,
) (out qdr_chan) {

	out = make(qdr_chan)

	go func() {
		defer close(out)

		for flow = flow.get(); flow != nil; flow = flow.get() {

			qv := <-in
			if qv == nil {
				return
			}

			if !qv.is_null && qv.qdr != nil {
				qdr := qv.qdr

				//  write the exec detail record down the pipe.
				log_ch <- []byte(
					Sprintf(qdr_LOG_FORMAT,
						qdr.start_time.Format(
							RFC3339Nano),
						qdr.flow_sequence,
						qdr.query_name,
						qdr.termination_class,
						qdr.udig,
						qdr.sqlstate,
						qdr.rows_affected,
						qdr.wall_duration.Seconds(),
						qdr.query_duration.Seconds(),
					))
			}
			out <- qv
		}
	}()

	return out
}

//  Reduce all the exec/query detail records into a single flow detail record.

func (flo *flow) reduce(inx []xdr_chan, inq []qdr_chan) (out fdr_chan) {

	out = make(fdr_chan)

	go func() {
		defer close(out)

		//  add xdr stats to fdr record
		xdr_stat := func(f *flow, fdr *fdr, xv *xdr_value) {

			xdr := xv.xdr

			//  null xdr indicates null when clause.
			//  nil xdr indicates false when clause

			switch {

			//  sanity check

			case xv.flow.seq < f.seq:
				panic("stale xdr")

			//  sanity test
			case xv.flow.seq > f.seq:
				panic("xdr from the future")

			//  when clause is null or false

			case xv.is_null:
			case xdr == nil:

			//  terminated with OK

			case xdr.termination_class == "OK":
				fdr.ok_count++

			//  terminated with ERR, SIG, or NOPS

			default:
				fdr.fault_count++
			}
		}

		//  add qdr stats to fdr record

		qdr_stat := func(f *flow, fdr *fdr, qv *qdr_value) {

			qdr := qv.qdr

			//  null qdr indicates null when clause.
			//  nil qdr indicates false when clause

			switch {

			//  sanity check

			case qv.flow.seq < f.seq:
				panic("stale qdr")

			//  sanity test
			//  Note: obsolete?

			case qv.flow.seq > f.seq:
				panic("qdr from the future")

			//  when clause is null or false

			case qv.is_null:
			case qdr == nil:

			//  terminated with OK

			case qdr.termination_class == "OK":
				fdr.ok_count++

			//  terminated with ERR, SIG, or NOPS

			default:
				fdr.fault_count++
			}
		}

		in_count := len(inx) + len(inq)

		//  merge many xdr channels onto a single xdr channel.

		xdr_merge := func() (merged xdr_chan) {

			var wg sync.WaitGroup
			merged = make(xdr_chan)

			io := func(in xdr_chan) {
				for xv := range in {
					merged <- xv
				}
				//  decrement active go routine count
				wg.Done()
			}
			wg.Add(len(inx))

			//  start merging
			for _, in := range inx {
				go io(in)
			}

			//  Start a goroutine to close 'merged' channel
			//  once all the output goroutines are done.

			go func() {
				wg.Wait()
				close(merged)
			}()
			return
		}()
		if len(inx) == 0 {
			xdr_merge = nil
		}

		//  merge many qdr channels onto a single channel.

		qdr_merge := func() (merged qdr_chan) {

			var wg sync.WaitGroup
			merged = make(qdr_chan)

			io := func(in qdr_chan) {
				for qv := range in {
					merged <- qv
				}
				wg.Done() //  decrement count
			}
			wg.Add(len(inq))

			//  start merging
			for _, in := range inq {
				go io(in)
			}

			//  Start a goroutine to close 'merged' channel
			//  once all the output goroutines are done.

			go func() {
				wg.Wait()
				close(merged)
			}()
			return
		}()
		if len(inq) == 0 {
			qdr_merge = nil
		}

		for flo = flo.get(); flo != nil; flo = flo.get() {

			fdr := &fdr{
				start_time: Now(),
				udig:       flo.brr[brr_UDIG],
				sequence:   flo.seq,
			}

			//  wait the xdr and qdr records

			for i := 0; i < in_count; i++ {

				select {

				case xv := <-xdr_merge:
					if xv == nil {
						return
					}
					if xv.flow.seq != flo.seq {
						panic("xv out of sync")
					}
					xdr_stat(flo, fdr, xv)

				case qv := <-qdr_merge:
					if qv == nil {
						return
					}
					if qv.flow.seq != flo.seq {
						panic("qv out of sync")
					}
					qdr_stat(flo, fdr, qv)
				}
			}
			fdr.wall_duration = Since(fdr.start_time)
			out <- &fdr_value{
				fdr:  fdr,
				flow: flo,
			}
		}
	}()
	return out
}

//  log the flow detail records

func (flo *flow) log_fdr(
	log_ch chan []byte,
	in fdr_chan,
) (out fdr_chan) {

	out = make(fdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			fdr := <-in
			if fdr == nil {
				return
			}

			//  log the flow detail record
			log_ch <- []byte(Sprintf(
				fdr_LOG_FORMAT,
				fdr.start_time.Format(RFC3339Nano),
				fdr.udig,
				fdr.ok_count,
				fdr.fault_count,
				fdr.wall_duration.Seconds(),
				fdr.sequence,
			))
			out <- fdr
		}
	}()
	return out
}

//  broadcast an xdr to many xdr channels
//
//  Note:
//	would be nice to randomize writes to the output channels

func (flo *flow) fanout_xdr(
	in xdr_chan,
	count uint8,
) (out []xdr_chan) {

	out = make([]xdr_chan, count)
	for i := uint8(0); i < count; i++ {
		out[i] = make(xdr_chan)
	}

	put := func(xv *xdr_value, xc xdr_chan) {

		xc <- xv
	}

	go func() {

		defer func() {
			for _, a := range out {
				close(a)
			}
		}()

		for flo = flo.get(); flo != nil; flo = flo.get() {

			xv := <-in
			if xv == nil {
				return
			}

			//  broadcast to channels in slice
			for _, xc := range out {
				go put(xv, xc)
			}
		}
	}()
	return out
}

//  broadcast a qdr to many qdr channels
//
//  Note:
//	would be nice to randomize writes to the output channels

func (flo *flow) fanout_qdr(
	in qdr_chan,
	count uint8,
) (out []qdr_chan) {

	out = make([]qdr_chan, count)
	for i := uint8(0); i < count; i++ {
		out[i] = make(qdr_chan)
	}

	put := func(qv *qdr_value, qc qdr_chan) {

		qc <- qv
	}

	go func() {

		defer func() {
			for _, a := range out {
				close(a)
			}
		}()

		for flo = flo.get(); flo != nil; flo = flo.get() {

			qv := <-in
			if qv == nil {
				return
			}

			//  broadcast to channels in slice
			//  Note: wait for the fanout to complete?
			for _, qc := range out {
				go put(qv, qc)
			}
		}
	}()
	return out
}

//  cast uint64 into string
func (flo *flow) cast_string(
	in uint64_chan,
) (out string_chan) {

	out = make(string_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			uv := <-in
			if uv == nil {
				return
			}

			var is_null bool
			var s string

			if uv.is_null {
				is_null = true
			} else {
				s = strconv.FormatUint(uv.uint64, 10)
			}
			out <- &string_value{
				string:  s,
				is_null: is_null,
				flow:    flo,
			}
		}
	}()
	return out
}

//  a rule fires if and only if both the argv[] exists and
//  the when clause is true.

func (flo *flow) wait_fire(
	in_argv argv_chan,
	in_when bool_chan,
) (
	argv *argv_value,
	when *bool_value,
) {

	//  wait for both an argv[] and resolution of the when clause
	for argv == nil || when == nil {
		select {
		case argv = <-in_argv:
			if argv == nil {
				return nil, nil
			}
			if argv.flow.seq != flo.seq {
				panic("argv flow out of sequence")
			}

		case when = <-in_when:
			if when == nil {
				return nil, nil
			}
			if when.flow.seq != flo.seq {
				panic("when clause flow out of sequence")
			}
		}
	}
	return
}

func (flo *flow) sql_query_row(
	q *sql_query_row,
	in_argv argv_chan,
	in_when bool_chan,
) (out qdr_chan) {

	out = make(qdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			argv, when := flo.wait_fire(in_argv, in_when)

			//  end of stream
			if argv == nil {
				return
			}

			//  both the argv and when channels have resolved,
			//  so determine if the query should fire.
			start_time := Now()

			var qv *qdr_value

			switch {

			//  argv or when is null, so qdr is null

			case argv.is_null || when.is_null:
				qv = &qdr_value{
					is_null: true,
					flow:    flo,
				}

			//  when predicate is true and argv exists, so
			//  fire query

			case when.bool:

				//  invoke sql query
				qv = q.query_row(argv.argv)

				qv.flow = flo
				qv.udig = flo.brr[brr_UDIG]
				qv.flow_sequence = flo.seq
				qv.query_name = q.name

			//  when clause is false.
			//
			//  since resolution requires that an qdr be generated
			//  inside a flow, we send an qdr_value that is both
			//  non null //  but the qdr_value.qdr field is nil.
			//
			//  ugly.
			//

			default:
				qv = &qdr_value{
					is_null: false,
					flow:    flo,
				}
			}
			if qv.qdr != nil {
				qv.start_time = start_time
				qv.wall_duration = Since(start_time)
			}

			//  the flow never resolves until all queries()s
			//  generate an qdr record.

			out <- qv
		}
	}()
	return out
}

func (flo *flow) sql_exec(
	ex *sql_exec,
	in_argv argv_chan,
	in_when bool_chan,
) (out qdr_chan) {

	out = make(qdr_chan)

	go func() {
		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			argv, when := flo.wait_fire(in_argv, in_when)
			if argv == nil {
				return
			}

			//  don't fire command if argv or when is null or
			//  when is false

			start_time := Now()

			var qv *qdr_value

			switch {

			//  argv or when is null, so qdr is null

			case argv.is_null || when.is_null:
				qv = &qdr_value{
					is_null: true,
					flow:    flo,
				}

			//  when predicate is true and argv exists, so
			//  fire query

			case when.bool:

				//  invoke sql query
				qv = ex.exec(argv.argv)

				qv.flow = flo
				qv.udig = flo.brr[brr_UDIG]
				qv.flow_sequence = flo.seq
				qv.query_name = ex.name

			//  when clause is false.
			//
			//  since resolution requires that an qdr be generated
			//  inside a flow, we send an qdr_value that is both
			//  non null //  but the qdr_value.qdr field is nil.
			//
			//  ugly.

			default:
				qv = &qdr_value{
					is_null: false,
					flow:    flo,
				}
			}
			if qv.qdr != nil {
				qv.start_time = start_time
				qv.wall_duration = Since(start_time)
			}

			//  the flow never resolves until all queries()s
			//  generate an qdr record.

			out <- qv
		}
	}()
	return out
}

func (flo *flow) sql_exec_txn(
	ex *sql_exec,
	in_argv argv_chan,
	in_when bool_chan,
) (out qdr_chan) {

	out = make(qdr_chan)

	go func() {

		defer close(out)

		for flo = flo.get(); flo != nil; flo = flo.get() {

			argv, when := flo.wait_fire(in_argv, in_when)
			if argv == nil {
				return
			}

			//  fire command if 'when' is true and 'argv' not null
			//  otherwise; send null qdr up the pipe

			start_time := Now()

			var qv *qdr_value

			switch {

			//  argv or when is null, so qdr is null

			case argv.is_null || when.is_null:
				qv = &qdr_value{
					is_null: true,
					flow:    flo,
				}

			//  when predicate is true and argv exists, so
			//  fire query

			case when.bool:

				//  invoke sql transaction
				qv = ex.exec_txn(argv.argv)

				qv.flow = flo
				qv.udig = flo.brr[brr_UDIG]
				qv.flow_sequence = flo.seq
				qv.query_name = ex.name

			//  when clause is false.
			//
			//  since resolution requires that an qdr be generated
			//  inside a flow, we send an qdr_value that is both
			//  non null //  but the qdr_value.qdr field is nil.
			//
			//  ugly.

			default:
				qv = &qdr_value{
					is_null: false,
					flow:    flo,
				}
			}
			if qv.qdr != nil {
				qv.start_time = start_time
				qv.wall_duration = Since(start_time)
			}

			//  the flow never resolves until all queries()s
			//  generate an qdr record.

			out <- qv
		}
	}()
	return out
}
