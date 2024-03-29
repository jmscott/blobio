/*
 *  Synopsis:
 *	Calculate jaccard metrics across all pairs of peer blob databases.
 *  Description:
 *	The jaccard metric measures the "sameness" of two sets:
 *
 *		count(A intersect B) / count(A union B)
 *
 *	For the blob databases, we measure the existence of various blobs in
 *	pairs of database that are peers of each other. The simplest measure
 *	is to compare all blobs in one peer against  all the other peer
 *	databases.  Another variation is to determine if the most recent N
 *	blobs in a peer exist in the other peers;  when N == 1 we call the
 *	blob the "tip".	 For, say, three peer databases, we calculate 
 *
 *		3 * 2 == 6
 *
 *	metrics of "sameness".
 *
 *	A list of missing blobs is summarized at the end of the report.
 *	The missing blobs are not specific to pair of database pairs.
 *
 *	The output of jaccard is in json.
 *
 *  Usage:
 *	jaccard <config-file>
 *  Note:
 *	Temp service files not removed!
 *
 *	Need to add uDig of config file in answer json.
 */

package main

import (
	"bytes"
	"crypto/sha1"
	"database/sql"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"sync"
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

	WitnessBlob		string	`json:"witness_blob"`
	EscapeHTML		bool	`json:"escape_html"`
	IndentLinePrefix	string	`json:"indent_line_prefix"`
	IndentPrefix		string	`json:"indent_prefix"`
	Debug			bool	`json:"debug"`
}

type AnswerService struct {
	DatabaseTag	string			`json:"database_tag"`
	pg		*PGDatabase

	BlobCount	uint64			`json:"blob_count"`

	answer		*Answer

	QueryDuration	time.Duration		`json:"query_duration"`
	QueryDurationEnglish	string		`json:"query_duration_english"`
	query_start_time	time.Time
}

type Answer struct {
	StartTime		time.Time	`json:"start_time"`
	WallDuration		time.Duration	`json:"wall_duration"`
	WallDurationEnglish	string		`json:"wall_duration_english"`
	Databases		map[string]*PGDatabase	`json:"databases"`
	AnswerService	map[string]*AnswerService `json:"answer_service"`

	exists	map[string]uint8
	exists_mutex	sync.Mutex

	MissingUdigs		[]string	`json:"missing_udigs"`
	MissingUdigCount	int		`json:"missing_udig_count"`

	ConfigPath	string			`json:"config_path"`
	ConfigBlob	string			`json:"config_blob"`
	GoMemStats	runtime.MemStats	`json:"runtime.MemStats"`
	config		*Config
}

const usage = "usage: jaccard <config-file-path>\n"
var udig_re_graph, udig_re_ascii *regexp.Regexp

var debugging bool

func debug(format string, args ...interface{}) {

	if !debugging {
		return
	}

	fmt.Fprintf(os.Stderr,
		time.Now().Format("2006-01-02 15:04:05") +
		": DEBUG: " +
		format +
		"\n",
		args...,
	)
}

func init() {

	const re_graph = `^[a-z][a-z0-9]{0,7}:[[:graph:]]{1,128}$`
	const re_ascii = `^[a-z][a-z0-9]{0,7}:[[:ascii:]]{1,128}$`

	udig_re_graph = regexp.MustCompile(re_graph)
	udig_re_ascii = regexp.MustCompile(re_ascii)
}

func leave(exit_status int) {
	debug("good bye, cruel world")
	os.Exit(exit_status)
}

func die(format string, args ...interface{}) {

	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...);
	os.Stderr.Write([]byte(usage))
	leave(1)
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
	if conf.WitnessBlob == "" {
		return
	}
	if !udig_re_graph.Match([]byte(conf.WitnessBlob)) {
		die("source udig does not match graphical udig")
	}
	if !udig_re_ascii.Match([]byte(conf.WitnessBlob)) {
		die("source udig does not match ascii udig")
	}
}

func (conf *Config) open() {

	done := make(chan *PGDatabase)
	for _, pg := range conf.Databases {
		go pg.open(done)
	}

	//  insure system ids for databases are distinct
	sid := make(map[string]string, len(conf.Databases)) 
	for cnt := len(conf.Databases);  cnt > 0;  cnt-- {

		//  Note:  do we need a timeout?
		pg := <- done
		if sid[pg.SystemIdentifier] != "" {
			pg.die(
				"duplicate system identifier: %s->%s",
				sid[pg.SystemIdentifier],
				pg.SystemIdentifier,
			)
		}
		sid[pg.SystemIdentifier] = pg.tag
	}
	if len(sid) != len(conf.Databases) {
		die("count of distinct system identifiers != database count")
	}
}

