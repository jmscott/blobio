//line parser.y:46
package main

import __yyfmt__ "fmt"

//line parser.y:46

import (
	"errors"
	"io"
	"reflect"
	"regexp"
	"strconv"
	"unicode"
	"unicode/utf8"

	. "fmt"
	. "time"
)

func init() {
	if yyToknames[3] != "__MIN_YYTOK" {
		panic("yyToknames[3] != __MIN_YYTOK: yacc may have changed")
	}
}

//  describes behavior of brr flow server

type config struct {
	//  path to config file
	path string

	//  maximum number of blob requests records in queue
	brr_capacity uint16

	//  maximum number of flows competing for blob request records
	flow_worker_count uint16

	//  maximum number of worker requests in queue
	os_exec_capacity uint16

	//  number of workers competing for requests
	os_exec_worker_count uint16

	//  duration between rolls of os exec detail record logs
	xdr_roll_duration Duration

	//  duration between rolls of query detail record logs
	qdr_roll_duration Duration

	//  duration between rolls of flow detail record logs
	fdr_roll_duration Duration

	//  duration between any heartbeat in log file
	heartbeat_duration Duration

	//  duration between calling runtime.ReadMemStats()
	memstats_duration Duration

	//  sole tail
	*tail

	//  sql database <name>
	sql_database map[string]*sql_database

	//  sql query <name> row (...)
	sql_query_row map[string]*sql_query_row

	//  sql exec <name>
	sql_exec map[string]*sql_exec

	//  all os commands{}
	command map[string]*command

	//  defaults to "log"
	log_directory string
}

//  abstract syntax tree that represents the config file
type ast struct {
	yy_tok int
	bool
	string
	regexp *regexp.Regexp
	uint64
	uint8
	brr_field
	*call
	*tail
	*command
	*sql_database
	*sql_query_row
	*sql_exec

	//  children
	left  *ast
	right *ast

	//  siblings
	next *ast
}

type parse struct {

	//  configuration file the parse is derived from
	*config

	ast_root *ast

	//  dependency order of call() and query() nodes
	depend_order []string

	//  map of a call() statements to the branch of it's abstract
	//  syntax tree

	call2ast map[string]*ast

	//  map of a query() statements to the branch of it's abstract
	//  syntax tree

	query2ast map[string]*ast
}

const (
	max_argv         = 16
	max_command_name = 128

	max_name_rune_count = 64
)

//line parser.y:174
type yySymType struct {
	yys int
	uint64
	string
	tail          *tail
	command       *command
	call          *call
	sql_database  *sql_database
	sql_query_row *sql_query_row
	sql_exec      *sql_exec
	string_list   []string
	ast           *ast
	brr_field     brr_field
	go_kind       reflect.Kind
}

const __MIN_YYTOK = 57346
const ARGV = 57347
const ARGV0 = 57348
const ARGV1 = 57349
const BLOB_SIZE = 57350
const BOOT = 57351
const BRR_CAPACITY = 57352
const CALL = 57353
const CALL0 = 57354
const CAST_STRING = 57355
const CAST_UINT64 = 57356
const CHAT_HISTORY = 57357
const COMMAND = 57358
const COMMAND_REF = 57359
const DATABASE = 57360
const DATA_SOURCE_NAME = 57361
const TRACE_BOOL = 57362
const DRIVER_NAME = 57363
const EQ = 57364
const MATCH = 57365
const EQ_BOOL = 57366
const EQ_STRING = 57367
const MATCH_STRING = 57368
const EQ_UINT64 = 57369
const EXIT_STATUS = 57370
const FDR_ROLL_DURATION = 57371
const FLOW_WORKER_COUNT = 57372
const FROM = 57373
const HEARTBEAT_DURATION = 57374
const IN = 57375
const IS = 57376
const LOG_DIRECTORY = 57377
const MAX_IDLE_CONNS = 57378
const MAX_OPEN_CONNS = 57379
const MEMSTAT_DURATION = 57380
const NAME = 57381
const NEQ = 57382
const NO_MATCH = 57383
const NEQ_BOOL = 57384
const NEQ_STRING = 57385
const NO_MATCH_STRING = 57386
const NEQ_UINT64 = 57387
const NETFLOW = 57388
const OS_EXEC_CAPACITY = 57389
const OS_EXEC_WORKER_COUNT = 57390
const PARSE_ERROR = 57391
const PATH = 57392
const PROJECT_BRR = 57393
const PROJECT_QDR_ROWS_AFFECTED = 57394
const PROJECT_QDR_SQLSTATE = 57395
const PROJECT_SQL_QUERY_ROW_BOOL = 57396
const PROJECT_XDR_EXIT_STATUS = 57397
const QDR_ROLL_DURATION = 57398
const QUERY = 57399
const QUERY_DURATION = 57400
const QUERY_EXEC = 57401
const QUERY_EXEC_TXN = 57402
const QUERY_ROW = 57403
const RESULT = 57404
const ROW = 57405
const ROWS_AFFECTED = 57406
const SQL = 57407
const SQL_DATABASE = 57408
const SQL_DATABASE_REF = 57409
const SQL_EXEC_REF = 57410
const SQL_QUERY_ROW_REF = 57411
const SQLSTATE = 57412
const START_TIME = 57413
const STATEMENT = 57414
const STRING = 57415
const TAIL = 57416
const TAIL_REF = 57417
const TRANSACTION = 57418
const UDIG = 57419
const UINT64 = 57420
const VERB = 57421
const WALL_DURATION = 57422
const WHEN = 57423
const XDR_ROLL_DURATION = 57424
const yy_AND = 57425
const yy_BOOL = 57426
const yy_EXEC = 57427
const yy_FALSE = 57428
const yy_INT64 = 57429
const yy_OK = 57430
const yy_OR = 57431
const yy_TRUE = 57432

var yyToknames = [...]string{
	"$end",
	"error",
	"$unk",
	"__MIN_YYTOK",
	"ARGV",
	"ARGV0",
	"ARGV1",
	"BLOB_SIZE",
	"BOOT",
	"BRR_CAPACITY",
	"CALL",
	"CALL0",
	"CAST_STRING",
	"CAST_UINT64",
	"CHAT_HISTORY",
	"COMMAND",
	"COMMAND_REF",
	"DATABASE",
	"DATA_SOURCE_NAME",
	"TRACE_BOOL",
	"DRIVER_NAME",
	"EQ",
	"MATCH",
	"EQ_BOOL",
	"EQ_STRING",
	"MATCH_STRING",
	"EQ_UINT64",
	"EXIT_STATUS",
	"FDR_ROLL_DURATION",
	"FLOW_WORKER_COUNT",
	"FROM",
	"HEARTBEAT_DURATION",
	"IN",
	"IS",
	"LOG_DIRECTORY",
	"MAX_IDLE_CONNS",
	"MAX_OPEN_CONNS",
	"MEMSTAT_DURATION",
	"NAME",
	"NEQ",
	"NO_MATCH",
	"NEQ_BOOL",
	"NEQ_STRING",
	"NO_MATCH_STRING",
	"NEQ_UINT64",
	"NETFLOW",
	"OS_EXEC_CAPACITY",
	"OS_EXEC_WORKER_COUNT",
	"PARSE_ERROR",
	"PATH",
	"PROJECT_BRR",
	"PROJECT_QDR_ROWS_AFFECTED",
	"PROJECT_QDR_SQLSTATE",
	"PROJECT_SQL_QUERY_ROW_BOOL",
	"PROJECT_XDR_EXIT_STATUS",
	"QDR_ROLL_DURATION",
	"QUERY",
	"QUERY_DURATION",
	"QUERY_EXEC",
	"QUERY_EXEC_TXN",
	"QUERY_ROW",
	"RESULT",
	"ROW",
	"ROWS_AFFECTED",
	"SQL",
	"SQL_DATABASE",
	"SQL_DATABASE_REF",
	"SQL_EXEC_REF",
	"SQL_QUERY_ROW_REF",
	"SQLSTATE",
	"START_TIME",
	"STATEMENT",
	"STRING",
	"TAIL",
	"TAIL_REF",
	"TRANSACTION",
	"UDIG",
	"UINT64",
	"VERB",
	"WALL_DURATION",
	"WHEN",
	"XDR_ROLL_DURATION",
	"yy_AND",
	"yy_BOOL",
	"yy_EXEC",
	"yy_FALSE",
	"yy_INT64",
	"yy_OK",
	"yy_OR",
	"yy_TRUE",
	"'='",
	"';'",
	"','",
	"'.'",
	"'('",
	"')'",
	"'{'",
	"'}'",
}
var yyStatenames = [...]string{}

const yyEofCode = 1
const yyErrCode = 2
const yyInitialStackSize = 16

//line parser.y:2033

var keyword = map[string]int{
	"and":                  yy_AND,
	"argv":                 ARGV,
	"blob_size":            BLOB_SIZE,
	"bool":                 yy_BOOL,
	"boot":                 BOOT,
	"brr_capacity":         BRR_CAPACITY,
	"call":                 CALL,
	"chat_history":         CHAT_HISTORY,
	"command":              COMMAND,
	"database":             DATABASE,
	"data_source_name":     DATA_SOURCE_NAME,
	"driver_name":          DRIVER_NAME,
	"exec":                 yy_EXEC,
	"exit_status":          EXIT_STATUS,
	"false":                yy_FALSE,
	"fdr_roll_duration":    FDR_ROLL_DURATION,
	"flow_worker_count":    FLOW_WORKER_COUNT,
	"heartbeat_duration":   HEARTBEAT_DURATION,
	"in":                   IN,
	"int64":                yy_INT64,
	"is":                   IS,
	"log_directory":        LOG_DIRECTORY,
	"max_idle_conns":       MAX_IDLE_CONNS,
	"max_open_conns":       MAX_OPEN_CONNS,
	"memstats_duration":    MEMSTAT_DURATION,
	"netflow":              NETFLOW,
	"OK":                   yy_OK,
	"or":                   yy_OR,
	"os_exec_capacity":     OS_EXEC_CAPACITY,
	"os_exec_worker_count": OS_EXEC_WORKER_COUNT,
	"path":                 PATH,
	"qdr_roll_duration":    QDR_ROLL_DURATION,
	"query_duration":       QUERY_DURATION,
	"query":                QUERY,
	"result":               RESULT,
	"row":                  ROW,
	"rows_affected":        ROWS_AFFECTED,
	"sql":                  SQL,
	"sqlstate":             SQLSTATE,
	"start_time":           START_TIME,
	"statement":            STATEMENT,
	"tail":                 TAIL,
	"true":                 yy_TRUE,
	"udig":                 UDIG,
	"verb":                 VERB,
	"wall_duration":        WALL_DURATION,
	"when":                 WHEN,
	"xdr_roll_duration":    XDR_ROLL_DURATION,
}

