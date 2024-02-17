//Synopsis:
//	Logging for info/WARN/ERROR

/*  why not use https://github.com/jmscott/play/tree/master/logroll  */

package main

import (
	"os"
	"sync"

	. "fmt"
	. "time"
)

func (out file_byte_chan) info(format string, args ...interface{}) {

	out <- []byte(Sprintf("%s: %s\n", Now().Format("2006/01/02 15:04:05"),
		Sprintf(format, args...)))
}

func (out file_byte_chan) ERROR(format string, args ...interface{}) {

	out.info("%s", "ERROR: " + Sprintf(format, args...))
}

func (out file_byte_chan) WARN(format string, args ...interface{}) {

	out.info("%s", "WARN: " + Sprintf(format, args...))
}

func (out *file_byte_chan) die(format string, args ...interface{}) {

	out.ERROR(format, args...)
	os.Exit(1)
}

var wtf_io chan *wtf_payload
var wtf_once sync.Once

type wtf_payload struct {
	msg  string
	done chan bool
}

func init() {
	//  writes to stdin appear to not be atomic, so serialize wtf_io
	wtf_io = make(chan *wtf_payload)
}

//  trivial, serialized debug printf() with easy to grep prefix

func wtf(format string, args ...interface{}) {

	io := func() {
		go func() {
			for pay := range wtf_io {
				stderr.Write([]byte(pay.msg))
				pay.done <- true
			}
		}()
	}

	//  boot the background routine to sequence the io
	wtf_once.Do(io)

	pay := &wtf_payload{
		msg:  Sprintf("WTF: %s\n", Sprintf(format, args...)),
		done: make(chan bool),
	}
	wtf_io <- pay
	<-pay.done
}

func (in file_byte_chan) roll_Dow(
	prefix, suffix string,
	notify_roll bool,
) (start, end chan *file_roll_notify) {

	return in.map_roll(
		prefix,
		"Dow",
		suffix,
		3*Second,
		true,
		notify_roll,
	)
}

func (in file_byte_chan) roll_YYYYMMDD(
	prefix, suffix string,
	notify_roll bool,
) (start, end chan *file_roll_notify) {

	return in.map_roll(
		prefix,
		"YYYYMMDD",
		suffix,
		3*Second,
		true,
		notify_roll,
	)
}

func (in file_byte_chan) roll_epoch(
	prefix, suffix string,
	roll_duration Duration,
	notify_roll bool,
) (start, end chan *file_roll_notify) {

	return in.map_roll(
		prefix,
		"epoch",
		suffix,
		roll_duration,
		false,
		notify_roll,
	)
}

func (in file_byte_chan) roll_YYYYMMDD_hhmmss(
	prefix, suffix string,
	roll_duration Duration,
	notify_roll bool,
) (start, end chan *file_roll_notify) {

	return in.map_roll(
		prefix,
		"YYYYMMDD_hhmmss",
		suffix,
		roll_duration,
		false,
		notify_roll,
	)
}