func (pg *PGDatabase) close(done chan bool) {

	debug("closing database: %s", pg.tag)
	pg.Stats = pg.db.Stats()
	err := pg.db.Close()
	if err != nil {
		pg.die("sql.Db.Close() failed: %s", err)
	}
	debug("closed database: %s", pg.tag)
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

	debug("opening database: %s", pg.tag)
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
	debug("database opened: %s", pg.tag)

	debug("fetching pg system id: %s", pg.tag)
	//  insure each database has unque system identifier
	err = pg.db.QueryRow(
		`
SELECT
	system_identifier || '-' || db.oid
  FROM
  	pg_control_system(),
	pg_database db
  WHERE
  	db.datname = $1
;
	`, pg.PGDATABASE).Scan(&pg.SystemIdentifier)
	if err != nil {
		pg.die("db.Query(system_identifier) failed: %s", err)
	}
	if len(pg.SystemIdentifier) == 0 {
		pg.die("empty system identifier")
	}
	debug("pg database id: %s: %s", pg.tag, pg.SystemIdentifier)
	done <- pg
}

func (as *AnswerService) select_service(done chan *PGDatabase) {

	pg := as.pg

	_debug := func(format string, args ...interface{}) {
		if !debugging {
			return
		}
		debug(pg.tag + ": select service: " + format, args...)
	}
	_debug("selecting all blobs in service table: %s", pg.tag)

	as.query_start_time = time.Now()
	//  insure each database has unique system identifier
	rows, err := pg.db.Query(
		`
SELECT
	blob
  FROM
  	blobio.service
	`)
	if err != nil {
		pg.die("db.Query(service) failed: %s", err)
	}
	defer rows.Close()

	for rows.Next() {
		var blob string

		if err := rows.Scan(&blob); err != nil {
			pg.die("rows.Scan(blob) failed: %s", err)
		}
		as.BlobCount++
		if as.BlobCount % 100000 == 0 {
			_debug("blob count: #%d", as.BlobCount)
		}

		an := as.answer
		an.exists_mutex.Lock()
		an.exists[blob]++
		if an.exists[blob] == uint8(len(an.Databases)) {
			delete(an.exists, blob)
		}
		an.exists_mutex.Unlock()
	}
	as.QueryDuration = time.Since(as.query_start_time)
	as.QueryDurationEnglish = as.QueryDuration.String()
	if as.BlobCount % 100000 != 0 {
		_debug("blob: total #%d", as.BlobCount)
	}
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

	//  wait for all the service queries to finish
	for cnt := len(an.AnswerService);  cnt > 0;  cnt-- {
		<- done
	}
	debug("all service queries done")

	//  merge the missing blobs into a single set
	debug("build merge set of missing blobs")
	an.MissingUdigs = make([]string, len(an.exists))
	an.MissingUdigCount = 0;
	for b, _ := range an.exists {
		an.MissingUdigs[an.MissingUdigCount] = b;
		an.MissingUdigCount++
	}
	debug("missing udig count: %d blobs", an.MissingUdigCount)
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
	debugging = conf.Debug
	debug("hello, world")

	conf.frisk()

	conf.open()

	answer := &Answer {
		StartTime:	now,
		Databases:	conf.Databases,
		config:		&conf,
		ConfigPath:	os.Args[1],
		ConfigBlob:	fmt.Sprintf(
					"sha:%x",
					sha1.Sum(cf_buf),
				),
		exists:		make(map[string]uint8),
	}
	answer.jaccard()
	conf.close()

	//  write out answer as json
	enc := json.NewEncoder(os.Stdout)
	enc.SetEscapeHTML(conf.EscapeHTML)
	enc.SetIndent(conf.IndentLinePrefix, conf.IndentPrefix)

	answer.WallDuration = time.Since(answer.StartTime)
	answer.WallDurationEnglish = answer.WallDuration.String()
	runtime.ReadMemStats(&answer.GoMemStats)
	err = enc.Encode(&answer)
	if err != nil {
		die("json.Encode(answer) failed: %s", err)
	}
	leave(0)
}
