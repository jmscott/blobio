//  Synopsis:
//	Server action for flowd.
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
//  Note:
//	Number of distinct udigs needs to be tracked.  Perhaps a udig malloc()
//	would be appropriate, as well.
//	
//	When rolling a daily log file, a summary of stats should be append to
//	end of previous day and prepended to new day.
//
//		distinct udr, fdr, xdr/comment and qdr/query ought to
//		summarized.
//
//	Server log needs time zone on both bootup and log roll.
//
package main

import (
	"database/sql"
	"os"
	"os/exec"
	"os/signal"
	"runtime"
	"sort"
	"syscall"

	_ "github.com/lib/pq"

	. "fmt"
	. "strings"
	. "time"
)

const (
	flow_sample_fmt       = "%0.1f flows/sec, wall=%f sec/flow, ok=%d, fault=%d"
	total_flow_sample_fmt = "all: %d flows, wall=%f sec/flow, ok=%d, fault=%d"

	//  grumble when at least one flow worker is this busy but idle
	//  still workers exist.  implies possible thread starvation and
	//  probably needs attention.  when all workers are idle but
	//  request records are arriving then we are deadlocked.
	//
	//  Note: flow_starvation_how_busy needs to be a settable parameter in
	//        in flow file.  Slow flows like large log crunching seem
	//	  to be sensitive.


	flow_starvation_how_busy = 8
)

//  sample of flow activity for a particular worker
type flow_worker_sample struct {
	worker_id int

	fdr_count     uint64
	ok_count      uint64
	fault_count   uint64
	wall_duration Duration
}

func (sam flow_worker_sample) String() (s string) {

	rate := "0"
	if sam.fdr_count > 0 {
		nano := (float64(sam.wall_duration) / float64(sam.fdr_count))
		rate = Sprintf("%s", Duration(nano))
	}
	s = Sprintf("fdr=%d, ok=%d, fault=%d, %s/flow",
			sam.fdr_count,
			sam.ok_count,
			sam.fault_count,
			rate,
		)
	if sam.worker_id == 0 {
		return
	}
	return Sprintf("#%d: %s", sam.worker_id, s)
}

//  Note: think about tracking unique udigs
type flow_worker struct {
	id uint16

	*parse

	brr_chan chan string
	os_exec_chan
	fdr_log_chan     file_byte_chan
	xdr_log_chan     file_byte_chan
	qdr_log_chan     file_byte_chan
	info_log_chan    file_byte_chan
	flow_sample_chan chan<- flow_worker_sample

	seq_chan <-chan uint64
}

var server_leaving bool

//  Synchronusly boot up the flowd server.