type yyLexState struct {
	config *config

	//  command being actively parsed, nil when not in 'command {...}'
	*command

	//  call being actively parsed, nil when not in 'call (...) when ...'
	*call

	*sql_database
	*sql_query_row
	*sql_exec

	in      io.RuneReader //  config source stream
	line_no uint64        //  lexical line number
	eof     bool          //  seen eof in token stream
	peek    rune          //  lookahead in lexer
	err     error

	//  various parsing boot states.

	in_boot   bool
	seen_boot bool

	seen_brr_capacity         bool
	seen_data_source_name     bool
	seen_driver_name          bool
	seen_fdr_roll_duration    bool
	seen_flow_worker_count    bool
	seen_heartbeat_duration   bool
	seen_max_idle_conns       bool
	seen_max_open_conns       bool
	seen_memstats_duration    bool
	seen_os_exec_capacity     bool
	seen_os_exec_worker_count bool
	seen_qdr_roll_duration    bool
	seen_xdr_roll_duration    bool

	ast_root *ast

	depends   []string
	call2ast  map[string]*ast
	query2ast map[string]*ast
}

func (l *yyLexState) pushback(c rune) {

	if l.peek != 0 {
		panic("pushback(): push before peek") /* impossible */
	}
	l.peek = c
	if c == '\n' {
		l.line_no--
	}
}

/*
 *  Read next UTF8 rune.
 */
func (l *yyLexState) get() (c rune, eof bool, err error) {

	if l.eof {
		return 0, true, nil
	}
	if l.peek != 0 { /* returned stashed char */
		c = l.peek
		/*
		 *  Only pushback 1 char.
		 */
		l.peek = 0
		if c == '\n' {
			l.line_no++
		}
		return c, false, nil
	}
	c, _, err = l.in.ReadRune()
	if err != nil {
		if err == io.EOF {
			l.eof = true
			return 0, true, nil
		}
		return 0, false, err
	}
	if c == unicode.ReplacementChar {
		return 0, false, l.mkerror("get: invalid unicode sequence")
	}
	if c == '\n' {
		l.line_no++
	}
	return c, false, nil
}

func lookahead(l *yyLexState, peek rune, ifyes int, ifno int) (int, error) {

	next, eof, err := l.get()
	if err != nil {
		return 0, err
	}
	if next == peek {
		return ifyes, err
	}
	if !eof {
		l.pushback(next)
	}
	return ifno, nil
}

/*
 *  Skip '#' comment.
 *  The scan stops on the terminating newline or error
 */
func skip_comment(l *yyLexState) (err error) {
	var c rune
	var eof bool

	/*
	 *  Scan for newline, end of file, or error.
	 */
	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {
		if c == '\n' {
			return
		}
	}
	return err

}

func skip_space(l *yyLexState) (c rune, eof bool, err error) {

	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {
		if unicode.IsSpace(c) {
			continue
		}
		if c != '#' {
			return c, false, nil
		}

		/*
		 *  Skipping over # comment terminated by newline or EOF
		 */
		err = skip_comment(l)
		if err != nil {
			return 0, false, err
		}
	}
	return 0, eof, err
}

/*
 *  Words are a sequence of ascii letters, digits/numbers and '_' characters.
 *  The word is mapped onto first a keyword, then command, then query and,
 *  finally, then tail.
 */
func (l *yyLexState) scan_word(yylval *yySymType, c rune) (tok int, err error) {
	var eof bool

	w := string(c)
	count := 1

	/*
	 *  Scan a string of ascii letters, numbers/digits and '_' character.
	 */
	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {
		if c > 127 ||
			(c != '_' &&
				!unicode.IsLetter(c) &&
				!unicode.IsNumber(c)) {
			break
		}
		count++
		if count > max_command_name {
			return 0, l.mkerror("name too many characters: max=%d",
				max_command_name)
		}
		w += string(c)
	}
	if err != nil {
		return 0, err
	}
	if !eof {
		l.pushback(c) /* first character after word */
	}

	if keyword[w] > 0 { /* got a keyword */
		return keyword[w], nil /* return yacc generated token */
	}

	if utf8.RuneCountInString(w) > max_name_rune_count {
		return PARSE_ERROR, l.mkerror(
			"name too long: expected %d chars, got %d",
			max_name_rune_count, utf8.RuneCountInString(w),
		)
	}

	//  command reference?
	if c := l.config.command[w]; c != nil {
		yylval.command = c
		return COMMAND_REF, nil
	}

	//  tail file reference?
	if l.config.tail != nil && w == l.config.tail.name {
		yylval.tail = l.config.tail
		return TAIL_REF, nil
	}

	//  sql query reference?
	if q := l.config.sql_query_row[w]; q != nil {
		yylval.sql_query_row = q
		return SQL_QUERY_ROW_REF, nil
	}

	//  sql exec reference?
	if ex := l.config.sql_exec[w]; ex != nil {
		yylval.sql_exec = ex
		return SQL_EXEC_REF, nil
	}

	//  sql database reference?

	if db := l.config.sql_database[w]; db != nil {
		yylval.sql_database = db
		return SQL_DATABASE_REF, nil
	}

	if l.in_boot {
		return PARSE_ERROR, l.mkerror("unknown variable in boot{}: '%s'",
			w)
	}

	/*
	 *  Note:
	 *	Need to insure first letter is not a digit!
	 */
	yylval.string = w
	return NAME, nil
}

func (l *yyLexState) scan_uint64(yylval *yySymType, c rune) (err error) {
	var eof bool

	ui64 := string(c)
	count := 1

	/*
	 *  Scan a string of unicode numbers/digits and let Scanf parse the
	 *  actual digit string.
	 */
	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {
		count++
		if count > 20 {
			return l.mkerror("uint64 > 20 digits")
		}
		if c > 127 || !unicode.IsNumber(c) {
			break
		}
		ui64 += string(c)
	}
	if err != nil {
		return
	}
	if !eof {
		l.pushback(c) //  first character after ui64
	}

	yylval.uint64, err = strconv.ParseUint(ui64, 10, 64)
	return
}

/*
 *  Very simple utf8 string scanning, with no proper escapes for characters.
 *  Expect this module to be replaced with correct text.Scanner.
 */
func (l *yyLexState) scan_string(yylval *yySymType) (eof bool, err error) {
	var c rune
	s := ""

	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {
		if c == '"' {
			yylval.string = s
			return false, nil
		}
		switch c {
		case '\n':
			return false, l.mkerror("new line in string")
		case '\r':
			return false, l.mkerror("carriage return in string")
		case '\t':
			return false, l.mkerror("tab in string")
		case '\\':
			return false, l.mkerror("backslash in string")
		}
		s += string(c)
	}
	if err != nil {
		return false, err
	}
	return true, nil
}

/*
 *  Scan an almost raw string as defined in golang.
 *  Carriage return is stripped.
 */
func (l *yyLexState) scan_raw_string(yylval *yySymType) (eof bool, err error) {
	var c rune
	s := ""

	/*
	 *  Scan a raw string of unicode letters, accepting all but `
	 */
	for c, eof, err = l.get(); !eof && err == nil; c, eof, err = l.get() {

		switch c {
		case '\r':
			//  why does go skip carriage return?  raw is not so raw
			continue
		case '`':
			yylval.string = s
			return false, nil
		}
		s += string(c)
	}
	if err != nil {
		return false, err
	}
	return true, nil
}

func (l *yyLexState) Lex(yylval *yySymType) (tok int) {

	if l.err != nil {
		return PARSE_ERROR
	}
	if l.eof {
		return 0
	}
	c, eof, err := skip_space(l)
	if err != nil {
		goto PARSE_ERROR
	}
	if eof {
		return 0
	}

	//  ascii outside of strings, for time being
	if c > 127 {
		goto PARSE_ERROR
	}
	/*
	 *  switch(c) statement?
	 */
	if (unicode.IsLetter(c)) || c == '_' {
		tok, err = l.scan_word(yylval, c)
		if err != nil {
			goto PARSE_ERROR
		}
		return tok
	}
	if unicode.IsNumber(c) {
		err = l.scan_uint64(yylval, c)
		if err != nil {
			goto PARSE_ERROR
		}
		return UINT64
	}
	if c == '"' {
		lno := l.line_no // reset line number on error

		eof, err = l.scan_string(yylval)
		if err != nil {
			goto PARSE_ERROR
		}
		if eof {
			l.line_no = lno
			err = l.mkerror("unexpected end of file in string")
			goto PARSE_ERROR
		}
		return STRING
	}
	if c == '`' {
		lno := l.line_no // reset line number on error

		eof, err = l.scan_raw_string(yylval)
		if err != nil {
			goto PARSE_ERROR
		}
		if eof {
			l.line_no = lno
			err = l.mkerror("unexpected end of file in raw string")
			goto PARSE_ERROR
		}
		return STRING
	}
	if c == '=' {

		//  string equality: ==
		tok, err = lookahead(l, '=', EQ, 0)
		if err != nil {
			goto PARSE_ERROR
		}
		if tok == EQ {
			return tok
		}

		//  match regular expression: =~

		tok, err = lookahead(l, '~', MATCH, '=')
		if err != nil {
			goto PARSE_ERROR
		}
		return tok
	}
	if c == '!' {

		//  string inequality: !=

		tok, err = lookahead(l, '=', NEQ, 0)
		if err != nil {
			goto PARSE_ERROR
		}
		if tok == NEQ {
			return tok
		}

		//  regular expression not matches: !~

		tok, err = lookahead(l, '~', NO_MATCH, '=')
		if err != nil {
			goto PARSE_ERROR
		}
		return tok
	}
	return int(c)
PARSE_ERROR:
	l.err = err
	return PARSE_ERROR
}

func (l *yyLexState) mkerror(format string, args ...interface{}) error {

	return errors.New(Sprintf("%s, near line %d",
		Sprintf(format, args...),
		l.line_no,
	))
}

func (l *yyLexState) error(format string, args ...interface{}) {

	l.Error(Sprintf(format, args...))
}

func (l *yyLexState) Error(msg string) {

	if l.err == nil { //  only report first error
		l.err = l.mkerror("%s", msg)
	}
}

//  wire in posible casts and verify correctness of expression

