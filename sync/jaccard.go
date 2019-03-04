/*
 *  Synopsis:
 *	Calculate jaccard pairwise metric across a set of blobio services.
 *  Usage:
 *	jaccard <config-file>
 *  Note:
 *	Added UDig of config file in answer json.
 */

package main

import (
	"database/sql"
	"regexp"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
	_ "github.com/lib/pq"
)

type ServiceUDig struct {

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
	Tip			ServiceUDig	`json:"tip"`
	Stats			sql.DBStats	`json:"stats"`
}

type TipQuery struct {

	SourceDB	string	`json:"source_db"`
	source		*PGDatabase

	TargetDB	string	`json:"target_db"`
	target		*PGDatabase

	SourceInTarget	bool	`json:"source_in_target"`
}

type Config struct {
	Databases		map[string]*PGDatabase `json:"databases"`

	SourceBlob		string	`json:"source_blob"`
	EscapeHTML		bool	`json:"escape_html"`
	IndentLinePrefix	string	`json:"indent_line_prefix"`
	IndentPrefix		string	`json:"indent_prefix"`
}

type Answer struct {

	SourceBlob		string		`json:"source_blob"`
	TipNotExistsCount	int		`json:"tip_not_exists_count"`
	TipExistsCount		int		`json:"tip_exists_count"`
	StartTime		time.Time	`json:"start_time"`
	WallDuration		time.Duration	`json:"wall_duration"`
	WallDurationString	string		`json:"wall_duration_string"`

	Databases	map[string]*PGDatabase	`json:"databases"`
	TipQueries		[]TipQuery	`json:"tip_queries"`
}

const usage = "usage: jaccard <config-file-path>\n"
var udig_re_graph, udig_re_ascii *regexp.Regexp

func init() {

	const re_graph = `^[a-z][a-z0-9]{0,7}:[[:graph:]]{1,128}$`
	const re_ascii = `^[a-z][a-z0-9]{0,7}:[[:ascii:]]{1,128}$`

	udig_re_graph = regexp.MustCompile(re_graph)
	udig_re_ascii = regexp.MustCompile(re_ascii)
}

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
	if conf.SourceBlob == "" {
		return
	}
	if !udig_re_graph.Match([]byte(conf.SourceBlob)) {
		die("source udig does not match graphical udig")
	}
	if !udig_re_ascii.Match([]byte(conf.SourceBlob)) {
		die("source udig does not match ascii udig")
	}
}

func (conf *Config) open() {

	done := make(chan *PGDatabase)
	for _, pg := range conf.Databases {
		go pg.open(done)
	}

	//  insure system ids for databases are distinct
	sid := make(map[string]bool, len(conf.Databases)) 
	for cnt := len(conf.Databases);  cnt > 0;  cnt-- {

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

func (pg *PGDatabase) close(done chan bool) {

	pg.Stats = pg.db.Stats()
	err := pg.db.Close()
	if err != nil {
		pg.die("sql.Db.Close() failed: %s", err)
	}
	done <- true
}

func (conf *Config) close() {

	done := make(chan bool)
	for _, pg := range conf.Databases {
		go pg.close(done)
	}
	for cnt := len(conf.Databases);  cnt > 0;  cnt-- {
		<- done
	}
}

func (pg *PGDatabase) select_tip(done chan bool) {

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
		Scan(&pg.Tip.Blob, &pg.Tip.DiscoverTime)
	if err != nil {
		pg.die("QueryRow(tip) failed: %s", err)
	}
	done <- true
}

func (conf *Config) select_tip() {

	done := make(chan bool)
	for _, pg := range conf.Databases {
		go pg.select_tip(done)
	}
	for cnt := len(conf.Databases);  cnt > 0;  cnt-- {
		<- done
	}
}

func (tq *TipQuery) exists(done chan bool) {

	sql := `
SELECT EXISTS (
  SELECT
  	blob
    FROM
    	blobio.service
    WHERE
    	blob = $1
)`
	err := tq.target.db.QueryRow(
			sql,
			tq.source.Tip.Blob,
	).Scan(&tq.SourceInTarget)
	if err != nil {
		die("tip query(" +
			tq.SourceDB +
			", " + tq.TargetDB + "): sql.QueryRow() failed: %s",
			err,
		)
		die("WTF")
	}
	done <- tq.SourceInTarget
}

func (answer *Answer) compare_tip(dbs map[string]*PGDatabase) {

	done := make(chan bool)

	//  check that tip for each database exists in other peer databases


	cnt := len(dbs) * (len(dbs) - 1)
	answer.TipQueries = make([]TipQuery, cnt)
	i := 0

	for src, src_pg := range dbs {
		for tgt, tgt_pg := range dbs {
			if src == tgt {
				continue
			}

			tq := &answer.TipQueries[i]
			tq.SourceDB = src
			tq.source = src_pg
			tq.TargetDB = tgt
			tq.target = tgt_pg
			go tq.exists(done)
			i++
		}
	}

	//  wait for tip queries to finish
	for i = 0;  i < cnt;  i++ {
		if <- done {
			answer.TipExistsCount++
		} else {
			answer.TipNotExistsCount++
		}
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

	//  insure each database has unque system identifier
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

	answer.SourceBlob = conf.SourceBlob
	conf.frisk()
	conf.open()
	conf.select_tip()
	answer.compare_tip(conf.Databases)
	conf.close()
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
