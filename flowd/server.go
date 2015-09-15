//  Synopsis:
//	Server action for flowd.
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
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
	total_flow_sample_fmt = "total: %d flows, wall=%f sec/flow, ok=%d, fault=%d"
	//  grumble when at least one flow worker is this busy but idle
	//  still workers exist.  implies possible thread starvation and
	//  probably needs attention.  when all workers are idle but
	//  request records are arriving then we are deadlocked.

	flow_starvation_how_busy = 4
)

//  sample of flow activity for a particular worker
type flow_worker_sample struct {
	worker_id int

	ok_count      uint64
	fault_count   uint64
	wall_duration Duration
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

//  Synchronusly boot up the flowd server.

func (conf *config) server(par *parse) {

	//  start a rolled logger to flowd-Dow.log, rolled daily
	info_log_ch := make(file_byte_chan)
	info_log_ch.roll_Dow(
		Sprintf("%s/flowd", conf.log_directory), "log", false)

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
		WARN("caught signal: %s", s)
		leave(1)
	}()

	info("hello, world")

	//  create run/flowd.pid file
	{
		pid := os.Getpid()

		pid_path := "run/flowd.pid"
		info("process id %d", pid)

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
		info("process id written to file: %s", pid_path)
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
		go osx_q.worker()
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
	total := flow_worker_sample{}
	total_fdr_count := uint64(0)
	sample_fdr_count := uint64(0)

	memstat_tick := NewTicker(conf.memstats_duration)

	info("flow starvation how busy count: %d", flow_starvation_how_busy)
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
			sample_fdr_count++
			sample.ok_count += sam.ok_count
			sample.fault_count += sam.fault_count
			sample.wall_duration += sam.wall_duration
			worker_stats[sam.worker_id-1]++

		//  burp out flow stats every heartbeat
		case <-heartbeat.C:

			bl := len(brr_chan)
			if sample_fdr_count == 0 {
				info("next heartbeat in %.0f seconds", hb)
				if bl > 1 {
					WARN("brr channel not draining: %d", bl)
				}
				continue
			}
			info("brr in queue: %d", bl)
			info("%s sample: wall rate=%s/flow",
				conf.heartbeat_duration,
				Duration(uint64(
					sample.wall_duration)/sample_fdr_count),
			)
			info("%s sample: fdr=%d, ok=%d, fault=%d",
				conf.heartbeat_duration,
				sample_fdr_count,
				sample.ok_count,
				sample.fault_count,
			)

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
			total_fdr_count += sample_fdr_count
			total.ok_count += sample.ok_count
			total.fault_count += sample.fault_count
			total.wall_duration += sample.wall_duration

			sample_fdr_count = 0
			sample.wall_duration = 0
			sample.ok_count = 0
			sample.fault_count = 0

			info("total: wall rate=%s/flow",
				Duration(uint64(
					total.wall_duration)/total_fdr_count))

			info("total: fdr=%d, ok=%d, fault=%d",
				total_fdr_count,
				total.ok_count,
				total.fault_count,
			)
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

	flow0 := &flow{
		seq:      uint64(work.id),
		next:     make(chan flow_chan),
		resolved: make(chan struct{}),
	}

	cmpl := &compile{
		parse:         work.parse,
		flow:          flow0,
		os_exec_chan:  work.os_exec_chan,
		fdr_log_chan:  work.fdr_log_chan,
		xdr_log_chan:  work.xdr_log_chan,
		qdr_log_chan:  work.qdr_log_chan,
		info_log_chan: work.info_log_chan,
	}

	//  flip_flow() both waits for a flow detail record on fB and
	//  waits for all outstanding goroutines, waiting in qualfications,
	//  to move from fA to fB.
	//
	//  A flow detail record can be generated before all the qualification
	//  workers on that flow have finished.  For example, the 'when' clause
	//
	//	when
	//		condition1
	//		or
	//		condition2
	//
	//  would always resolve to true if either condition1 or condition2 is
	//  true, even if the other condition is null.
	//
	//  flip_flow() assumes upon entry that fA has resolved,
	//  implying that the channel fA.resolved has been closed.
	//  Also, flip_flow() assumes that fA may have outstanding
	//  qualification goroutines, implying that possibly
	//  fA.confluence_count > 0.
	//
	//  Upon exit of flip_flow() two conditions must be met.  fB
	//  must read a new flow detail record and fA must have no
	//  outstanding qualification goroutines.  After return from
	//  flip_flow() the caller sets flow0 to flow1 and creates a new
	//  flow1 for the newly read flow detail record.
	//
	//  and the beat goes on.

	flip_flow := func(fc fdr_chan, fA, fB *flow) (fv *fdr_value) {

		//  Note: wait for an fdr from flow1 and for all stragglers
		//  from flow0 to terminate.

		for fv == nil || fA.confluent_count > 0 {

			select {

			//  send flow1 to resolved goroutines which resolved
			//  flow0

			case reply := <-fA.next:
				fA.confluent_count--
				reply <- fB
				fB.confluent_count++

			//  flow1 has finished.
			case fv = <-fc:
				if fv == nil {
					return nil
				}

				//  cheap sanity test to insure we are in sync

				if fv.flow.seq != fB.seq {
					panic("flow out of sync: zombie seen")
				}
				close(fv.flow.resolved)
			}
		}
		return
	}

	fc := cmpl.compile()

	//  first flow resolves immediately
	close(flow0.resolved)

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

		flow1 := &flow{
			brr:      brr,
			next:     make(chan flow_chan),
			seq:      <-work.seq_chan,
			resolved: make(chan struct{}),
		}

		//  flow0 is resolved, so wait for an fdr from flo1,
		//  which implies flow1 has resolved, then flip flow1 to flow0.

		fv := flip_flow(fc, flow0, flow1)
		if fv == nil {
			break
		}
		flow0 = flow1

		sam.ok_count = uint64(fv.fdr.ok_count)
		sam.fault_count = uint64(fv.fdr.fault_count)
		sam.wall_duration = fv.fdr.wall_duration

		work.flow_sample_chan <- sam
	}

	//  indicate termination by negating worker id
	sam.worker_id = -sam.worker_id
	work.flow_sample_chan <- sam
}
