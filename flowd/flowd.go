//Synopsis:
//	flowd command main()

package main

import (
	"bufio"
	"os"
	"syscall"

	. "fmt"
	. "time"
)

var (
	stderr = os.NewFile(uintptr(syscall.Stderr), "/dev/stderr")
)

//  temporary die() used only during boot up
func croak(format string, args ...interface{}) {

	Fprintf(stderr, "flowd: ERROR: %s\n", Sprintf(format, args...))
	Fprintf(stderr,
		"usage: flowd [server|parse|ast|depend] <config_path>\n")
	os.Exit(255)
}

// flowd [server|parse|ast|depend] <schema.flow>
func main() {

	if len(os.Args) != 3 {
		croak("wrong number of arguments: expected 2, got %d",
			len(os.Args) - 1)
	}
	action := os.Args[1]

	switch action {
		case "server":
		case "ast":
		case "depend":
		case "parse": 
		default:
			croak("unknown action: %s", action)
	}

	conf := &config{
		path:                 os.Args[2],
		command:              make(map[string]*command),
		sync_map:             make(map[string]*sync_map),
		Clear_sync_map:       make(map[string]*sync_map),
		Delete_sync_map:      make(map[string]*sync_map),
		sql_database:         make(map[string]*sql_database),
		sql_query_row:        make(map[string]*sql_query_row),
		sql_exec:             make(map[string]*sql_exec),
		brr_capacity:         1,
		flow_worker_count:    1,
		os_exec_worker_count: 1,
		os_exec_capacity:     1,
		xdr_roll_duration:    24 * Hour,
		fdr_roll_duration:    24 * Hour,
		qdr_roll_duration:    24 * Hour,
		heartbeat_duration:   10 * Second,
		memstats_duration:    15 * Minute,
		log_directory:        "log",
	}
	cf, err := os.OpenFile(conf.path, os.O_RDONLY, 0)
	if err != nil {
		if os.IsNotExist(err) {
			croak("configuration file does not exist: %s",
				conf.path)
		} else if os.IsPermission(err) {
			croak("not permitted to open configuration file: %s",
				conf.path)
		}
		croak("failed to open configuration file: %s", err.Error())
	}

	par, err := conf.parse(bufio.NewReader(cf))
	if err != nil {
		croak("%s: %s", action, err)
	}
	switch action {
	case "parse":
	case "server":
		conf.server(par)
	case "ast":
		par.ast_root.print()
	case "depend":
		for _, n := range par.depend_order {
			Println(n)
		}
	default:
		croak("unknown action: %s", action)
	}
	os.Exit(0)
}