func (l *yyLexState) wire_rel_op(left, op, right *ast) bool {

	//  verify the comparisons make sense

	//  verify that the PROJECT_BRR or command.exit_status
	//  are compared to a constant of a matching type.

	if left.yy_tok == PROJECT_BRR {

		//  if the brr field is a blob size compared to a
		//  uint, then reparent the with a CAST_UINT64 node

		if left.brr_field == brr_BLOB_SIZE {
			if right.yy_tok != UINT64 {
				l.error(
					"%s.blob_size not compared to uint64",
					left.tail.name)
				return false
			}
			left = &ast{
				yy_tok: CAST_UINT64,
				left:   left,
			}
			if op.yy_tok == EQ {
				op.yy_tok = EQ_UINT64
			} else {
				op.yy_tok = NEQ_UINT64
			}
			op.uint64 = right.uint64
		} else {
			if right.yy_tok != STRING {
				l.error("%s.%s not compared to string",
					left.tail.name, left.brr_field)
				return false
			}

			var err error

			switch op.yy_tok {
			case EQ:
				op.yy_tok = EQ_STRING
				op.string = right.string
			case NEQ:
				op.yy_tok = NEQ_STRING
				op.string = right.string
			case MATCH:
				op.yy_tok = MATCH_STRING
				op.regexp = right.regexp
				op.regexp, err = regexp.Compile(right.string)
			case NO_MATCH:
				op.yy_tok = NO_MATCH_STRING
				op.regexp = right.regexp
				op.regexp, err = regexp.Compile(right.string)
			default:
				panic("impossible relop")
			}
			if err != nil {
				l.error(
					"can not compile regexp: %s", right.string)
				return false
			}
		}
	} else {
		//  <command>.exit_status == ...

		if right.yy_tok != UINT64 {
			l.error("%s.exit_status not compared to uint64",
				left.string)
			return false
		}
		if op.yy_tok == EQ {
			op.yy_tok = EQ_UINT64
		} else {
			op.yy_tok = NEQ_UINT64
		}
		op.uint64 = right.uint64
	}
	return true
}

func (conf *config) parse(in io.RuneReader) (
	par *parse,
	err error,
) {
	l := &yyLexState{
		config:    conf,
		line_no:   1,
		in:        in,
		eof:       false,
		err:       nil,
		depends:   make([]string, 0),
		call2ast:  make(map[string]*ast),
		query2ast: make(map[string]*ast),
	}
	yyParse(l)
	err = l.err
	if err != nil {
		return
	}

	//  make sure a tail is defined
	if conf.tail == nil {
		return nil, errors.New("tail{} not defined")
	}

	root_depend := make(map[string]bool)

	//  added unreferenced calls() to root depend list
	var find_unreferenced_fire func(a *ast)

	find_unreferenced_fire = func(a *ast) {
		var name string

		switch {
		case a == nil:
			return

		case a.yy_tok == CALL:
			cmd := a.call.command
			if cmd.called && cmd.depend_ref_count == 0 {
				name = cmd.name
			}
		case a.yy_tok == QUERY_ROW:
			q := a.sql_query_row
			if q.called && q.depend_ref_count == 0 {
				name = q.name
			}
		case a.yy_tok == QUERY_EXEC || a.yy_tok == QUERY_EXEC_TXN:
			q := a.sql_exec
			if q.called && q.depend_ref_count == 0 {
				name = q.name
			}
		}
		if name != "" {
			root_depend[name] = true
		}
		find_unreferenced_fire(a.left)
		find_unreferenced_fire(a.right)
		find_unreferenced_fire(a.next)
	}
	find_unreferenced_fire(l.ast_root)

	for n, _ := range root_depend {
		l.depends = append(l.depends, Sprintf("%s %s", n, n))
	}

	//  tsort the call/query dependencies.
	//  reverse the order so roots are first

	depend_order := tsort(l.depends)
	if depend_order == nil {
		err = errors.New("cycle exists between call/query graphs")
	} else {
		for i, j := 0, len(depend_order)-1; i < j; i, j = i+1, j-1 {
			depend_order[i], depend_order[j] =
				depend_order[j], depend_order[i]
		}
	}
	return &parse{
		config:       conf,
		ast_root:     l.ast_root,
		depend_order: depend_order,
		call2ast:     l.call2ast,
		query2ast:    l.query2ast,
	}, err
}

func (a *ast) to_string(brief bool) string {

	var what string

	switch a.yy_tok {
	case COMMAND:
		what = Sprintf("COMMAND(%s)", a.command.name)
	case CALL0:
		what = Sprintf("CALL0(%s)", a.call.command.name)
	case CALL:
		what = Sprintf("CALL(%s)", a.call.command.name)
	case EQ_STRING:
		what = Sprintf("EQ_STRING(\"%s\")", a.string)
	case TAIL:
		what = Sprintf("TAIL(%s)", a.tail.name)
	case TAIL_REF:
		what = Sprintf("TAIL_REF(%s.%s)", a.tail.name, a.brr_field)
	case COMMAND_REF:
		what = Sprintf("COMMAND_REF(%s)", a.command.name)
	case UINT64:
		what = Sprintf("UINT64(%d)", a.uint64)
	case STRING:
		what = Sprintf("STRING(\"%s\")", a.string)
	case ARGV:
		what = Sprintf("ARGV(%d)", a.uint64)
	case SQL_DATABASE:
		what = Sprintf("SQL_DATABASE(%s:%s)",
			a.sql_database.driver_name,
			a.sql_database.name,
		)
	case SQL_DATABASE_REF:
		what = Sprintf("SQL_DATABASE_REF(%s)",
			a.sql_database.name,
		)
	case SQL_QUERY_ROW_REF:
		what = Sprintf("SQL_QUERY_ROW_REF(%s.%s)",
			a.sql_query_row.sql_database.name,
			a.sql_query_row.name,
		)
	case SQL_EXEC_REF:
		what = Sprintf("SQL_EXEC_REF(%s.%s)",
			a.sql_exec.sql_database.name,
			a.sql_exec.name,
		)
	case QUERY_ROW:
		what = Sprintf("QUERY_ROW(%s)",
			a.sql_query_row.name,
		)
	case QUERY_EXEC:
		what = Sprintf("QUERY_EXEC(%s)",
			a.sql_exec.name,
		)
	case QUERY_EXEC_TXN:
		what = Sprintf("QUERY_EXEC_TXN(%s)",
			a.sql_exec.name,
		)
	case PROJECT_SQL_QUERY_ROW_BOOL:
		what = Sprintf("PROJECT_SQL_QUERY_ROW_BOOL(%s:%d)",
			a.string,
			a.uint8,
		)
	case PROJECT_BRR:
		what = Sprintf("PROJECT_BRR(%s[%d])",
			a.brr_field.String(),
			a.brr_field,
		)
	default:
		offset := a.yy_tok - __MIN_YYTOK + 3
		if a.yy_tok > __MIN_YYTOK {
			what = yyToknames[offset]
		} else {
			what = Sprintf(
				"UNKNOWN(%d)",
				a.yy_tok,
			)
		}
	}
	if brief {
		return what
	}

	what = Sprintf("%s:&=%p", what, a)

	if a.left != nil {
		what = Sprintf("%s,l=%p", what, a.left)
	}
	if a.right != nil {
		what = Sprintf("%s,r=%p", what, a.right)
	}
	if a.next != nil {
		what = Sprintf("%s,n=%p", what, a.next)
	}
	return what
}

func (a *ast) String() string {

	return a.to_string(true)

}

func (a *ast) walk_print(indent int, is_first_sibling bool) {

	if a == nil {
		return
	}
	if indent == 0 {
		Println("")
	} else {
		for i := 0; i < indent; i++ {
			Print("  ")
		}
	}
	Println(a.to_string(true))

	//  print kids
	a.left.walk_print(indent+1, true)
	a.right.walk_print(indent+1, true)

	//  print siblings
	if is_first_sibling {
		for as := a.next; as != nil; as = as.next {
			as.walk_print(indent, false)
		}
	}
}

func (a *ast) print() {
	a.walk_print(0, true)
}

func (a *ast) is_uint64() bool {
	return a.yy_tok == UINT64 ||
		a.yy_tok == PROJECT_XDR_EXIT_STATUS ||
		a.yy_tok == PROJECT_QDR_ROWS_AFFECTED
}

func (a *ast) is_string() bool {
	switch a.yy_tok {
	case STRING:
	case PROJECT_QDR_SQLSTATE:
	default:
		return false
	}
	return true
}

func (a *ast) is_bool() bool {

	switch a.yy_tok {
	case yy_TRUE:
	case yy_FALSE:
	case PROJECT_SQL_QUERY_ROW_BOOL:
	default:
		return false
	}
	return true
}

func (l *yyLexState) put_sqlstate(code string) bool {

	if !is_sqlstate(code) {
		l.error("invalid sqlstate code: %s", code)
		return false
	}

	switch {

	case l.sql_query_row != nil:
		q := l.sql_query_row
		if q.sqlstate_OK == nil {
			q.sqlstate_OK = make(map[string]bool)
		}
		q.sqlstate_OK[code] = true

	case l.sql_exec != nil:
		ex := l.sql_exec
		if ex.sqlstate_OK == nil {
			ex.sqlstate_OK = make(map[string]bool)
		}
		ex.sqlstate_OK[code] = true

	default:
		panic("put_sqlstate: impossible rule")
	}
	return true
}

//line yacctab:1
var yyExca = [...]int{
	-1, 1,
	1, -1,
	-2, 0,
}

const yyPrivate = 57344

const yyLast = 274

