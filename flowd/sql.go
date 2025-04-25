//Synopsis:
//	Flow ops for PostgreSQL queries.

package main

import (
	"database/sql"
	"github.com/lib/pq"
	"reflect"
	"regexp"

	. "fmt"
	. "time"
)

type sql_database struct {
	//  database tag name in flow file.  not name on database server
	name string

	//  postgresql, mysql
	driver_name string

	//  url string to connect to database
	data_source_name string

	max_idle_conns int
	max_open_conns int

	opendb *sql.DB

	prepared map[string]sql.Stmt
}

//  Note: ought to be an anonymous struct in struct sql_query_row!
type sql_query_result_row struct {
	name    string
	go_kind reflect.Kind
	offset  uint8
}

//  query a single row from sql database
type sql_query_row struct {
	name      string
	statement string

	result_row  []sql_query_result_row
	name2result map[string]*sql_query_result_row

	*sql_database

	//  prepared statement
	stmt *sql.Stmt

	//  sqlstate codes to be classified as OK in query detail record
	sqlstate_OK map[string]bool

	called bool

	depend_ref_count uint8
}

//  execute an sql query with no rows
type sql_exec struct {
	name      string
	statement []string

	*sql_database

	//  prepared statements
	stmt []*sql.Stmt

	//  sqlstate codes to be classified as OK in query detail record
	sqlstate_OK map[string]bool

	called bool

	depend_ref_count uint8
}

func (q *sql_query_row) query_row(argv []string) (qv *qdr_value) {

	qv = &qdr_value{
		qdr: &qdr{
			termination_class: "OK",
			sqlstate:          "00000",
		},
		results: make([]interface{}, len(q.result_row)),
	}

	die := func(format string, args ...interface{}) {
		format = "sql query row: " + q.name + ": " + format
		msg := Sprintf(format, args...)
		panic(msg)
	}

	//  allocate result slice of return values
	rargv := qv.results
	for i, rr := range q.result_row {

		switch rr.go_kind {
		case reflect.Bool:
			rargv[i] = new(sql.NullBool)
		case reflect.Int64:
			rargv[i] = new(sql.NullInt64)
		case reflect.String:
			rargv[i] = new(sql.NullString)
		default:
			die("impossible row go_kind")
		}
	}

	qargv := make([]interface{}, len(argv))
	for i := 0; i < len(argv); i++ {
		qargv[i] = argv[i]
	}

	//  ought to be measured by the server
	start_time := Now()
	qv.err = q.stmt.QueryRow(qargv...).Scan(rargv...)
	qv.query_duration = Since(start_time)

	switch {
	case sql.ErrNoRows == qv.err:
		qv.err = nil
		qv.sqlstate = "02000"
	case qv.err == nil:
		qv.rows_affected = 1
	default:
		//  postgres specific until database/sql adds proper
		//  sqlstate interface

		if e, ok := qv.err.(*pq.Error); ok {
			OK := q.sqlstate_OK
			qv.sqlstate = string(e.Code)

			if OK == nil || !OK[qv.sqlstate] {
				qv.termination_class = "ERR"
				qv.err_driver = Sprintf("%s/%s",
					e.Code.Name(),
					e.Message,
				)
			} else {
				qv.termination_class = "OK"
				qv.err = nil
			}
		} else {
			die("unknown pg error: %s", qv.err)
		}
	}
	return qv
}

func (ex *sql_exec) exec(argv []string) (qv *qdr_value) {

	die := func(format string, args ...interface{}) {
		panic(Sprintf("sql exec: %s: %s", ex.name,
			Sprintf(format, args...)))
	}

	qv = &qdr_value{
		qdr: &qdr{
			termination_class: "OK",
			sqlstate:          "00000",
		},
	}

	qargv := make([]interface{}, len(argv))
	for i := 0; i < len(argv); i++ {
		qargv[i] = argv[i]
	}

	var res sql.Result

	//  ought to be measured on the database backend server
	start_time := Now()
	res, qv.err = ex.stmt[0].Exec(qargv...)
	qv.query_duration = Since(start_time)

	//  parse errors and set termination class/code
	switch {
	case sql.ErrNoRows == qv.err:
		qv.err = nil
		qv.sqlstate = "02000"
	case qv.err == nil:
		var er error

		qv.rows_affected, er = res.RowsAffected()
		if er != nil {
			die("%s", er)
		}
	default:
		//  postgres specific until database/sql adds proper
		//  sqlstate interface

		if e, ok := qv.err.(*pq.Error); ok {
			OK := ex.sqlstate_OK
			qv.sqlstate = string(e.Code)

			if OK == nil || !OK[qv.sqlstate] {
				qv.termination_class = "ERR"
				qv.err_driver = Sprintf("%s/%s",
					e.Code.Name(),
					e.Message,
				)
			} else {
				qv.termination_class = "OK"
				qv.err = nil
			}
		} else {
			die("%s", qv.err)
		}
	}

	return qv
}

func (ex *sql_exec) exec_txn(argv []string) (qv *qdr_value) {

	die := func(format string, args ...interface{}) {
		panic(Sprintf("sql exec txn: %s: %s", ex.name,
			Sprintf(format, args...)))
	}

	qv = &qdr_value{
		qdr: &qdr{
			termination_class: "OK",
			sqlstate:          "00000",
		},
	}

	qargv := make([]interface{}, len(argv))
	for i := 0; i < len(argv); i++ {
		qargv[i] = argv[i]
	}

	var res sql.Result

	//  ought to be measured on the database backend server
	start_time := Now()

	tx, err := ex.opendb.Begin()
	if err != nil {
		die("%s", err)
	}

	defer func() {
		if tx != nil {
			tx.Rollback()
		}
	}()

	for i, stmt := range ex.stmt {
		st := tx.Stmt(stmt)
		defer st.Close()

		if i+1 == len(ex.stmt) {
			res, qv.err = st.Exec(qargv...)
		} else {
			res, qv.err = st.Exec()
		}

		//  parse errors and set termination class/code
		switch {
		case sql.ErrNoRows == qv.err:
			qv.err = nil
			qv.sqlstate = "02000"
		case qv.err == nil:
			var er error

			qv.rows_affected, er = res.RowsAffected()
			if er != nil {
				die("%s", er)
			}
		default:
			//  postgres specific until database/sql adds proper
			//  sqlstate interface

			if e, ok := qv.err.(*pq.Error); ok {
				OK := ex.sqlstate_OK
				qv.sqlstate = string(e.Code)

				if OK == nil || !OK[qv.sqlstate] {
					qv.termination_class = "ERR"
					qv.err_driver = Sprintf("%s/%s",
						e.Code.Name(),
						e.Message,
					)
				} else {
					qv.termination_class = "OK"
					qv.err = nil
				}
			} else {
				die("%s", qv.err)
			}
		}
		if qv.termination_class != "OK" {
			break
		}
	}
	if qv.termination_class == "OK" {
		err = tx.Commit()
	} else {
		err = tx.Rollback()
	}
	tx = nil
	if err != nil {
		die("%s", err)
	}
	qv.query_duration = Since(start_time)

	return qv
}

var sqlstate_re *regexp.Regexp

func init() {
	var err error

	// verify sqlstate code
	sqlstate_re, err = regexp.Compile("^[A-Z0-9]{5}$")
	if err != nil {
		panic(err)
	}
	if err != nil {
		panic(err)
	}
}

func is_sqlstate(s string) bool {

	return sqlstate_re.MatchString(s)
}
