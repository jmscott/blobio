/*
 *  Synopsis:
 *	Calculate jaccard pairwise metric across a set of blobio databases.
 *  Usage:
 *	jaccard <config-file>
 *  Note:
 *	Need to add uDig of config file in answer json.
 */

package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"database/sql"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"

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

	SystemIdentifier	string		`json:"system_identifier"`
	Stats			sql.DBStats	`json:"stats"`
}

type Config struct {
	Databases		map[string]*PGDatabase `json:"databases"`

	SourceBlob		string	`json:"source_blob"`
	EscapeHTML		bool	`json:"escape_html"`
	IndentLinePrefix	string	`json:"indent_line_prefix"`
	IndentPrefix		string	`json:"indent_prefix"`
}

type AnswerService struct {
	DatabaseTag	string			`json:"database_tag"`
	pg		*PGDatabase

	BlobCount	uint64			`json:"blob_count"`
	BlobSortSHA256	string			`json:"blob_sort_sha256"`

	answer		*Answer
}

type Answer struct {
	StartTime		time.Time	`json:"start_time"`
	WallDuration		time.Duration	`json:"wall_duration"`
	WallDurationString	string		`json:"wall_duration_string"`
	Databases		map[string]*PGDatabase	`json:"databases"`
	AnswerService	map[string]*AnswerService `json:"answer_service"`

	ConfigPath	string			`json:"config_path"`
	ConfigBlob	string			`json:"config_blob"`
	config		*Config
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
		`
SELECT
	system_identifier
  FROM
  	pg_control_system()

	`).Scan(&pg.SystemIdentifier)
	if err != nil {
		pg.die("db.Query(system_identifier) failed: %s", err)
	}
	if len(pg.SystemIdentifier) == 0 {
		pg.die("empty system identifier")
	}
	done <- pg
}

func (as *AnswerService) select_service(done chan *PGDatabase) {

	pg := as.pg
	f, err := os.OpenFile(
			"service-" + pg.SystemIdentifier + ".udig",
			os.O_WRONLY | os.O_CREATE | os.O_TRUNC,
			0755,
	)
	if err != nil {
		pg.die("os.Open(select_service) failed: %s", err)
	}
	defer f.Close()
	service := bufio.NewWriter(f)

	//  insure each database has unque system identifier
	rows, err := pg.db.Query(
		`
SELECT
	blob
  FROM
  	blobio.service
  ORDER BY
  	blob ASC
	`)
	if err != nil {
		pg.die("db.Query(service) failed: %s", err)
	}
	defer rows.Close()

	h256 := sha256.New()
	for rows.Next() {
		var blob string

		if err := rows.Scan(&blob); err != nil {
			pg.die("rows.Scan(blob) failed: %s", err)
		}
		b := []byte(blob)
		h256.Write(b)
		service.Write(b)
		service.Write([]byte("\n"))
		as.BlobCount++
	}
	err = service.Flush()
	if err != nil {
		pg.die("Flush(service) failed: %s", err)
	}
	as.BlobSortSHA256 = fmt.Sprintf("%x", h256.Sum(nil))
	done <- pg
}

func (an *Answer) jaccard() {

	done := make(chan *PGDatabase)

	an.AnswerService = make(map[string]*AnswerService)

	for _, pg := range an.config.Databases {

		as := &AnswerService{
			DatabaseTag:	pg.tag,
			pg:		pg,

			answer:		an,
		}
		an.AnswerService[pg.tag] = as
		go as.select_service(done)
	}
	for cnt := len(an.AnswerService);  cnt > 0;  cnt-- {
		<- done
	}
}

func main() {

	now := time.Now()

	if (len(os.Args) - 1 != 1) {
		die(
			"wrong number of arguments: got %d, expected 1",
			len(os.Args) - 1,
		)
	}

	//  slurp up the json configuartion file into a string
	cf_buf, err := ioutil.ReadFile(os.Args[1])
	if err != nil {
		die("ioutil.ReadFile(config) failed: %s", err)
	}
	dec := json.NewDecoder(bytes.NewReader(cf_buf))
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

	conf.frisk()

	conf.open()

	answer := &Answer {
		StartTime:	now,
		Databases:	conf.Databases,
		config:		&conf,
		ConfigPath:	os.Args[1],
		ConfigBlob:	fmt.Sprintf(
					"sha256:%x",
					sha256.Sum256(cf_buf),
				),
	}
	answer.jaccard()
	conf.close()

	//  write out answer as json
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
