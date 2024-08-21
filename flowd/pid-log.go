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
	"sync"
	"time"
)

var boot_time time.Time
var pid_mux sync.Mutex
const stale_duration = 120		//  120 seconds
const pid_update_pause = time.Minute
const pid_path = "run/flowd.pid"

func write_pid_log() {

	msg := []byte(
		strconv.FormatInt(int64(os.Getpid()), 10) + "\n" +
		strconv.FormatInt(int64(boot_time.Unix()), 10)+ "\n",
	)

	pid_mux.Lock()
	defer pid_mux.Unlock()

	f, err := os.OpenFile(
			pid_path,
			os.O_WRONLY|os.O_CREATE|os.O_TRUNC,
			0740,
	)
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
		now := time.Now()
		pid_mux.Lock()
		err := os.Chtimes(pid_path, now, now);
		pid_mux.Unlock()

		if err != nil {
			croak("os.Chtimes(pid log) failed: %s", err)
		}
	}
}

func boot_pid_log() (is_stale bool) {

	boot_time = time.Now()

	//  zap a stale pid or croak if recently updated
	if st, err := os.Stat(pid_path);  err == nil {

		pid_mux.Lock()
		defer pid_mux.Unlock()

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
		os.Remove("run/flowd.gyr")
	} else if !os.IsNotExist(err) {
		croak("os.Stat(pid log) failed: %s", err)
	}
	write_pid_log()
	go update_pid_log()
	return
}
