/*
 *  Synopsis:
 *	Calculate jaquard pairwise metric across a set of blobio services.
 *  Usage:
 *	jaquard <config-file>
 *  Note:
 *	Added UDig of config file in answer json.
 */

package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
	_ "github.com/lib/pq"
)

type ServiceUDIG struct {

	Blob		string		`json:"blob"`
	DiscoverTime	time.Time	`json:"discover_time"`
}

type PGDatabase struct {
	tag			string
	PGHOST			string
	PGPORT			uint16
	PGUSER			string
	PGPASSWORD		string
	PGDATABASE		string

	db			*sql.DB

	SystemIdentifier	string		`json:"system_identifier"`
	TipUDIG			ServiceUDIG	`json:"tip_udig"`
}

type Config struct {
	Databases		map[string]*PGDatabase `json:"databases"`

	EscapeHTML		bool	`json:"escape_html"`
	IndentLinePrefix	string	`json:"indent_line_prefix"`
	IndentPrefix		string	`json:"indent_prefix"`
}

type Answer struct {

	StartTime		time.Time	`json:"start_time"`
	WallDuration		time.Duration	`json:"wall_duration"`
	WallDurationString	string		`json:"wall_duration_string"`

	Databases	map[string]*PGDatabase	`json:"databases"`
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
		if sid[pg.SystemIdentifier] {
			pg.die(
				"duplicate system identifier: %s",
				pg.SystemIdentifier,
			)
		}
		sid[pg.SystemIdentifier] = true
	}
	if len(sid) != len(conf.Databases) {
		die("count of distinct system identifiers != database count")
	}
}

func (pg *PGDatabase) jaquard(done chan *PGDatabase) {


	sql := `
SELECT
	blob,
	discover_time
  FROM
  	blobio.service
  ORDER BY
  	discover_time DESC
  LIMIT
  	1
`
	err := pg.db.
		QueryRow(sql).
		Scan(&pg.TipUDIG.Blob, &pg.TipUDIG.DiscoverTime)
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

	pg.db, err = sql.Open(
			"postgres",
			"dbname=" + pg.PGDATABASE + " " +
			"user=" + pg.PGUSER + " " +
			"host=" + pg.PGHOST + " " +
			"port=" + strconv.Itoa(int(pg.PGPORT)) + " " +
			"sslmode=disable " +
			"connect_timeout=20 " +
			"password=" + pg.PGPASSWORD,
	)
	if err != nil {
		pg.die("sql.Open() failed: %s", err)
	}

	err = pg.db.QueryRow(
		`SELECT system_identifier FROM pg_control_system();`,
	).Scan(&pg.SystemIdentifier)
	if err != nil {
		pg.die("db.Query(system_identifier) failed: %s", err)
	}
	if len(pg.SystemIdentifier) == 0 {
		pg.die("empty system identifier")
	}

	done <- pg
}

func main() {

	answer := &Answer{
		StartTime:	time.Now(),
	}

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

	conf := Config{
		EscapeHTML:		false,
		IndentLinePrefix:	"",
		IndentPrefix:		"\t",
	}
	err = dec.Decode(&conf)
	if err != nil {
		die("json.Decode(config) failed: %s", err)
	}
	err = cf.Close()
	if err != nil {
		die("os.File.Close(config) failed: %s", err)
	}

	conf.frisk()
	conf.open()
	conf.jaquard()
	answer.Databases = conf.Databases

	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(conf.EscapeHTML)
	enc.SetIndent(conf.IndentLinePrefix, conf.IndentPrefix)

	answer.WallDuration = time.Since(answer.StartTime)
	answer.WallDurationString = answer.WallDuration.String()
	err = enc.Encode(&answer)
	if err != nil {
		die("json.Encode(answer) failed: %s", err)
	}
}