var yyAct = [...]int{

	212, 198, 146, 32, 123, 145, 139, 134, 59, 147,
	178, 215, 147, 228, 141, 136, 140, 225, 227, 214,
	144, 217, 226, 40, 216, 122, 121, 54, 62, 65,
	57, 142, 143, 53, 204, 66, 58, 208, 137, 119,
	204, 151, 112, 205, 154, 60, 61, 151, 151, 103,
	153, 152, 206, 149, 56, 29, 149, 28, 196, 95,
	135, 150, 192, 148, 150, 94, 148, 40, 93, 88,
	65, 87, 76, 75, 38, 37, 66, 189, 183, 89,
	55, 41, 65, 19, 193, 179, 201, 181, 66, 188,
	108, 67, 171, 163, 18, 107, 97, 124, 124, 124,
	17, 34, 174, 110, 172, 164, 158, 109, 157, 229,
	155, 16, 128, 98, 168, 167, 166, 165, 159, 102,
	126, 127, 101, 108, 100, 41, 99, 65, 107, 195,
	182, 219, 59, 66, 197, 20, 110, 230, 223, 187,
	109, 156, 86, 186, 130, 162, 221, 147, 170, 81,
	169, 54, 62, 125, 57, 124, 177, 53, 210, 4,
	58, 10, 78, 199, 185, 184, 6, 180, 132, 60,
	61, 131, 170, 77, 21, 129, 116, 175, 56, 25,
	85, 113, 117, 91, 50, 63, 213, 45, 190, 194,
	31, 149, 44, 43, 136, 30, 14, 176, 202, 150,
	173, 148, 22, 161, 55, 82, 114, 11, 27, 26,
	42, 80, 115, 84, 83, 9, 191, 137, 224, 69,
	70, 68, 106, 24, 5, 74, 141, 111, 140, 207,
	118, 203, 3, 92, 96, 12, 120, 72, 71, 135,
	90, 64, 15, 142, 143, 23, 13, 211, 200, 138,
	133, 220, 222, 209, 160, 73, 104, 105, 48, 47,
	46, 49, 1, 218, 79, 33, 36, 8, 7, 39,
	35, 51, 52, 2,
}
var yyPact = [...]int{

	150, -1000, 150, -1000, -1000, 157, -1000, 19, 2, 117,
	206, 140, -1000, -40, -42, 156, -1000, 6, -1000, 6,
	154, 153, 148, -1000, -1000, -1000, -1000, -1000, 122, 135,
	-1000, -1, 197, 197, 6, -1000, -1000, -21, -22, 134,
	-23, -25, -13, -1000, 120, -1000, -27, -30, -36, -2,
	21, 35, 33, 31, -1000, -1000, -1000, -1000, -1000, -1000,
	-1000, -1000, -1000, 28, -48, 6, 6, -1000, 17, -1000,
	-1000, -1000, -1000, 17, -54, 142, 112, -1000, -1000, -1000,
	-1000, -1000, -1000, -1000, -1000, -1000, -1000, 202, -1000, -1000,
	-58, -1000, -71, 50, 50, 50, 20, -1000, -1000, 102,
	66, 98, 95, 189, 44, 44, -1000, -1000, -1000, -1000,
	-1000, -1000, -1000, -1000, -1000, -1000, -1000, -1000, -1000, 207,
	-77, 129, -45, -1000, -1000, -1000, -46, -52, -1000, -1000,
	-1000, -1000, 18, 10, 14, 27, -1000, 169, -5, 13,
	26, 25, 24, 23, 129, -6, 12, 166, 11, 114,
	163, 50, -1000, -1000, -1000, -88, -7, -1000, -1000, 94,
	-4, 42, -14, -1000, -1000, 92, 91, 65, 61, -9,
	-15, -1000, -1000, 149, -11, 155, 41, -1000, -1000, -1000,
	-1000, -37, 53, -1000, -1000, -1000, -1000, -1000, -1000, -1000,
	-1000, -1000, -1000, 90, -1000, 5, 90, 198, -53, -1000,
	-43, 196, -59, -1000, 85, -1000, 147, -78, -1000, -86,
	-1000, -72, -1000, 47, 73, 60, -1000, 147, -1000, -1000,
	-76, -1000, -80, -1000, -1000, 36, -1000, -1000, 59, -1000,
	-1000,
}
var yyPgo = [...]int{

	0, 273, 232, 272, 271, 1, 153, 221, 270, 3,
	269, 4, 25, 190, 268, 267, 266, 265, 264, 263,
	262, 184, 261, 260, 259, 258, 7, 254, 253, 252,
	251, 250, 6, 249, 2, 248, 247, 5, 246, 242,
	241, 240, 236, 233, 0,
}
var yyR1 = [...]int{

	0, 20, 3, 3, 3, 3, 4, 4, 4, 4,
	4, 21, 21, 21, 22, 22, 5, 5, 6, 6,
	6, 6, 7, 7, 7, 7, 10, 16, 17, 17,
	17, 17, 17, 18, 18, 18, 18, 18, 18, 18,
	8, 8, 8, 9, 9, 13, 13, 13, 13, 13,
	11, 11, 12, 12, 12, 14, 23, 14, 15, 24,
	15, 25, 15, 26, 27, 26, 28, 26, 29, 29,
	30, 30, 31, 31, 32, 32, 32, 32, 33, 33,
	34, 34, 34, 34, 35, 34, 34, 37, 37, 38,
	2, 2, 39, 40, 2, 2, 2, 2, 2, 41,
	2, 42, 2, 43, 2, 19, 44, 36, 36, 1,
	1,
}
var yyR2 = [...]int{

	0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 3, 3, 3, 2, 3, 1, 3, 1, 1,
	1, 1, 1, 1, 1, 1, 2, 3, 3, 3,
	3, 3, 3, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 1, 1, 3, 3, 3, 3, 3,
	1, 1, 0, 1, 3, 2, 0, 6, 2, 0,
	6, 0, 6, 3, 0, 6, 0, 9, 1, 3,
	1, 3, 2, 3, 3, 3, 3, 3, 2, 3,
	3, 3, 3, 5, 0, 7, 8, 2, 3, 0,
	5, 8, 0, 0, 7, 2, 4, 2, 4, 0,
	7, 0, 8, 0, 7, 1, 2, 1, 3, 1,
	2,
}
var yyChk = [...]int{

	-1000, -20, -1, -2, 9, 74, 16, -14, -15, 65,
	11, 57, -2, -38, 39, -39, 92, 81, 92, 81,
	18, 57, 85, 39, 17, 39, 69, 68, 97, 97,
	39, -13, -9, -17, 95, -8, -16, 69, 68, -10,
	17, 75, -13, 39, 39, 39, -23, -24, -25, -22,
	-21, -4, -3, 35, 29, 82, 56, 32, 38, 10,
	47, 48, 30, 50, -40, 83, 89, 92, -7, 22,
	23, 41, 40, -7, -13, 94, 94, 39, 28, -18,
	77, 15, 71, 80, 79, 46, 8, 94, 94, 92,
	-41, 63, -43, 95, 95, 95, -21, 98, 92, 91,
	91, 91, 91, 97, -13, -13, -6, 78, 73, 90,
	86, -6, 96, 39, 64, 70, 64, 70, 28, 97,
	-42, 97, -12, -11, -9, -6, -12, -12, 92, 73,
	78, 73, 73, -31, -26, 50, 5, 28, -33, -32,
	21, 19, 36, 37, 97, -37, -34, 18, 72, 62,
	70, 93, 96, 96, 96, 92, -26, 98, 92, 91,
	-27, 34, -32, 98, 92, 91, 91, 91, 91, -37,
	-34, 98, 92, 34, 91, 63, 34, -11, 98, 92,
	73, 91, 88, 92, 73, 73, 78, 78, 98, 92,
	39, 67, 73, 95, 34, 88, 95, 81, -5, 73,
	-35, 81, -5, 33, 93, 96, 95, 33, 96, -28,
	73, -36, -44, 39, 97, 97, 96, 93, -19, 84,
	-30, 73, -29, 78, -44, 93, 98, 98, 93, 73,
	78,
}
var yyDef = [...]int{

	0, -2, 1, 109, 89, 0, 92, 0, 0, 0,
	0, 0, 110, 0, 0, 0, 95, 0, 97, 0,
	0, 0, 0, 55, 56, 58, 59, 61, 0, 0,
	93, 0, 0, 0, 0, 43, 44, 0, 0, 0,
	0, 0, 0, 99, 0, 103, 0, 0, 0, 0,
	0, 0, 0, 0, 6, 7, 8, 9, 10, 2,
	3, 4, 5, 0, 0, 0, 0, 96, 0, 22,
	23, 24, 25, 0, 0, 0, 0, 40, 41, 42,
	33, 34, 35, 36, 37, 38, 39, 0, 26, 98,
	0, 101, 0, 52, 52, 52, 0, 90, 14, 0,
	0, 0, 0, 0, 48, 49, 45, 18, 19, 20,
	21, 46, 47, 28, 29, 30, 31, 32, 27, 0,
	0, 0, 0, 53, 50, 51, 0, 0, 15, 11,
	12, 13, 0, 0, 0, 0, 64, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 57, 60, 62, 0, 0, 94, 72, 0,
	0, 0, 0, 100, 78, 0, 0, 0, 0, 0,
	0, 104, 87, 0, 0, 0, 0, 54, 91, 73,
	63, 0, 0, 79, 74, 75, 76, 77, 102, 88,
	80, 81, 82, 0, 84, 0, 0, 0, 0, 16,
	0, 0, 0, 66, 0, 83, 0, 0, 65, 0,
	17, 0, 107, 0, 0, 0, 85, 0, 106, 105,
	0, 70, 0, 68, 108, 0, 86, 67, 0, 71,
	69,
}
var yyTok1 = [...]int{

	1, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	95, 96, 3, 3, 93, 3, 94, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 92,
	3, 91, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 97, 3, 98,
}
var yyTok2 = [...]int{

	2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
	62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89, 90,
}
var yyTok3 = [...]int{
	0,
}

var yyErrorMessages = [...]struct {
	state int
	token int
	msg   string
}{}

//line yaccpar:1

/*	parser for yacc output	*/

var (
	yyDebug        = 0
	yyErrorVerbose = false
)

type yyLexer interface {
	Lex(lval *yySymType) int
	Error(s string)
}

type yyParser interface {
	Parse(yyLexer) int
	Lookahead() int
}

type yyParserImpl struct {
	lval  yySymType
	stack [yyInitialStackSize]yySymType
	char  int
}

func (p *yyParserImpl) Lookahead() int {
	return p.char
}

func yyNewParser() yyParser {
	return &yyParserImpl{}
}

const yyFlag = -1000

func yyTokname(c int) string {
	if c >= 1 && c-1 < len(yyToknames) {
		if yyToknames[c-1] != "" {
			return yyToknames[c-1]
		}
	}
	return __yyfmt__.Sprintf("tok-%v", c)
}

func yyStatname(s int) string {
	if s >= 0 && s < len(yyStatenames) {
		if yyStatenames[s] != "" {
			return yyStatenames[s]
		}
	}
	return __yyfmt__.Sprintf("state-%v", s)
}

func yyErrorMessage(state, lookAhead int) string {
	const TOKSTART = 4

	if !yyErrorVerbose {
		return "syntax error"
	}

	for _, e := range yyErrorMessages {
		if e.state == state && e.token == lookAhead {
			return "syntax error: " + e.msg
		}
	}

	res := "syntax error: unexpected " + yyTokname(lookAhead)

	// To match Bison, suggest at most four expected tokens.
	expected := make([]int, 0, 4)

	// Look for shiftable tokens.
	base := yyPact[state]
	for tok := TOKSTART; tok-1 < len(yyToknames); tok++ {
		if n := base + tok; n >= 0 && n < yyLast && yyChk[yyAct[n]] == tok {
			if len(expected) == cap(expected) {
				return res
			}
			expected = append(expected, tok)
		}
	}

	if yyDef[state] == -2 {
		i := 0
		for yyExca[i] != -1 || yyExca[i+1] != state {
			i += 2
		}

		// Look for tokens that we accept or reduce.
		for i += 2; yyExca[i] >= 0; i += 2 {
			tok := yyExca[i]
			if tok < TOKSTART || yyExca[i+1] == 0 {
				continue
			}
			if len(expected) == cap(expected) {
				return res
			}
			expected = append(expected, tok)
		}

		// If the default action is to accept or reduce, give up.
		if yyExca[i+1] != 0 {
			return res
		}
	}

	for i, tok := range expected {
		if i == 0 {
			res += ", expecting "
		} else {
			res += " or "
		}
		res += yyTokname(tok)
	}
	return res
}