func (conf *config) server(par *parse) {

	start_time := Now()
	//  start a rolled logger to flowd-Dow.log, rolled daily
	info_log_ch := make(file_byte_chan)

	roll_start, roll_end := info_log_ch.roll_Dow(
			Sprintf("%s/flowd", conf.log_directory), "log", true)
	roll_when_start := "today"
	roll_when_end := "yesterday"
	boot_sample := flow_worker_sample{}

	//  accullate per roll statistics on roll_sample channel.
	//  burp those stats into end of closing log file and
	//  start of new log file.

	roll_sample := make(chan flow_worker_sample)
	go func() {
		today_sample := flow_worker_sample{}

		roll_entry := func(msg string) []byte {
			return []byte(Sprintf("%s: %s\n",
				Now().Format("2006/01/02 15:04:05"),
				msg,
			))
		}

		for {
			var entries [4][]byte

			select {

			//  rolling to new log file

			//  closing old log file.
			//  append stats before finally closing

			case fr := <- roll_start:
				if fr == nil {
					return
				}
				entries[0] = roll_entry(Sprintf("%s: %s",
						roll_when_start,
						today_sample.String(),
						))
				entries[1] = roll_entry(Sprintf("%s: %s",
						"boot",
						boot_sample.String(),
						))
				z, off := Now().Zone()
				entries[2] = roll_entry(Sprintf(
						"uptime: %s, time zone=%s %d",
						Since(start_time),
						z, off))
				entries[3] = roll_entry(Sprintf("roll %s -> %s",
						fr.open_path, fr.roll_path))
				fr.entries = entries[:]
				roll_start <- fr

			//  finished rolling to new log file
			//  open new log file with stats
			case fr := <- roll_end:
				if fr == nil {
					return
				}
				entries[0] = roll_entry(Sprintf("%s: %s",
						roll_when_end,
						today_sample.String(),
						))
				entries[1] = roll_entry(Sprintf("%s: %s",
						"boot",
						boot_sample.String(),
						))
				z, off := Now().Zone()
				entries[2] = roll_entry(Sprintf(
						"uptime: %s, time zone=%s %d",
						Since(start_time),
						z, off))
				entries[3] = roll_entry(Sprintf(
							"rolled %s -> %s",
							fr.roll_path,
							fr.open_path))
				fr.entries = entries[:]
				roll_end <- fr
				today_sample = flow_worker_sample{}

			// update per roll stats 
			case sam := <- roll_sample:
				today_sample.fdr_count += sam.fdr_count
				today_sample.wall_duration += sam.wall_duration
				today_sample.ok_count += sam.ok_count
				today_sample.fault_count += sam.fault_count
			}
		}
	}()

	info := info_log_ch.info
	WARN := info_log_ch.WARN

	leave := func(status int) {

		var f *file
		f.unlink("run/flowd.pid")

		info("good bye, cruel world")
		Sleep(Second)
		os.Exit(status)
	}

	defer func() {
		if r := recover(); r != nil {
			info_log_ch.ERROR("panic: %s", r)
			leave(1)
		}
	}()

	go func() {
		c := make(chan os.Signal)
		signal.Notify(c, syscall.SIGTERM)
		s := <-c
		server_leaving = true
		WARN("caught signal: %s", s)
		leave(1)
	}()

	info("hello, world")

	//  create run/flowd.pid file
	{
		pid := os.Getpid()

		pid_path := "run/flowd.pid"

		_, err := os.OpenFile(pid_path, os.O_RDONLY, 0)
		if err == nil {
			WARN("process id file exists: %s", pid_path)
			//  Note: write the pid of the previous process
			WARN("another flowd process may be running")
		} else if !os.IsNotExist(err) {
			panic(err)
		}

		f, err := os.Create(pid_path)
		if err != nil {
			panic(err)
		}
		_, err = f.WriteString(Sprintf("%d\n", pid))
		if err != nil {
			panic(err)
		}
		err = f.Close()
		if err != nil {
			panic(err)
		}
		info("process id %d written to file: %s", pid, pid_path)
	}
	info("go version: %s", runtime.Version())
	info("number of cpus: %d", runtime.NumCPU())
	info("GOROOT(): %s", runtime.GOROOT())
	info("GOMAXPROCS(0): %d", runtime.GOMAXPROCS(0))
	info("heartbeat duration: %s", conf.heartbeat_duration)
	info("memstats duration: %s", conf.memstats_duration)

	//  list environment variables
	{
		env := os.Environ()
		info("listing %d environment variables ...", len(env))
		sort.Strings(env)

		for _, n := range env {
			info("	%s", n)
		}
	}

	info("enumerated dependency order: %d commands/queries/tail",
		len(par.depend_order))
	for i, n := range par.depend_order {
		info("	% 3d: %s", i, n)
	}

	info("looking up paths for %d commands", len(conf.command))
	for _, cmd := range conf.command {
		fp, err := exec.LookPath(cmd.path)

		//  should not abort if the file does not exist in the path
		if err != nil {
			panic(err)
		}
		cmd.full_path = fp
		info("	%s -> %s", cmd.path, cmd.full_path)
	}

	info("os exec capacity: %d", conf.os_exec_capacity)
	osx_q := make(os_exec_chan, conf.os_exec_capacity)

	info("spawning %d os exec workers", conf.os_exec_worker_count)
	for i := uint16(0); i < conf.os_exec_worker_count; i++ {
		go osx_q.worker_flowd_execv()
	}

	info("opening brr tail %s (cap=%d) for %s biod.brr",
		conf.tail.name, conf.brr_capacity, conf.tail.path)
	tail := &tail{
		name:            conf.tail.name,
		path:            conf.tail.path,
		output_capacity: conf.brr_capacity,
	}
	brr_chan := tail.forever()

	//  installed database drivers ...
	{
		d := sql.Drivers()
		info("%d installed database/sql drivers", len(d))
		for _, n := range d {
			info("	%s", n)
		}
	}

	//  open databases
	if len(conf.sql_database) > 0 {
		info("opening %d databases", len(conf.sql_database))
		for _, db := range conf.sql_database {
			var err error

			n := Sprintf("%s/%s",
				db.name,
				db.driver_name,
			)
			if db.data_source_name == "" {
				info("	%s: no data source, so using default")
			} else {
				info("	%s: data source name: %s",
					n, db.data_source_name)
			}
			db.opendb, err = sql.Open(
				db.driver_name,
				db.data_source_name,
			)
			if err != nil {
				panic(Sprintf("%s: %s", db.name, err))
			}
			defer db.opendb.Close()

			info("	%s: max idle connections: %d",
				n, db.max_idle_conns)
			db.opendb.SetMaxIdleConns(db.max_idle_conns)

			info("	%s: max open connections: %d",
				n, db.max_open_conns)
			db.opendb.SetMaxOpenConns(db.max_open_conns)
		}
	} else {
		info("no sql databases defined (ok): %s", conf.path)
	}

	info("preparing %d sql query row declarations", len(conf.sql_query_row))
	for _, q := range conf.sql_query_row {
		var err error

		info("	%s", q.name)
		q.stmt, err = q.sql_database.opendb.Prepare(q.statement)
		if err != nil {
			panic(Sprintf("%s: %s", q.name, err))
		}
		defer q.stmt.Close()
	}

	info("preparing %d sql exec declarations", len(conf.sql_exec))
	for _, ex := range conf.sql_exec {
		var err error

		ex.stmt = make([]*sql.Stmt, len(ex.statement))
		info("	%s (%d statements)", ex.name, len(ex.statement))
		for i, s := range ex.statement {
			info("		#%d: %s",
				i,
				TrimRight((TrimSpace(s))[:20], "\n"),
			)
			ex.stmt[i], err = ex.sql_database.opendb.Prepare(s)
			if err != nil {
				panic(Sprintf("%s: %s", ex.name, err))
			}
			defer ex.stmt[i].Close()
		}
	}

	//  flow detail record log file

	info("fdr file roll duration: %s", conf.fdr_roll_duration)
	path := Sprintf("%s/flowd", conf.log_directory)
	info("opening fdr log file: %s.fdr", path)
	fdr_log_chan := make(file_byte_chan)
	fdr_log_chan.roll_epoch(path, "fdr", conf.fdr_roll_duration, false)

	//  execution detail record log file

	info("xdr file roll duration: %s", conf.xdr_roll_duration)
	path = Sprintf("%s/flowd", conf.log_directory)
	info("opening xdr log file: %s.xdr", path)
	xdr_log_chan := make(file_byte_chan)
	xdr_log_chan.roll_epoch(path, "xdr", conf.xdr_roll_duration, false)

	//  query detail log file

	info("qdr file roll duration: %s", conf.qdr_roll_duration)
	path = Sprintf("%s/flowd", conf.log_directory)
	info("opening qdr log file: %s.qdr", path)
	qdr_log_chan := make(file_byte_chan)
	qdr_log_chan.roll_epoch(path, "qdr", conf.qdr_roll_duration, false)

	//  start a sequence channel for the fdr records

	seq_q := make(chan uint64, conf.brr_capacity)
	go func() {
		seq := uint64(1)
		for {
			seq_q <- seq
			seq++
		}
	}()

	info("spawning %d flow workers", conf.flow_worker_count)
	flow_sample_ch := make(chan flow_worker_sample, conf.brr_capacity)
	for i := uint16(1); i <= conf.flow_worker_count; i++ {
		go (&flow_worker{
			id: uint16(<-seq_q),

			parse: par,

			brr_chan:         brr_chan,
			os_exec_chan:     osx_q,
			fdr_log_chan:     fdr_log_chan,
			xdr_log_chan:     xdr_log_chan,
			qdr_log_chan:     qdr_log_chan,
			info_log_chan:    info_log_ch,
			flow_sample_chan: flow_sample_ch,

			seq_chan: seq_q,
		}).flow()
	}

	//  monitor the incremental samples sent by the workers
	//  and periodically publish the stats

	info("starting logger for incremental stats")
	sample := flow_worker_sample{}

	//  stat burped on every heart beat
	active_count := conf.flow_worker_count
	worker_stats := make([]int, conf.flow_worker_count)

	heartbeat := NewTicker(conf.heartbeat_duration)
	hb := float64(conf.heartbeat_duration) / float64(Second)

	memstat_tick := NewTicker(conf.memstats_duration)

	info("flow starvation threshold: %d", flow_starvation_how_busy)
	//  wait for all workers to finish
	for active_count > 0 {
		select {

		//  fdr sample from flow worker
		case sam := <-flow_sample_ch:
			if sam.worker_id < 0 {
				info("worker #%d exited", -sam.worker_id)
				active_count--
				break
			}
			sample.fdr_count++
			sample.ok_count += sam.ok_count
			sample.fault_count += sam.fault_count
			sample.wall_duration += sam.wall_duration
			worker_stats[sam.worker_id-1]++

			//  update roll level sample stats
			roll_sample <- sam

		//  burp out flow stats every heartbeat
		case <-heartbeat.C:

			bl := len(brr_chan)

			sfc := sample.fdr_count
			switch {

			//  no completed flows seen, no blob requests in queue
			case sfc == 0 && bl == 0:
				info("next heartbeat: %s",
							conf.heartbeat_duration)
				continue

			//  no completed flows seen, but unresolved exist 
			case sfc == 0:
				WARN("no fdr samples seen in %.0f sec", hb)
				WARN("all jobs may be running > %.0f sec")
				WARN("perhaps increase heartbeat duration")
				continue
			}
			info("%s sample: %s", conf.heartbeat_duration, sample)

			//  help debug starvation of flow thread

			info("summary of %d active workers ...", active_count)
			idle_count := active_count
			max_busy := 0
			for i, c := range worker_stats {
				if c > 0 {
					if c > max_busy {
						max_busy = c
					}
					info("	#%d: %d flows", i+1, c)
					worker_stats[i] = 0
					idle_count--
				} else {
					info("	#%d idle", i+1)
				}
			}

			//  check that all flow workers are busy.
			//  warn about existence of idle workers when at
			//  least one worker appears to be be "busy".
			//  Note: wouldn't standard deviation be better measure?

			if idle_count == 0 {
				info("all %d flow workers busy", active_count)
			} else if max_busy >= flow_starvation_how_busy {
				info_log_ch.WARN("%d flow workers stagnant",
					idle_count)
			}

			//  update accumulated totals
			boot_sample.fdr_count += sfc
			boot_sample.ok_count += sample.ok_count
			boot_sample.fault_count += sample.fault_count
			boot_sample.wall_duration += sample.wall_duration

			sample = flow_worker_sample{}

			info("boot: %s", boot_sample)
			info("brr in queue: %d", bl)
		case <-memstat_tick.C:
			var m runtime.MemStats

			now := Now()
			runtime.ReadMemStats(&m)
			info("%s MemStat for %d routines, read in %s",
				conf.memstats_duration,
				runtime.NumGoroutine(),
				Since(now),
			)
			info("--")
			info("        Allocated in Use: %d bytes", m.Alloc)
			info("         Total Allocated: %d bytes", m.TotalAlloc)
			info("             From System: %d bytes", m.Sys)
			info("         Pointer Lookups: %d lookups", m.Lookups)
			info("                 Mallocs: %d mallocs", m.Mallocs)
			info("                   Frees: %d frees", m.Frees)
			info("   Heap Allocated in Use: %d bytes", m.HeapAlloc)
			info("        Heap From System: %d bytes", m.HeapSys)
			info("      Heap in Idle Spans: %d bytes", m.HeapIdle)
			info("   Heap in Non-Idle Span: %d bytes", m.HeapInuse)
			info("     Heap Released to OS: %d bytes",
				m.HeapReleased)
			info("Heap Total Alloc Objects: %d", m.HeapObjects)
			info("--")
		}
	}
	leave(0)
}

