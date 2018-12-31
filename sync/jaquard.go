/*
 *  Synopsis:
 *	Calculate jaquard pairwise metric across a set of blobio services.
 *  Usage:
 *	jaquard <config-file>
 */

package main

import (
	"database/sql"
	"strconv"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	_ "github.com/lib/pq"
)

type PGDatabase struct {
	tag			string
	PGHOST			string
	PGPORT			uint16
	PGUSER			string
	PGPASSWORD		string
	PGDATABASE		string

	db			*sql.DB

	system_identifier	string
	recent_uuid		string
}

type Config struct {
	Databases	map[string]*PGDatabase `json:"databases"`
}

type Results struct {
	
}

const usage = "usage: jaquard <config-file-path>\n"

func die(format string, args ...interface{}) {

	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...);
	os.Stderr.Write([]byte(usage))
	os.Exit(1)
}

func (pg *PGDatabase) die(format string, args ...interface{}) {

	die("database(" + pg.tag + "): " + format, args...)
}

func (pg *PGDatabase) frisk() {

	croak := func(format string, args ...interface{}) {
		pg.die("frisk: " + format, args...)
	}

	empty := func(field string) {
		croak("empty or undefined field: %s", field)
	}
	if pg.PGHOST == "" {
		empty("PGHOST")
	}
	if pg.PGPORT == 0 {
		empty("PGPORT")
	}
	if pg.PGUSER == "" {
		empty("PGUSER")
	}
	if pg.PGDATABASE == "" {
		empty("PGDATABASE")
	}
	if strings.Index(pg.PGPASSWORD, " ") > -1 {
		croak("PGPASSWORD contains space character")
	}
}

func (conf *Config) frisk() {

	for tag, pg := range conf.Databases {
		pg.tag = tag
		pg.frisk()
	}
}

func (conf *Config) open() {

	done := make(chan *PGDatabase)
	for _, pg := range conf.Databases {
		go pg.open(done)
	}

	cnt := len(conf.Databases)

	//  insure system ids for databases are distinct
	sid := make(map[string]bool, cnt) 
	for ;  cnt > 0;  cnt-- {

		//  Note:  do we need a timeout?
		pg := <- done
		if sid[pg.system_identifier] {
			pg.die(
				"duplicate system identifier: %s",
				pg.system_identifier,
			)
		}
		sid[pg.system_identifier] = true
	}
	if len(sid) != len(conf.Databases) {
		die("count of distinct system identifiers != database count")
	}
}

func (pg *PGDatabase) jaquard(done chan *PGDatabase) {


	sql := `
SELECT
	blob
  FROM
  	blobio.service
  ORDER BY
  	discover_time DESC
  LIMIT
  	1
`
	var blob string
	err := pg.db.QueryRow(sql).Scan(&blob)
	if err != nil {
		pg.die("QueryRow(jaquard) failed: %s", err)
	}
	done <- pg
}

func (conf *Config) jaquard() {

	done := make(chan *PGDatabase)
	for _, pg := range conf.Databases {
		go pg.jaquard(done)
	}

	cnt := len(conf.Databases)
	for ;  cnt > 0;  cnt-- {
		<- done
	}
}

func (pg *PGDatabase) open(done chan *PGDatabase) {

	var err error

	url := "dbname=" + pg.PGDATABASE + " " +
			"user=" + pg.PGUSER + " " +
			"host=" + pg.PGHOST + " " +
			"port=" + strconv.Itoa(int(pg.PGPORT)) + " " +
			"sslmode=disable " +
			"connect_timeout=20 " +
			"password=" + pg.PGPASSWORD
	pg.db, err = sql.Open("postgres", url)
	if err != nil {
		pg.die("sql.Open() failed: %s", err)
	}

	err = pg.db.QueryRow(
		`SELECT system_identifier FROM pg_control_system();`,
	).Scan(&pg.system_identifier)
	if err != nil {
		pg.die("db.Query(system_identifier) failed: %s", err)
	}
	if len(pg.system_identifier) == 0 {
		pg.die("empty system identifier")
	}

	done <- pg
}

func main() {

	if (len(os.Args) - 1 != 1) {
		die(
			"wrong number of arguments: got %d, expected 1",
			len(os.Args) - 1,
		)
	}

	//  slurp up the json configuartion file
	cf, err := os.Open(os.Args[1])
	if err != nil {
		die("os.Open(config) failed: %s", err)
	}
	dec := json.NewDecoder(cf)
	dec.DisallowUnknownFields()

	var conf Config
	err = dec.Decode(&conf)
	if err != nil {
		die("dec.Decode(config) failed: %s", err)
	}
	cf.Close()

	conf.frisk()
	conf.open()
	conf.jaquard()
	fmt.Printf("conf=%#v\n", conf)
}
