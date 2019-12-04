//Synopsis:
//	Create and update run/flowd.pid file every minute
//Note:
//	Add stats like OK/FAULT.
//
//	a race condition exists updating run/flowd.pid.
//	need to update in temp first, then do a rename to run/flowd.pid

package main

import (
	"os"
	"strconv"
	"time"
)

var boot_time time.Time
const stale_duration = 120		//  120 seconds
const pid_update_pause = time.Minute
const pid_path = "run/flowd.pid"
const perms = 0740;

func put_pid_log(do_create bool) {

	flag := os.O_WRONLY
	if (do_create) {
		flag |= os.O_CREATE
	}
	msg := []byte(
		strconv.FormatInt(int64(os.Getpid()), 10) + "\n" +
		strconv.FormatInt(
			int64(time.Now().Sub(boot_time).Seconds()),
			10,
		) + "\n")
	f, err := os.OpenFile(pid_path, flag, perms)
	if err != nil {
		croak("os.OpenFile(pid log) failed: %s", err)
	}

	//  write string to run/flowd.pid
	//
	//	: <pid>\n<uptime-duration>\n
	_, err = f.Write(msg)
	if err != nil {
		croak("os.WriteString(pid log) failed: %s", err)
	}
	err = f.Close()
	if err != nil {
		croak("Close(pid log) failed: %s", err)
	}
}

func update_pid_log() {

	for {
		time.Sleep(pid_update_pause);
		put_pid_log(false)
	}
}

func boot_pid_log() (is_stale bool) {

	boot_time = time.Now()

	//  zap a stale pid or croak if recently updated
	if st, err := os.Stat(pid_path);  err == nil {

		//  if the file has not been touched in awhile
		//  then remove and move on.

		if time.Now().Unix() - st.ModTime().Unix() < stale_duration {
			croak("is another flowd process running?")
		}
		is_stale = true
		err = os.Remove(pid_path)
		if err != nil {
			croak("os.Remove(pid log) failed: %s", err)
		}
	} else if !os.IsNotExist(err) {
		croak("os.Stat(pid log) failed: %s", err)
	}
	put_pid_log(true)
	go update_pid_log()
	return
}