// slurp brr records from a string chan and fire associated rules

func (work *flow_worker) flow() {

	boot_seq := uint64(work.id)
	flowA := &flow{
		seq:      boot_seq,
		next:     make(chan flow_chan),
		resolved: make(chan struct{}),
	}

	cmpl := &compile{
		parse:         work.parse,
		flow:          flowA,
		os_exec_chan:  work.os_exec_chan,
		fdr_log_chan:  work.fdr_log_chan,
		xdr_log_chan:  work.xdr_log_chan,
		qdr_log_chan:  work.qdr_log_chan,
		info_log_chan: work.info_log_chan,
	}

	fc := cmpl.compile()

	//  first flow resolves immediately
	close(flowA.resolved)

	sam := flow_worker_sample{
		worker_id: int(work.id),
	}

	for line := range work.brr_chan {

		var brr *brr
		var err error

		brr, err = brr.parse(TrimRight(line, "\n"))
		if err != nil {
			panic(err)
		}

		flowB := &flow{
			brr:      brr,
			next:     make(chan flow_chan),
			seq:      <-work.seq_chan,
			resolved: make(chan struct{}),
		}

		//  push flowA to flowB

		for flowA.confluent_count > 0 {

			reply := <-flowA.next
			flowA.confluent_count--

			reply <- flowB
			flowB.confluent_count++
		}

		//  wait for flowB to finish
		fv := <-fc
		if fv == nil {
			break
		}
		close(fv.resolved)

		//  cheap sanity test

		if fv.flow.seq != flowB.seq {
			panic("fdr out of sync with flowB")
		}

		//  send stats to server goroutine

		sam.ok_count = uint64(fv.fdr.ok_count)
		sam.fault_count = uint64(fv.fdr.fault_count)
		sam.wall_duration = fv.fdr.wall_duration
		work.flow_sample_chan <- sam

		flowA = flowB
	}

	//  indicate termination by negating worker id
	sam.worker_id = -sam.worker_id
	work.flow_sample_chan <- sam
}