func yylex1(lex yyLexer, lval *yySymType) (char, token int) {
	token = 0
	char = lex.Lex(lval)
	if char <= 0 {
		token = yyTok1[0]
		goto out
	}
	if char < len(yyTok1) {
		token = yyTok1[char]
		goto out
	}
	if char >= yyPrivate {
		if char < yyPrivate+len(yyTok2) {
			token = yyTok2[char-yyPrivate]
			goto out
		}
	}
	for i := 0; i < len(yyTok3); i += 2 {
		token = yyTok3[i+0]
		if token == char {
			token = yyTok3[i+1]
			goto out
		}
	}

out:
	if token == 0 {
		token = yyTok2[1] /* unknown char */
	}
	if yyDebug >= 3 {
		__yyfmt__.Printf("lex %s(%d)\n", yyTokname(token), uint(char))
	}
	return char, token
}

func yyParse(yylex yyLexer) int {
	return yyNewParser().Parse(yylex)
}

func (yyrcvr *yyParserImpl) Parse(yylex yyLexer) int {
	var yyn int
	var yyVAL yySymType
	var yyDollar []yySymType
	_ = yyDollar // silence set and not used
	yyS := yyrcvr.stack[:]

	Nerrs := 0   /* number of errors */
	Errflag := 0 /* error recovery flag */
	yystate := 0
	yyrcvr.char = -1
	yytoken := -1 // yyrcvr.char translated into internal numbering
	defer func() {
		// Make sure we report no lookahead when not parsing.
		yystate = -1
		yyrcvr.char = -1
		yytoken = -1
	}()
	yyp := -1
	goto yystack

ret0:
	return 0

ret1:
	return 1

yystack:
	/* put a state and value onto the stack */
	if yyDebug >= 4 {
		__yyfmt__.Printf("char %v in %v\n", yyTokname(yytoken), yyStatname(yystate))
	}

	yyp++
	if yyp >= len(yyS) {
		nyys := make([]yySymType, len(yyS)*2)
		copy(nyys, yyS)
		yyS = nyys
	}
	yyS[yyp] = yyVAL
	yyS[yyp].yys = yystate

yynewstate:
	yyn = yyPact[yystate]
	if yyn <= yyFlag {
		goto yydefault /* simple state */
	}
	if yyrcvr.char < 0 {
		yyrcvr.char, yytoken = yylex1(yylex, &yyrcvr.lval)
	}
	yyn += yytoken
	if yyn < 0 || yyn >= yyLast {
		goto yydefault
	}
	yyn = yyAct[yyn]
	if yyChk[yyn] == yytoken { /* valid shift */
		yyrcvr.char = -1
		yytoken = -1
		yyVAL = yyrcvr.lval
		yystate = yyn
		if Errflag > 0 {
			Errflag--
		}
		goto yystack
	}

yydefault:
	/* default state action */
	yyn = yyDef[yystate]
	if yyn == -2 {
		if yyrcvr.char < 0 {
			yyrcvr.char, yytoken = yylex1(yylex, &yyrcvr.lval)
		}

		/* look through exception table */
		xi := 0
		for {
			if yyExca[xi+0] == -1 && yyExca[xi+1] == yystate {
				break
			}
			xi += 2
		}
		for xi += 2; ; xi += 2 {
			yyn = yyExca[xi+0]
			if yyn < 0 || yyn == yytoken {
				break
			}
		}
		yyn = yyExca[xi+1]
		if yyn < 0 {
			goto ret0
		}
	}
	if yyn == 0 {
		/* error ... attempt to resume parsing */
		switch Errflag {
		case 0: /* brand new error */
			yylex.Error(yyErrorMessage(yystate, yytoken))
			Nerrs++
			if yyDebug >= 1 {
				__yyfmt__.Printf("%s", yyStatname(yystate))
				__yyfmt__.Printf(" saw %s\n", yyTokname(yytoken))
			}
			fallthrough

		case 1, 2: /* incompletely recovered error ... try again */
			Errflag = 3

			/* find a state where "error" is a legal shift action */
			for yyp >= 0 {
				yyn = yyPact[yyS[yyp].yys] + yyErrCode
				if yyn >= 0 && yyn < yyLast {
					yystate = yyAct[yyn] /* simulate a shift of "error" */
					if yyChk[yystate] == yyErrCode {
						goto yystack
					}
				}

				/* the current p has no shift on "error", pop stack */
				if yyDebug >= 2 {
					__yyfmt__.Printf("error recovery pops state %d\n", yyS[yyp].yys)
				}
				yyp--
			}
			/* there is no state on the stack with an error shift ... abort */
			goto ret1

		case 3: /* no shift yet; clobber input char */
			if yyDebug >= 2 {
				__yyfmt__.Printf("error recovery discards %s\n", yyTokname(yytoken))
			}
			if yytoken == yyEofCode {
				goto ret1
			}
			yyrcvr.char = -1
			yytoken = -1
			goto yynewstate /* try again in the same state */
		}
	}

	/* reduction by production yyn */
	if yyDebug >= 2 {
		__yyfmt__.Printf("reduce %v in:\n\t%v\n", yyn, yyStatname(yystate))
	}

	yynt := yyn
	yypt := yyp
	_ = yypt // guard against "declared and not used"

	yyp -= yyR2[yyn]
	// yyp is now the index of $0. Perform the default action. Iff the
	// reduced production is ε, $1 is possibly out of range.
	if yyp+1 >= len(yyS) {
		nyys := make([]yySymType, len(yyS)*2)
		copy(nyys, yyS)
		yyS = nyys
	}
	yyVAL = yyS[yyp+1]

	/* consult goto table to find next state */
	yyn = yyR1[yyn]
	yyg := yyPgo[yyn]
	yyj := yyg + yyS[yyp].yys + 1

	if yyj >= yyLast {
		yystate = yyAct[yyg]
	} else {
		yystate = yyAct[yyj]
		if yyChk[yystate] != -yyn {
			yystate = yyAct[yyg]
		}
	}
	// dummy call; replaced with literal code
	switch yynt {

	case 2:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:304
		{
			l := yylex.(*yyLexState)
			if l.seen_brr_capacity {
				l.error("boot: can not set brr_capacity again")
				return 0
			}
			l.seen_brr_capacity = true
			yyVAL.string = "brr_capacity"
		}
	case 3:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:315
		{
			l := yylex.(*yyLexState)
			if l.seen_os_exec_capacity {
				l.error("boot: can not set os_exec_capacity again")
				return 0
			}
			l.seen_os_exec_capacity = true
			yyVAL.string = "os_exec_capacity"
		}
	case 4:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:326
		{
			l := yylex.(*yyLexState)
			if l.seen_os_exec_worker_count {
				l.error("boot: can not set os_exec_worker_count again")
				return 0
			}
			l.seen_os_exec_worker_count = true
			yyVAL.string = "os_exec_worker_count"
		}
	case 5:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:337
		{
			l := yylex.(*yyLexState)
			if l.seen_flow_worker_count {
				l.error("boot: can not set flow_worker_count again")
				return 0
			}
			l.seen_flow_worker_count = true
			yyVAL.string = "flow_worker_count"
		}
	case 6:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:350
		{
			l := yylex.(*yyLexState)
			if l.seen_fdr_roll_duration {
				l.error("boot: can not set fdr_roll_duration again")
				return 0
			}
			l.seen_fdr_roll_duration = true
			yyVAL.string = "fdr_roll_duration"
		}
	case 7:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:361
		{
			l := yylex.(*yyLexState)
			if l.seen_xdr_roll_duration {
				l.error("boot: can not set xdr_roll_duration again")
				return 0
			}
			l.seen_xdr_roll_duration = true
			yyVAL.string = "xdr_roll_duration"
		}
	case 8:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:372
		{
			l := yylex.(*yyLexState)
			if l.seen_qdr_roll_duration {
				l.error("boot: can not set qdr_roll_duration again")
				return 0
			}
			l.seen_qdr_roll_duration = true
			yyVAL.string = "qdr_roll_duration"
		}
	case 9:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:383
		{
			l := yylex.(*yyLexState)
			if l.seen_heartbeat_duration {
				l.error("boot: can not set heartbeat_duration again")
				return 0
			}
			l.seen_heartbeat_duration = true
			yyVAL.string = "heartbeat_duration"
		}
	case 10:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:394
		{
			l := yylex.(*yyLexState)
			if l.seen_memstats_duration {
				l.error("boot: can not set memstats_duration again")
				return 0
			}
			l.seen_memstats_duration = true
			yyVAL.string = "memstats_duration"
		}
	case 11:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:407
		{
			l := yylex.(*yyLexState)

			d, err := ParseDuration(yyDollar[3].string)
			if err != nil {
				l.error("boot: %s: %s", yyDollar[1].string, err)
				return 0
			}
			if d == 0 {
				l.error("boot: %s: duration can not be 0", yyDollar[1].string)
				return 0
			}
			switch {

			case yyDollar[1].string == "fdr_roll_duration":
				l.config.fdr_roll_duration = d

			case yyDollar[1].string == "xdr_roll_duration":
				l.config.xdr_roll_duration = d

			case yyDollar[1].string == "qdr_roll_duration":
				l.config.qdr_roll_duration = d

			case yyDollar[1].string == "heartbeat_duration":
				l.config.heartbeat_duration = d

			case yyDollar[1].string == "memstats_duration":
				l.config.memstats_duration = d

			default:
				panic("boot: impossible duration variable: " + yyDollar[1].string)
			}
		}
	case 12:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:442
		{
			l := yylex.(*yyLexState)

			cap64 := yyDollar[3].uint64
			if cap64 > 65535 {
				l.error("boot: %s too big: > 65535: %d", yyDollar[1].string, cap64)
				return 0
			}
			cap := uint16(cap64)
			switch {
			case yyDollar[1].string == "brr_capacity":
				l.config.brr_capacity = cap

			case yyDollar[1].string == "os_exec_capacity":
				l.config.os_exec_capacity = cap

			case yyDollar[1].string == "os_exec_worker_count":
				l.config.os_exec_worker_count = cap

			case yyDollar[1].string == "flow_worker_count":
				l.config.flow_worker_count = cap

			default:
				panic("boot: impossible capacity variable: " + yyDollar[1].string)
			}
		}
	case 13:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:470
		{
			l := yylex.(*yyLexState)

			if yyDollar[3].string == "" {
				l.error("boot: log_directory is empty")
				return 0
			}

			if l.config.log_directory != "" {
				l.error("boot: log_directory set again")
				return 0
			}
			l.config.log_directory = yyDollar[3].string
		}
	case 16:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:497
		{
			sl := make([]string, 1)
			sl[0] = yyDollar[1].string
			yyVAL.string_list = sl
		}
	case 17:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:504
		{
			yyVAL.string_list = append(yyDollar[1].string_list, yyDollar[3].string)
			if len(yyVAL.string_list) >= max_argv {
				l := yylex.(*yyLexState)

				l.error("argv > %d strings", max_argv)
				return 0
			}
		}
	case 18:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:517
		{
			yyVAL.ast = &ast{
				yy_tok: UINT64,
				uint64: yyDollar[1].uint64,
			}
		}
	case 19:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:525
		{
			yyVAL.ast = &ast{
				yy_tok: STRING,
				string: yyDollar[1].string,
			}
		}
	case 20:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:533
		{
			yyVAL.ast = &ast{
				yy_tok: yy_TRUE,
				bool:   true,
			}
		}
	case 21:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:541
		{
			yyVAL.ast = &ast{
				yy_tok: yy_FALSE,
				bool:   false,
			}
		}
	case 22:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:551
		{
			yyVAL.ast = &ast{
				yy_tok: EQ,
			}
		}
	case 23:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:558
		{
			yyVAL.ast = &ast{
				yy_tok: MATCH,
			}
		}
	case 24:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:565
		{
			yyVAL.ast = &ast{
				yy_tok: NO_MATCH,
			}
		}
	case 25:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:572
		{
			yyVAL.ast = &ast{
				yy_tok: NEQ,
			}
		}
	case 26:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:581
		{
			yyVAL.ast = &ast{
				yy_tok: PROJECT_BRR,
				tail:   yyDollar[1].tail,
			}
		}
	case 27:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:591
		{
			l := yylex.(*yyLexState)

			//  record dependency between call_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			//  the command references/depends upon which rule?
			//
			//  Note: move the depends code to production 'projection'?

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("exit_status: unknown rule")
			}
			if yyDollar[1].command.depend_ref_count == 255 {
				l.error("%s: too many dependencies: > 255", yyDollar[1].command.name)
				return 0
			}
			yyDollar[1].command.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == yyDollar[1].command.name {
				l.error("%s '%s' refers to itself", what, yyDollar[1].command.name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, yyDollar[1].command.name),
			)

			yyVAL.ast = &ast{
				yy_tok:  PROJECT_XDR_EXIT_STATUS,
				string:  yyDollar[1].command.name,
				command: yyDollar[1].command,
			}
		}
	case 28:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:649
		{
			l := yylex.(*yyLexState)

			r := yyDollar[1].sql_query_row.name2result[yyDollar[3].string]
			if r == nil {
				l.error("sql query row: %s: unknown attribute: %s",
					yyDollar[1].sql_query_row.name, yyDollar[3].string)
				return 0
			}

			//  record dependency between query_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			name := yyDollar[1].sql_query_row.name

			//  what is the rule being parsed?

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("sql query row: inside unknown rule")
			}
			if yyDollar[1].sql_query_row.depend_ref_count == 255 {
				l.error("%s: %s: too many dependencies: > 255",
					what, name)
				return 0
			}
			yyDollar[1].sql_query_row.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == name {
				l.error("%s '%s' refers to itself", what, name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, name),
			)

			yyVAL.ast = &ast{
				yy_tok: PROJECT_SQL_QUERY_ROW_BOOL,
				string: yyDollar[1].sql_query_row.name,
				uint8:  r.offset,
			}
		}
	case 29:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:713
		{
			l := yylex.(*yyLexState)

			//  record dependency between query_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			name := yyDollar[1].sql_query_row.name

			//  what is the rule being parsed?
			//  Note: desparatley needs to be refactored

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("rows_affected: inside unknown rule")
			}
			if yyDollar[1].sql_query_row.depend_ref_count == 255 {
				l.error("%s: %s: too many dependencies: > 255",
					what, name)
				return 0
			}
			yyDollar[1].sql_query_row.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == name {
				l.error("%s '%s' refers to itself", what, name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, name),
			)

			yyVAL.ast = &ast{
				yy_tok: PROJECT_QDR_ROWS_AFFECTED,
				string: yyDollar[1].sql_query_row.name,
			}
		}
	case 30:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:770
		{
			l := yylex.(*yyLexState)

			//  record dependency between query_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			name := yyDollar[1].sql_query_row.name

			//  what is the rule being parsed?

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("sqlstate: inside unknown rule")
			}
			if yyDollar[1].sql_query_row.depend_ref_count == 255 {
				l.error("%s: %s: too many dependencies: > 255",
					what, name)
				return 0
			}
			yyDollar[1].sql_query_row.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == name {
				l.error("%s '%s' refers to itself", what, name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, name),
			)

			yyVAL.ast = &ast{
				yy_tok: PROJECT_QDR_SQLSTATE,
				string: yyDollar[1].sql_query_row.name,
			}
		}
	case 31:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:826
		{
			l := yylex.(*yyLexState)

			//  record dependency between query_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			name := yyDollar[1].sql_exec.name

			//  what is the rule being parsed?

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("rows_affected: inside unknown rule")
			}
			if yyDollar[1].sql_exec.depend_ref_count == 255 {
				l.error("%s: %s: too many dependencies: > 255",
					what, name)
				return 0
			}
			yyDollar[1].sql_exec.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == name {
				l.error("%s '%s' refers to itself", what, name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, name),
			)

			yyVAL.ast = &ast{
				yy_tok: PROJECT_QDR_ROWS_AFFECTED,
				string: yyDollar[1].sql_exec.name,
			}
		}
	case 32:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:882
		{
			l := yylex.(*yyLexState)

			//  record dependency between query_project and statement.
			//
			//  gnu tsort treats nodes that point to themselves as islands,
			//  not loops, so check here that we don't point to our selves.

			var subject, what string

			name := yyDollar[1].sql_exec.name

			//  what is the rule being parsed?

			switch {
			case l.call != nil:
				what = "call"
				subject = l.call.command.name
			case l.sql_query_row != nil:
				what = "sql query row"
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				what = "sql exec"
				subject = l.sql_exec.name
			default:
				panic("sql exec: sqlstate: inside unknown rule")
			}
			if yyDollar[1].sql_exec.depend_ref_count == 255 {
				l.error("%s: %s: too many dependencies: > 255",
					what, name)
				return 0
			}
			yyDollar[1].sql_exec.depend_ref_count++

			//  is trivial loopback?  gnu tsort for some reason treats
			//  these as island nodes and not loopbacks.

			if subject == name {
				l.error("%s '%s' refers to itself", what, name)
				return 0
			}

			//  record for detection of cycles in the invocation graph

			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, name),
			)

			yyVAL.ast = &ast{
				yy_tok: PROJECT_QDR_SQLSTATE,
				string: yyDollar[1].sql_exec.name,
			}
		}
	case 33:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:940
		{
			yyVAL.brr_field = brr_field(brr_UDIG)
		}
	case 34:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:945
		{
			yyVAL.brr_field = brr_field(brr_CHAT_HISTORY)
		}
	case 35:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:950
		{
			yyVAL.brr_field = brr_field(brr_START_TIME)
		}
	case 36:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:955
		{
			yyVAL.brr_field = brr_field(brr_WALL_DURATION)
		}
	case 37:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:960
		{
			yyVAL.brr_field = brr_field(brr_VERB)
		}
	case 38:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:965
		{
			yyVAL.brr_field = brr_field(brr_NETFLOW)
		}
	case 39:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:970
		{
			yyVAL.brr_field = brr_field(brr_BLOB_SIZE)
		}
	case 40:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:977
		{
			l := yylex.(*yyLexState)
			l.error("%s: unknown tail attribute: %s", yyDollar[1].ast.tail.name, yyDollar[2].string)
			return 0
		}
	case 41:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:984
		{
			l := yylex.(*yyLexState)
			l.error("%s: exit_status is not a tail attribute", yyDollar[1].ast.tail.name)
			return 0
		}
	case 42:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:991
		{
			yyDollar[1].ast.brr_field = yyDollar[2].brr_field

			l := yylex.(*yyLexState)

			var subject string

			//  Note: move the depends code to production 'att'?
			switch {
			case l.call != nil:
				subject = l.call.command.name
			case l.sql_query_row != nil:
				subject = l.sql_query_row.name
			case l.sql_exec != nil:
				subject = l.sql_exec.name
			default:
				panic("impossible TAIL_REF outside of rule")
			}
			l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, yyDollar[1].ast.tail.name))
		}
	case 45:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1024
		{
			l := yylex.(*yyLexState)
			left := yyDollar[1].ast

			if l.wire_rel_op(yyDollar[1].ast, yyDollar[2].ast, yyDollar[3].ast) == false {
				return 0
			}
			yyDollar[2].ast.left = left
			yyDollar[2].ast.right = nil

			yyVAL.ast = yyDollar[2].ast
		}
	case 46:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1039
		{
			l := yylex.(*yyLexState)
			q := yyDollar[1].ast.sql_query_row

			rel_tok := yyDollar[2].ast.yy_tok

			switch {
			case yyDollar[1].ast.is_bool() && yyDollar[3].ast.is_bool():
				if rel_tok == EQ {
					rel_tok = EQ_BOOL
				} else {
					rel_tok = NEQ_BOOL
				}
				yyVAL.ast = &ast{
					yy_tok: rel_tok,

					bool: yyDollar[3].ast.bool,
					left: yyDollar[1].ast,
				}
			case yyDollar[1].ast.is_uint64() && yyDollar[3].ast.is_uint64():
				if rel_tok == EQ {
					rel_tok = EQ_UINT64
				} else {
					rel_tok = NEQ_UINT64
				}
				yyVAL.ast = &ast{
					yy_tok: rel_tok,

					uint64: yyDollar[3].ast.uint64,
					left:   yyDollar[1].ast,
				}
			case yyDollar[1].ast.is_string() && yyDollar[3].ast.is_string():

				var rex *regexp.Regexp
				var err error

				switch rel_tok {
				case EQ:
					rel_tok = EQ_STRING
				case NEQ:
					rel_tok = NEQ_STRING
				case MATCH:
					rel_tok = MATCH_STRING
					rex, err = regexp.Compile(yyDollar[3].ast.string)
				case NO_MATCH:
					rel_tok = NO_MATCH_STRING
					rex, err = regexp.Compile(yyDollar[3].ast.string)
				default:
					panic("impossible relop")
				}
				if err != nil {
					l.error("can not compile regex: %s", yyDollar[3].ast.string)
					return 0
				}
				yyVAL.ast = &ast{
					yy_tok: rel_tok,

					string: yyDollar[3].ast.string,
					regexp: rex,
					left:   yyDollar[1].ast,
				}
			default:
				l.error("%s: comparisons are different types", q.name)
				return 0
			}
		}
	case 47:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1107
		{
			yyVAL.ast = yyDollar[2].ast
		}
	case 48:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1112
		{
			yyVAL.ast = &ast{
				yy_tok: yy_AND,
				left:   yyDollar[1].ast,
				right:  yyDollar[3].ast,
			}
		}
	case 49:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1121
		{
			yyVAL.ast = &ast{
				yy_tok: yy_OR,
				left:   yyDollar[1].ast,
				right:  yyDollar[3].ast,
			}
		}
	case 52:
		yyDollar = yyS[yypt-0 : yypt+1]
		//line parser.y:1138
		{
			yyVAL.ast = nil
		}
	case 54:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1145
		{
			a := yyDollar[1].ast

			//  find the tail of the list
			for ; a.next != nil; a = a.next {
			}

			a.next = yyDollar[3].ast
		}
	case 55:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1157
		{
			l := yylex.(*yyLexState)
			l.error("unknown command: '%s'", yyDollar[2].string)
			return 0
		}
	case 56:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1164
		{
			l := yylex.(*yyLexState)
			l.call = &call{
				command: yyDollar[2].command,
			}
		}
	case 57:
		yyDollar = yyS[yypt-6 : yypt+1]
		//line parser.y:1171
		{
			l := yylex.(*yyLexState)
			cmd := yyDollar[2].command

			if cmd.called {
				l.error("command %s: called twice", cmd.name)
				return 0
			}

			call := l.call
			cmd.called = true

			//  count the arguments and determine if we need to cast
			//  certain arguments to STRING
			//
			//  Note: need to refactor with code in query() prodcution

			argc := uint64(0)
			has_uint64 := false
			for a := yyDollar[5].ast; a != nil; a = a.next {
				argc++
				if a.is_uint64() {
					has_uint64 = true
				}
			}

			//  the 'call(args ...)' expects all args to be strings,
			//  so reparent any is_uint64() nodes with CAST_STRING node
			//  and rewire the link list of arguments nodes.

			argv := yyDollar[5].ast
			if has_uint64 {

				al := make([]*ast, argc)

				//  copy nodes to temporary list, reparenting
				//  is_uint64() nodes

				for a, i := yyDollar[5].ast, 0; a != nil; a = a.next {
					if a.is_uint64() {
						al[i] = &ast{
							yy_tok: CAST_STRING,
							left:   a,
						}
					} else {
						al[i] = a
					}
					i++
				}

				//  rewire the linked list list
				var prev *ast
				for _, a := range al {
					if prev != nil {
						prev.next = a
					}
					prev = a
					if a.yy_tok == CAST_STRING {
						a.left.next = nil
					}
				}
				argv = al[0]
			}
			var tok, ctok int

			ctok = CALL
			switch argc {
			case 0:
				if call.command.path == "true" {
					ctok = CALL0
				}
				tok = ARGV0
			case 1:
				tok = ARGV1
			default:
				tok = ARGV
			}

			yyVAL.ast = &ast{
				yy_tok: ctok,
				call:   call,
				left: &ast{
					yy_tok: tok,
					left:   argv,
					uint64: argc,
				},
			}
			l.call2ast[call.command.name] = yyVAL.ast
		}
	case 58:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1264
		{
			l := yylex.(*yyLexState)
			l.error("unknown query: '%s'", yyDollar[2].string)
			return 0
		}
	case 59:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1271
		{
			l := yylex.(*yyLexState)
			l.sql_query_row = yyDollar[2].sql_query_row
		}
	case 60:
		yyDollar = yyS[yypt-6 : yypt+1]
		//line parser.y:1276
		{
			l := yylex.(*yyLexState)
			q := yyDollar[2].sql_query_row

			if q.called {
				l.error("sql query %s: called twice", q.name)
				return 0
			}

			q.called = true

			//  count the arguments and determine if we need to cast
			//  certain arguments to STRING
			//
			//  Note: need to refactor with code in call() prodcution

			argc := uint64(0)
			has_uint64 := false
			for a := yyDollar[5].ast; a != nil; a = a.next {
				argc++
				if a.is_uint64() {
					has_uint64 = true
				}
			}

			//  the 'query(args ...)' expects all args to be strings,
			//  so reparent any is_uint64() nodes with CAST_STRING node
			//  and rewire the link list of arguments nodes.

			argv := yyDollar[5].ast
			if has_uint64 {

				al := make([]*ast, argc)

				//  copy nodes to temporary list, reparenting
				//  is_uint64() nodes

				for a, i := yyDollar[5].ast, 0; a != nil; a = a.next {
					if a.is_uint64() {
						al[i] = &ast{
							yy_tok: CAST_STRING,
							left:   a,
						}
					} else {
						al[i] = a
					}
					i++
				}

				//  rewire the linked list list
				var prev *ast
				for _, a := range al {
					if prev != nil {
						prev.next = a
					}
					prev = a
					if a.yy_tok == CAST_STRING {
						a.left.next = nil
					}
				}
				argv = al[0]
			}
			var tok int

			switch argc {
			case 0:
				tok = ARGV0
			case 1:
				tok = ARGV1
			default:
				tok = ARGV
			}

			yyVAL.ast = &ast{
				yy_tok:        QUERY_ROW,
				sql_query_row: q,
				left: &ast{
					yy_tok: tok,
					left:   argv,
					uint64: argc,
				},
			}
			l.query2ast[q.name] = yyVAL.ast
		}
	case 61:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1362
		{
			l := yylex.(*yyLexState)
			l.sql_exec = yyDollar[2].sql_exec
		}
	case 62:
		yyDollar = yyS[yypt-6 : yypt+1]
		//line parser.y:1367
		{
			l := yylex.(*yyLexState)
			ex := yyDollar[2].sql_exec

			if ex.called {
				l.error("sql exec %s: called twice", ex.name)
				return 0
			}

			ex.called = true

			//  count the arguments and determine if we need to cast
			//  certain arguments to STRING
			//
			//  Note: need to refactor with code in call() prodcution
			//        Really!

			argc := uint64(0)
			has_uint64 := false
			for a := yyDollar[5].ast; a != nil; a = a.next {
				argc++
				if a.is_uint64() {
					has_uint64 = true
				}
			}

			//  the 'query(args ...)' expects all args to be strings,
			//  so reparent any is_uint64() nodes with CAST_STRING node
			//  and rewire the link list of arguments nodes.

			argv := yyDollar[5].ast
			if has_uint64 {

				al := make([]*ast, argc)

				//  copy nodes to temporary list, reparenting
				//  is_uint64() nodes

				for a, i := yyDollar[5].ast, 0; a != nil; a = a.next {
					if a.is_uint64() {
						al[i] = &ast{
							yy_tok: CAST_STRING,
							left:   a,
						}
					} else {
						al[i] = a
					}
					i++
				}

				//  rewire the linked list list
				var prev *ast
				for _, a := range al {
					if prev != nil {
						prev.next = a
					}
					prev = a
					if a.yy_tok == CAST_STRING {
						a.left.next = nil
					}
				}
				argv = al[0]
			}
			var tok int

			switch argc {
			case 0:
				tok = ARGV0
			case 1:
				tok = ARGV1
			default:
				tok = ARGV
			}

			//  multiple statements imply a transaction
			var q_tok int
			if len(ex.statement) == 1 {
				q_tok = QUERY_EXEC
			} else {
				q_tok = QUERY_EXEC_TXN
			}

			yyVAL.ast = &ast{
				yy_tok:   q_tok,
				sql_exec: ex,
				left: &ast{
					yy_tok: tok,
					left:   argv,
					uint64: argc,
				},
			}
			l.query2ast[ex.name] = yyVAL.ast
		}
	case 63:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1464
		{
			l := yylex.(*yyLexState)
			cmd := l.command
			if cmd.path != "" {
				l.error("command: %s: path can not be set again",
					cmd.name)
				return 0
			}
			if yyDollar[3].string == "" {
				l.error("command: %s: path can not be empty", cmd.name)
				return 0
			}
			cmd.path = yyDollar[3].string
		}
	case 64:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1479
		{
			l := yylex.(*yyLexState)
			cmd := l.command

			if cmd.argv != nil {
				l.error("command %s: argv defined again", cmd.name)
				return 0
			}
		}
	case 65:
		yyDollar = yyS[yypt-6 : yypt+1]
		//line parser.y:1488
		{
			yylex.(*yyLexState).command.argv = yyDollar[5].string_list
		}
	case 66:
		yyDollar = yyS[yypt-5 : yypt+1]
		//line parser.y:1495
		{
			l := yylex.(*yyLexState)
			cmd := l.command
			if cmd.OK_exit_status != nil {
				l.error("command %s: 'exit_status OK' defined again",
					cmd.name)
				return 0
			}
			cmd.OK_exit_status = make([]byte, 32)
		}
	case 68:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1510
		{
			l := yylex.(*yyLexState)

			if yyDollar[1].uint64 > 255 {
				l.error("exit status > 255")
				return 0
			}
			u8 := uint8(yyDollar[1].uint64)

			l.command.OK_exit_status[u8/8] |= 0x1 << (u8 % 8)
		}
	case 69:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1523
		{
			l := yylex.(*yyLexState)
			if yyDollar[3].uint64 > 255 {
				l.error("exit status > 255")
				return 0
			}
			u8 := uint8(yyDollar[3].uint64)

			/*
			 *  Logical and in the bit.
			 */
			l.command.OK_exit_status[u8/8] |= 0x1 << (u8 % 8)
		}
	case 70:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1540
		{
			if !(yylex.(*yyLexState)).put_sqlstate(yyDollar[1].string) {
				return 0
			}
		}
	case 71:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1547
		{
			if !(yylex.(*yyLexState)).put_sqlstate(yyDollar[3].string) {
				return 0
			}
		}
	case 74:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1562
		{
			l := yylex.(*yyLexState)
			if l.seen_driver_name {
				l.error("sql database: %s: driver_name redefined",
					l.sql_database.name)
				return 0
			}
			l.seen_driver_name = true

			if yyDollar[3].string == "" {
				l.error("sql database: empty driver")
				return 0
			}
			l.sql_database.driver_name = yyDollar[3].string
		}
	case 75:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1579
		{
			l := yylex.(*yyLexState)
			if l.seen_data_source_name {
				l.error("sql database: %s: data_source_name redefined",
					l.sql_database.name)
				return 0
			}
			l.seen_data_source_name = true

			if yyDollar[3].string == "" {
				l.error("sql database: empty driver")
				return 0
			}
			l.sql_database.data_source_name = yyDollar[3].string
		}
	case 76:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1596
		{
			l := yylex.(*yyLexState)
			if l.seen_max_idle_conns {
				l.error(
					"sql database: %s: max_open_connections redefined",
					l.sql_database.name)
				return 0
			}
			l.seen_max_idle_conns = true
			l.sql_database.max_idle_conns = int(yyDollar[3].uint64)
		}
	case 77:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1609
		{
			l := yylex.(*yyLexState)
			if l.seen_max_open_conns {
				l.error(
					"sql database: %s: max_open_connections redefined",
					l.sql_database.name)
				return 0
			}
			l.seen_max_open_conns = true
			l.sql_database.max_open_conns = int(yyDollar[3].uint64)
		}
	case 80:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1630
		{
			l := yylex.(*yyLexState)
			l.error("unknown database: %s", yyDollar[3].string)
			return 0
		}
	case 81:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1637
		{
			l := yylex.(*yyLexState)

			switch {

			//  statement is in an 'sql query <name> row' block
			case l.sql_query_row != nil:
				q := l.sql_query_row
				if q.sql_database != nil {
					l.error("sql query row: %s: database redefined",
						q.name)
					return 0
				}
				q.sql_database = yyDollar[3].sql_database

				//  statement is in an 'sql exec <name>' block
			case l.sql_exec != nil:
				ex := l.sql_exec
				if ex.sql_database != nil {
					l.error("sql exec: %s: database redefined",
						ex.name)
					return 0
				}
				ex.sql_database = yyDollar[3].sql_database
			default:
				panic("sql database: " +
					"both sql_query_row and sql_exec are nil")
			}
		}
	case 82:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1668
		{
			l := yylex.(*yyLexState)
			if yyDollar[3].string == "" {
				l.error("sql: empty query string not allowed")
				return 0
			}

			switch {

			//  statement is in an 'sql query <name> row' block
			case l.sql_query_row != nil:
				q := l.sql_query_row
				if q.statement != "" {
					l.error(
						"sql query row: %s: statement redefined",
						q.name,
					)
					return 0
				}
				q.statement = yyDollar[3].string

				//  statement is in an 'sql exec <name>' block
			case l.sql_exec != nil:
				ex := l.sql_exec
				if ex.statement != nil {
					l.error("sql exec: %s: statement redefined",
						ex.name)
					return 0
				}
				ex.statement = append(ex.statement, yyDollar[3].string)
			default:
				panic("both sql_query_row and sql_exec are nil")
			}
		}
	case 83:
		yyDollar = yyS[yypt-5 : yypt+1]
		//line parser.y:1704
		{
			l := yylex.(*yyLexState)

			if l.sql_query_row != nil {
				l.error("sql query row: %s: transaction not allowed",
					l.sql_query_row.name)
				return 0
			}
			ex := l.sql_exec
			if ex.statement != nil {
				l.error("sql exec txn: %s: statement redefined",
					ex.name)
				return 0
			}
			ex.statement = make([]string, len(yyDollar[4].string_list))
			for i, s := range yyDollar[4].string_list {
				ex.statement[i] = s
			}
		}
	case 84:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1725
		{
			l := yylex.(*yyLexState)

			switch {

			//  statement is in an 'sql query <name> row' block
			case l.sql_query_row != nil:
				q := l.sql_query_row
				if len(q.result_row) > 0 {
					l.error("sql query row: result row redefined",
						q.name)
					return 0
				}
			case l.sql_exec != nil:
				l.error("sql exec: result row can't be in exec")
				return 0
			default:
				panic("both sql_query_row and sql_exec are nil")
			}
		}
	case 89:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1757
		{
			l := yylex.(*yyLexState)
			if l.seen_boot {
				l.error("boot{...} defined again")
				return 0
			}
			l.seen_boot = true
			l.in_boot = true
		}
	case 90:
		yyDollar = yyS[yypt-5 : yypt+1]
		//line parser.y:1768
		{
			yylex.(*yyLexState).in_boot = false
			yyVAL.ast = &ast{
				yy_tok: BOOT,
			}
		}
	case 91:
		yyDollar = yyS[yypt-8 : yypt+1]
		//line parser.y:1777
		{
			l := yylex.(*yyLexState)
			/*
			 *  Uggh.  Only one tail per document
			 */
			if l.config.tail != nil {

				l.error("tail already defined: %s", l.config.tail.name)
				return 0
			}
			l.config.tail = &tail{
				name: yyDollar[2].string,
				path: yyDollar[6].string,
			}
			yyVAL.ast = &ast{
				yy_tok: TAIL,
				tail:   l.config.tail,
			}
		}
	case 92:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1798
		{
			l := yylex.(*yyLexState)
			if l.command != nil {
				panic("command: yyLexState.command != nil")
			}
			l.command = &command{}
			yyVAL.command = l.command

		}
	case 93:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1806
		{
			yyDollar[2].command.name = yyDollar[3].string
		}
	case 94:
		yyDollar = yyS[yypt-7 : yypt+1]
		//line parser.y:1807
		{
			l := yylex.(*yyLexState)
			if len(l.config.command) > 255 {
				l.error("too many commands defined: max is 255")
				return 0
			}
			cmd := l.command
			name := cmd.name

			if cmd.path == "" {
				l.error("command %s: path not set", name)
				return 0
			}

			l.config.command[name] = yyDollar[2].command
			l.command = nil
			yyVAL.ast = &ast{
				yy_tok:  COMMAND,
				command: yyDollar[2].command,
			}
		}
	case 95:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1830
		{
			yyDollar[1].ast.right = &ast{
				yy_tok: WHEN,
				left: &ast{
					yy_tok: yy_TRUE,
				},
			}
			yylex.(*yyLexState).call = nil
		}
	case 96:
		yyDollar = yyS[yypt-4 : yypt+1]
		//line parser.y:1841
		{
			yyDollar[1].ast.right = &ast{
				yy_tok: WHEN,
				left:   yyDollar[3].ast,
			}
			yylex.(*yyLexState).call = nil
		}
	case 97:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1850
		{
			yyDollar[1].ast.right = &ast{
				yy_tok: WHEN,
				left: &ast{
					yy_tok: yy_TRUE,
				},
			}
			yylex.(*yyLexState).sql_query_row = nil
			yylex.(*yyLexState).sql_exec = nil
		}
	case 98:
		yyDollar = yyS[yypt-4 : yypt+1]
		//line parser.y:1862
		{
			yyDollar[1].ast.right = &ast{
				yy_tok: WHEN,
				left:   yyDollar[3].ast,
			}
			yylex.(*yyLexState).sql_query_row = nil
			yylex.(*yyLexState).sql_exec = nil
		}
	case 99:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1872
		{
			l := yylex.(*yyLexState)
			l.sql_database = &sql_database{
				name: yyDollar[3].string,
			}
		}
	case 100:
		yyDollar = yyS[yypt-7 : yypt+1]
		//line parser.y:1878
		{
			l := yylex.(*yyLexState)
			if l.sql_database.driver_name == "" {
				l.error("sql database: %s: driver_name is required",
					yyDollar[3].string)
				return 0
			}
			yyVAL.ast = &ast{
				yy_tok:       SQL_DATABASE,
				sql_database: l.sql_database,
			}
			l.config.sql_database[yyDollar[3].string] = l.sql_database
			l.sql_database = nil
		}
	case 101:
		yyDollar = yyS[yypt-4 : yypt+1]
		//line parser.y:1894
		{
			l := yylex.(*yyLexState)
			l.sql_query_row = &sql_query_row{
				name:       yyDollar[3].string,
				result_row: make([]sql_query_result_row, 0),
				name2result: make(
					map[string]*sql_query_result_row),
			}
		}
	case 102:
		yyDollar = yyS[yypt-8 : yypt+1]
		//line parser.y:1903
		{
			l := yylex.(*yyLexState)
			q := l.sql_query_row

			grump := func(format string, args ...interface{}) int {
				l.error("sql query row: %s: %s", q.name,
					Sprintf(format, args...))
				return 0
			}

			if q.statement == "" {
				return grump("missing statement declaration")
			}

			//  unambiguously determine which database to bind to query

			if q.sql_database == nil {
				switch {
				case len(l.config.sql_database) == 1:
					for _, db := range l.config.sql_database {
						q.sql_database = db
					}
				case len(l.config.sql_database) == 0:
					return grump("no database defined")
				default:
					return grump("can't determine database")
				}
			}
			if len(q.result_row) == 0 {
				grump("missing result row declaration")
			}
			l.config.sql_query_row[yyDollar[3].string] = l.sql_query_row
			l.sql_query_row = nil
		}
	case 103:
		yyDollar = yyS[yypt-3 : yypt+1]
		//line parser.y:1939
		{
			l := yylex.(*yyLexState)
			l.sql_exec = &sql_exec{
				name: yyDollar[3].string,
			}
		}
	case 104:
		yyDollar = yyS[yypt-7 : yypt+1]
		//line parser.y:1945
		{
			l := yylex.(*yyLexState)
			ex := l.sql_exec

			grump := func(format string, args ...interface{}) int {
				l.error("sql exec: %s: %s", ex.name,
					Sprintf(format, args...))
				return 0
			}

			if len(ex.statement) == 0 {
				return grump("missing statement declaration")
			}

			if ex.sql_database == nil {
				switch {
				case len(l.config.sql_database) == 1:
					for _, db := range l.config.sql_database {
						ex.sql_database = db
					}
				case len(l.config.sql_database) == 0:
					return grump("no database defined")
				default:
					return grump("can't determine database")
				}
			}

			l.config.sql_exec[yyDollar[3].string] = l.sql_exec
			l.sql_exec = nil
		}
	case 105:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:1981
		{
			yyVAL.go_kind = reflect.Bool
		}
	case 106:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:1988
		{
			l := yylex.(*yyLexState)
			q := l.sql_query_row

			if q.name2result[yyDollar[1].string] != nil {
				l.error("sql query row: %s: attribute redefined: %s",
					q.name, yyDollar[1].string)
				return 0
			}

			if len(q.result_row) == 16 {
				l.error("sql query row: %s: too many attributes: > 16",
					q.name)
				return 0
			}
			rr := &sql_query_result_row{
				name:    yyDollar[1].string,
				go_kind: yyDollar[2].go_kind,
				offset:  uint8(len(q.result_row)),
			}
			q.result_row = append(q.result_row, *rr)
			q.name2result[yyDollar[1].string] = rr
		}
	case 109:
		yyDollar = yyS[yypt-1 : yypt+1]
		//line parser.y:2021
		{
			yylex.(*yyLexState).ast_root = yyDollar[1].ast
		}
	case 110:
		yyDollar = yyS[yypt-2 : yypt+1]
		//line parser.y:2026
		{
			s := yyDollar[1].ast
			for ; s.next != nil; s = s.next {
			} //  find last stmt

			s.next = yyDollar[2].ast
		}
	}
	goto yystack /* stack new state and value */
}
