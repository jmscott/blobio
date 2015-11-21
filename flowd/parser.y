/*
 *  Synopsis:
 *	Yacc grammar for 'flow' language.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 *  Note:
 *	Query arguments are always strings.  Not too much of a problem in
 *	the short run, since the cast can be added in the query, put
 *	eventually ought to be added to the sql query/exec definition.
 *
 *	Think about limiting the number of call/queries to < 2^16 -1.
 *	Limiting may allow interesting optimizations.  What about limiting
 *	to < 256?
 *
 *	Apologies for overloading the yy_token with values not explicitly
 *	in the grammar.  For example, CAST_STRING and ARGV0.  Also,
 *	how do we determine the lowest valued yy_token.  START plays
 *	that role, but an explicit value would be nice.  See ast.String().
 *
 *	Need to rename max_idle_conns to sql_max_idle_conns.
 *
 *	Think about changing syntax of
 *	
 *		exit_status is OK when in {...};
 *	to
 *		OK when exit_status in {...};
 *
 *	Second version allows adding more qualifications to OK.
 *
 *	How to index symbol array yyToknames[a.yy_tok-__MIN_YYTOK]?
 *
 *	Multiple statements in an sql exec imply a transaction.
 *	Would an explicit keyword be better?
 *
 *	sql queries and commands need separate name space for attributes.
 *	for example, the following code breaks in wierd ways:
 *
 *		sql query abc row {
 *			result row is (is_xml bool)
 *			...
 *		command ls_xml { ...
 */
%{
package main

import (
	"bufio"
	"errors"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"reflect"
	"strconv"
	"strings"
	"syscall"
	"unicode"
	"unicode/utf8"

	. "time"
	. "fmt"
)

func init() {
	if yyToknames[3] != "__MIN_YYTOK" {
		panic("yyToknames[3] != __MIN_YYTOK: yacc may have changed")
	}
}

//  describes behavior of brr flow server

type config struct {
	//  path to config file
	path			string

	//  maximum number of blob requests records in queue
	brr_capacity		uint16

	//  maximum number of flows competing for blob request records 
	flow_worker_count	uint16

	//  maximum number of worker requests in queue
	os_exec_capacity	uint16

	//  number of workers competing for requests
	os_exec_worker_count	uint16

	//  duration between rolls of os exec detail record logs
	xdr_roll_duration	Duration

	//  duration between rolls of query detail record logs
	qdr_roll_duration	Duration

	//  duration between rolls of flow detail record logs
	fdr_roll_duration	Duration

	//  duration between any heartbeat in log file
	heartbeat_duration	Duration

	//  duration between calling runtime.ReadMemStats()
	memstats_duration	Duration

	//  sole tail
	*tail

	//  sql database <name>
	sql_database	map[string]*sql_database

	//  sql query <name> row (...)
	sql_query_row	map[string]*sql_query_row

	//  sql exec <name>
	sql_exec	map[string]*sql_exec

	//  all os commands{}
	command          	map[string]*command

	//  defaults to "log"
	log_directory		string
}

//  abstract syntax tree that represents the config file
type ast struct {

	yy_tok	int
	bool
	string
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
	left	*ast
	right	*ast

	//  siblings
	next	*ast
}

type parse struct {
	
	//  configuration file the parse is derived from
	*config

	ast_root	*ast

	//  dependency order of call() and query() nodes
	depend_order	[]string

	//  map of a call() statements to the branch of it's abstract
	//  syntax tree

	call2ast	map[string]*ast

	//  map of a query() statements to the branch of it's abstract
	//  syntax tree

	query2ast	map[string]*ast
}

const (
	max_argv = 16
	max_command_name = 128

	max_name_rune_count = 64
)

%}

%union {
	uint64				
	string
	tail		*tail
	command		*command
	call		*call
	sql_database	*sql_database
	sql_query_row	*sql_query_row
	sql_exec	*sql_exec
	string_list	[]string
	ast		*ast
	brr_field	brr_field
	go_kind		reflect.Kind
}

//  lowest numbered yytoken.  must be first in list.
%token	__MIN_YYTOK

%token	ARGV
%token	ARGV0
%token	ARGV1
%token	BLOB_SIZE
%token	BOOT
%token	BRR_CAPACITY
%token	CALL
%token	CALL0
%token	CAST_STRING
%token	CAST_UINT64
%token	CHAT_HISTORY
%token	COMMAND
%token	COMMAND_REF
%token	DATABASE
%token	DATA_SOURCE_NAME
%token  TRACE_BOOL
%token	DRIVER_NAME
%token	EQ
%token	EQ_BOOL
%token	EQ_STRING
%token	EQ_UINT64
%token	EXIT_STATUS
%token	FDR_ROLL_DURATION
%token	FLOW_WORKER_COUNT
%token	FROM
%token	HEARTBEAT_DURATION
%token	IN
%token	IS
%token	LOG_DIRECTORY
%token	MAX_IDLE_CONNS
%token	MAX_OPEN_CONNS
%token	MEMSTAT_DURATION
%token	NAME
%token	NEQ
%token	NEQ_BOOL
%token	NEQ_STRING
%token	NEQ_UINT64
%token	NETFLOW
%token	OS_EXEC_CAPACITY
%token	OS_EXEC_WORKER_COUNT
%token	PARSE_ERROR
%token	PATH
%token	PROJECT_BRR
%token	PROJECT_QDR_ROWS_AFFECTED
%token	PROJECT_QDR_SQLSTATE
%token	PROJECT_SQL_QUERY_ROW_BOOL
%token	PROJECT_XDR_EXIT_STATUS
%token	QDR_ROLL_DURATION
%token  QUERY
%token	QUERY_DURATION
%token	QUERY_EXEC
%token	QUERY_EXEC_TXN
%token	QUERY_ROW
%token	RESULT
%token	ROW
%token	ROWS_AFFECTED
%token	SQL
%token	SQL_DATABASE
%token	SQL_DATABASE_REF
%token	SQL_EXEC_REF
%token	SQL_QUERY_ROW_REF
%token	SQLSTATE
%token	START_TIME
%token	STATEMENT
%token	STRING
%token	TAIL
%token	TAIL_REF
%token	TRANSACTION
%token	UDIG
%token	UINT64
%token	VERB
%token	WALL_DURATION
%token	WHEN
%token	XDR_ROLL_DURATION
%token	yy_AND
%token  yy_BOOL
%token	yy_EXEC
%token	yy_FALSE
%token	yy_INT64
%token	yy_OK
%token	yy_OR
%token	yy_TRUE

%type	<uint64>	UINT64
%type	<string>	STRING
%type	<string>	NAME
%type	<ast>		statement_list  statement
%type	<string>	boot_capacity  boot_duration
%type	<string_list>	string_list
%type	<ast>		constant  rel_op
%type	<ast>		tail_project  projection  tail_rel
%type	<ast>		arg arg_list
%type	<ast>		qualify
%type	<sql_database>	SQL_DATABASE_REF
%type	<sql_query_row>	SQL_QUERY_ROW_REF
%type	<sql_exec>	SQL_EXEC_REF
%type	<command>	COMMAND_REF
%type	<tail>		TAIL_REF
%type	<ast>		call  query
%type	<ast>		call_project  query_project
%type	<brr_field>	brr_field
%type	<go_kind>	sql_result_type

%%

configure:
	  statement_list
	;

boot_capacity:
	  //  BRR_CAPACITY should be in the tail definition, not the boot{}
	  BRR_CAPACITY
	  {
		l := yylex.(*yyLexState)
		if l.seen_brr_capacity {
			l.error("boot: can not set brr_capacity again")
			return 0
		}
		l.seen_brr_capacity = true
	  	$$ = "brr_capacity"
	  }
	|
	  OS_EXEC_CAPACITY
	  {
		l := yylex.(*yyLexState)
		if l.seen_os_exec_capacity {
			l.error("boot: can not set os_exec_capacity again")
			return 0
		}
		l.seen_os_exec_capacity = true
	  	$$ = "os_exec_capacity"
	  }
	|
	  OS_EXEC_WORKER_COUNT
	  {
		l := yylex.(*yyLexState)
		if l.seen_os_exec_worker_count {
			l.error("boot: can not set os_exec_worker_count again")
			return 0
		}
		l.seen_os_exec_worker_count = true
	  	$$ = "os_exec_worker_count"
	  }
	|
	  FLOW_WORKER_COUNT
	  {
		l := yylex.(*yyLexState)
		if l.seen_flow_worker_count {
			l.error("boot: can not set flow_worker_count again")
			return 0
		}
		l.seen_flow_worker_count = true
	  	$$ = "flow_worker_count"
	  }
	;

boot_duration:
	  FDR_ROLL_DURATION
	  {
		l := yylex.(*yyLexState)
		if l.seen_fdr_roll_duration {
			l.error("boot: can not set fdr_roll_duration again")
			return 0
		}
		l.seen_fdr_roll_duration = true
	  	$$ = "fdr_roll_duration"
	  }
	|
	  XDR_ROLL_DURATION
	  {
		l := yylex.(*yyLexState)
		if l.seen_xdr_roll_duration {
			l.error("boot: can not set xdr_roll_duration again")
			return 0
		}
		l.seen_xdr_roll_duration = true
	  	$$ = "xdr_roll_duration"
	  }
	|
	  QDR_ROLL_DURATION
	  {
		l := yylex.(*yyLexState)
		if l.seen_qdr_roll_duration {
			l.error("boot: can not set qdr_roll_duration again")
			return 0
		}
		l.seen_qdr_roll_duration = true
	  	$$ = "qdr_roll_duration"
	  }
	|
	  HEARTBEAT_DURATION
	  {
		l := yylex.(*yyLexState)
		if l.seen_heartbeat_duration {
			l.error("boot: can not set heartbeat_duration again")
			return 0
		}
		l.seen_heartbeat_duration = true
	  	$$ = "heartbeat_duration"
	  }
	|
	  MEMSTAT_DURATION
	  {
		l := yylex.(*yyLexState)
		if l.seen_memstats_duration {
			l.error("boot: can not set memstats_duration again")
			return 0
		}
		l.seen_memstats_duration = true
	  	$$ = "memstats_duration"
	  }
	;

boot_stmt:
	  boot_duration  '='  STRING
	  {
		l := yylex.(*yyLexState)

		d, err := ParseDuration($3)
		if err != nil {
			l.error("boot: %s: %s", $1, err)
			return 0
		}
		if d == 0 {
			l.error("boot: %s: duration can not be 0", $1)
			return 0
		}
		switch {

		case $1 == "fdr_roll_duration":
			l.config.fdr_roll_duration = d

		case $1 == "xdr_roll_duration":
			l.config.xdr_roll_duration = d

		case $1 == "qdr_roll_duration":
			l.config.qdr_roll_duration = d

		case $1 == "heartbeat_duration":
			l.config.heartbeat_duration = d

		case $1 == "memstats_duration":
			l.config.memstats_duration = d

		default:
			panic("boot: impossible duration variable: " + $1)
		}
	  }
	|
	  boot_capacity  '='  UINT64
	  {
		l := yylex.(*yyLexState)

	  	cap64 := $3
		if cap64 > 65535 {
			l.error("boot: %s too big: > 65535: %d", $1, cap64)
			return 0
		}
		cap := uint16(cap64)
		switch {
		case $1 == "brr_capacity":
			l.config.brr_capacity = cap

		case $1 == "os_exec_capacity":
			l.config.os_exec_capacity = cap

		case $1 == "os_exec_worker_count":
			l.config.os_exec_worker_count = cap

		case $1 == "flow_worker_count":
			l.config.flow_worker_count = cap

		default:
			panic("boot: impossible capacity variable: " + $1)
		}
	  }
	|
	  LOG_DIRECTORY  '='  STRING
	  {
		l := yylex.(*yyLexState)

		if $3 == "" {
			l.error("boot: log_directory is empty")
			return 0
		}

		if l.config.log_directory != "" {
			l.error("boot: log_directory set again")
			return 0
		}
		l.config.log_directory = $3
	  }
	;

boot_stmt_list:
	  /*
	   *  What about terminating the statement here?
	   */
	  boot_stmt  ';'
	|
	  boot_stmt_list  boot_stmt  ';'
	;

string_list:
	  STRING
	  {
		sl := make([]string, 1)
		sl[0] = $1
		$$ = sl
	  }
	|
	  string_list  ','  STRING
	  {
		$$ = append($1, $3)
		if len($$) >= max_argv {
			l := yylex.(*yyLexState)

			l.error("argv > %d strings", max_argv)
			return 0
		}
	  }
	;

constant:
	  UINT64
	  {
	  	$$ = &ast{
			yy_tok:	UINT64,
			uint64:		$1,
		}
	  }
	|
	  STRING
	  {
	  	$$ = &ast{
			yy_tok:		STRING,
			string:		$1,
		}
	  }
	|
	  yy_TRUE
	  {
	  	$$ = &ast{
			yy_tok:	yy_TRUE,
			bool:	true,
		}
	  }
	|
	  yy_FALSE
	  {
	  	$$ = &ast{
			yy_tok:	yy_FALSE,
			bool:	false,
		}
	  }
	;

rel_op:
	  EQ
	  {
	  	$$ = &ast{
			yy_tok:	EQ,
		};
	  }
	|
	  NEQ
	  {
	  	$$ = &ast{
			yy_tok:	NEQ,
		};
	  }
	;

tail_rel:
	  TAIL_REF  '.'
	  {
	  	$$ = &ast{
			yy_tok:	PROJECT_BRR,
			tail:	$1,
		}
	  }
	;

call_project:
	  COMMAND_REF  '.'  EXIT_STATUS
	  {
		l := yylex.(*yyLexState)

		//  the call() must occur before the reference to exit_status
		if $1.called == false {
			l.error(
			     "command referenced before called: %s.exit_status",
			     $1.name,
			)
		}

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
		if $1.depend_ref_count == 255 {
			l.error("%s: too many dependencies: > 255", $1.name)
			return 0
		}
		$1.depend_ref_count++

		//  is trivial loopback?  gnu tsort for some reason treats
		//  these as island nodes and not loopbacks.

		if subject == $1.name {
			l.error("%s '%s' refers to itself", what, $1.name)
			return 0
		}

		//  record for detection of cycles in the invocation graph

		l.depends = append(
				l.depends,
				Sprintf("%s %s", subject, $1.name),
			)

	  	$$ = &ast{
			yy_tok:	PROJECT_XDR_EXIT_STATUS,
			string:	$1.name,
			command:$1,
		}
	  }
	;

query_project:
	  SQL_QUERY_ROW_REF  '.'  NAME
	  {
		l := yylex.(*yyLexState)

		r := $1.name2result[$3]
		if r == nil {
			l.error("sql query row: %s: unknown attribute: %s",
							$1.name, $3)
			return 0
		}

		//  the query() qualification must occur before the reference
		if $1.called == false {
			l.error("sql query row referenced before called: %s.%s",
							$1.name, $3)
			return 0
		}

		//  record dependency between query_project and statement.
		//
		//  gnu tsort treats nodes that point to themselves as islands,
		//  not loops, so check here that we don't point to our selves.

		var subject, what string

		name := $1.name

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
		if $1.depend_ref_count == 255 {
			l.error("%s: %s: too many dependencies: > 255",
							what, name)
			return 0
		}
		$1.depend_ref_count++

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

	  	$$ = &ast{
			yy_tok:		PROJECT_SQL_QUERY_ROW_BOOL,
			string:		$1.name,
			uint8:	        r.offset,
		}
	  }
	|
	  SQL_QUERY_ROW_REF  '.'  ROWS_AFFECTED
	  {
		l := yylex.(*yyLexState)

		//  the query() qualification must occur before the reference
		if $1.called == false {
			l.error("sql query row referenced before called: %s",
							$1.name)
			return 0
		}

		//  record dependency between query_project and statement.
		//
		//  gnu tsort treats nodes that point to themselves as islands,
		//  not loops, so check here that we don't point to our selves.

		var subject, what string

		name := $1.name

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
		if $1.depend_ref_count == 255 {
			l.error("%s: %s: too many dependencies: > 255",
							what, name)
			return 0
		}
		$1.depend_ref_count++

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

	  	$$ = &ast{
			yy_tok:		PROJECT_QDR_ROWS_AFFECTED,
			string:		$1.name,
		}
	  }
	|
	  SQL_QUERY_ROW_REF  '.'  SQLSTATE
	  {
		l := yylex.(*yyLexState)

		//  the query() qualification must occur before the reference
		if $1.called == false {
			l.error("sql query row referenced before called: %s",
							$1.name)
			return 0
		}

		//  record dependency between query_project and statement.
		//
		//  gnu tsort treats nodes that point to themselves as islands,
		//  not loops, so check here that we don't point to our selves.

		var subject, what string

		name := $1.name

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
		if $1.depend_ref_count == 255 {
			l.error("%s: %s: too many dependencies: > 255",
							what, name)
			return 0
		}
		$1.depend_ref_count++

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

	  	$$ = &ast{
			yy_tok:	PROJECT_QDR_SQLSTATE,
			string:	$1.name,
		}
	  }
	|
	  SQL_EXEC_REF  '.'  ROWS_AFFECTED
	  {
		l := yylex.(*yyLexState)

		//  the exec() qualification must occur before the reference
		if $1.called == false {
			l.error("sql exec referenced before called: %s",$1.name)
			return 0
		}

		//  record dependency between query_project and statement.
		//
		//  gnu tsort treats nodes that point to themselves as islands,
		//  not loops, so check here that we don't point to our selves.

		var subject, what string

		name := $1.name

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
		if $1.depend_ref_count == 255 {
			l.error("%s: %s: too many dependencies: > 255",
							what, name)
			return 0
		}
		$1.depend_ref_count++

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

	  	$$ = &ast{
			yy_tok:	PROJECT_QDR_ROWS_AFFECTED,
			string:	$1.name,
		}
	  }
	|
	  SQL_EXEC_REF  '.'  SQLSTATE
	  {
		l := yylex.(*yyLexState)

		//  the exec() qualification must occur before the reference
		if $1.called == false {
			l.error("sql exec referenced before called: %s",
								$1.name)
			return 0
		}

		//  record dependency between query_project and statement.
		//
		//  gnu tsort treats nodes that point to themselves as islands,
		//  not loops, so check here that we don't point to our selves.

		var subject, what string

		name := $1.name

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
		if $1.depend_ref_count == 255 {
			l.error("%s: %s: too many dependencies: > 255",
							what, name)
			return 0
		}
		$1.depend_ref_count++

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

	  	$$ = &ast{
			yy_tok:	PROJECT_QDR_SQLSTATE,
			string:	$1.name,
		}
	  }
	;

brr_field:
	  UDIG
	  {
	  	$$ = brr_field(brr_UDIG)
	  }
	|
	  CHAT_HISTORY
	  {
	  	$$ = brr_field(brr_CHAT_HISTORY)
	  }
	|
	  START_TIME
	  {
	  	$$ = brr_field(brr_START_TIME)
	  }
	|
	  WALL_DURATION
	  {
	  	$$ = brr_field(brr_WALL_DURATION)
	  }
	|
	  VERB
	  {
	  	$$ = brr_field(brr_VERB)
	  }
	|
	  NETFLOW
	  {
	  	$$ = brr_field(brr_NETFLOW)
	  }
	|
	  BLOB_SIZE
	  {
	  	$$ = brr_field(brr_BLOB_SIZE)
	  }
	;

tail_project:
	  tail_rel  NAME
	  {
		l := yylex.(*yyLexState)
		l.error("%s: unknown tail attribute: %s", $1.tail.name, $2)
		return 0
	  }
	|
	  tail_rel  EXIT_STATUS
	  {
		l := yylex.(*yyLexState)
		l.error("%s: exit_status is not a tail attribute", $1.tail.name)
		return 0
	  }
	|
	  tail_rel  brr_field
	  {
	  	$1.brr_field = $2

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
				Sprintf("%s %s", subject, $1.tail.name,
			))
	  }
	;

projection:
	  tail_project
	|
	  call_project
	;

qualify:
	  projection  rel_op  constant
	  {
		l := yylex.(*yyLexState)
		left := $1

	  	//  verify the comparisons make sense

		//  verify that the PROJECT_BRR or command.exit_status
		//  are compared to a constant of a matching type.

		if $1.yy_tok == PROJECT_BRR {

			//  if the brr field is a blob size compared to a
			//  uint, then reparent the with a CAST_UINT64 node

			if $1.brr_field == brr_BLOB_SIZE {
				if $3.yy_tok != UINT64 {
					l.error(
					  "%s.blob_size not compared to uint64",
						  $1.tail.name)
					return 0
				}
				left = &ast{
						yy_tok:	CAST_UINT64,
						left:	left,
				}
				if $2.yy_tok == EQ {
					$2.yy_tok = EQ_UINT64
				} else {
					$2.yy_tok = NEQ_UINT64
				}
				$2.uint64 = $3.uint64
			} else {
				if $3.yy_tok != STRING {
					l.error("%s.%s not compared to string",
						$1.tail.name, $1.brr_field)
					return 0
				}
				if $2.yy_tok == EQ {
					$2.yy_tok = EQ_STRING
				} else {
					$2.yy_tok = NEQ_STRING
				}
				$2.string = $3.string
			}
		} else {
			//  <command>.exit_status == ...

			if $3.yy_tok != UINT64 {
				l.error("%s.exit_status not compared to uint64",
						  $1.string)
				return 0
			}
			if $2.yy_tok == EQ {
				$2.yy_tok = EQ_UINT64
			} else {
				$2.yy_tok = NEQ_UINT64
			}
			$2.uint64 = $3.uint64
		}
	  	$2.left = left;
	  	$2.right = nil

		$$ = $2
	  }
	|
	  //  Note: query_project needs to merge into projection rule
	  query_project  rel_op  constant
	  {
		l := yylex.(*yyLexState)
		q := $1.sql_query_row

		rel_tok := $2.yy_tok

		switch {
		case $1.is_bool() && $3.is_bool():
			if rel_tok == EQ {
				rel_tok = EQ_BOOL
			} else {
				rel_tok = NEQ_BOOL
			}
			$$ = &ast{
				yy_tok:		rel_tok,

				bool:		$3.bool,
				left:		$1,
			}
		case $1.is_uint64() && $3.is_uint64():
			if rel_tok == EQ {
				rel_tok = EQ_UINT64
			} else {
				rel_tok = NEQ_UINT64
			}
			$$ = &ast{
				yy_tok:		rel_tok,

				uint64:		$3.uint64,
				left:		$1,
			}
		case $1.is_string() && $3.is_string():
			if rel_tok == EQ {
				rel_tok = EQ_STRING
			} else {
				rel_tok = NEQ_STRING
			}
			$$ = &ast{
				yy_tok:		rel_tok,

				string:		$3.string,
				left:		$1,
			}
		default:
			l.error("%s: comparisons are different types", q.name)
			return 0
		}
	  }
	|
	  '('  qualify   ')'
	  {
	  	$$ = $2
	  }
	|
	  qualify  yy_AND  qualify
	  {
		$$ = &ast {
			yy_tok:	yy_AND,
			left:	$1,
			right:	$3,
		}
	  }
	|
	  qualify  yy_OR  qualify
	  {
		$$ = &ast {
			yy_tok:	yy_OR,
			left:	$1,
			right:	$3,
		}
	  }
	;

arg:
	  projection
	|
	  constant
	;

arg_list:
	  /* empty */
	  {
	  	$$ = nil
	  }
	|
	  arg
	|
	  arg_list  ','  arg
	  {
		a := $1

		//  find the tail of the list
		for ;  a.next != nil;  a = a.next {}

		a.next = $3
	  }
	;

call:
	  CALL  NAME
	  {
		l := yylex.(*yyLexState)
		l.error("unknown command: '%s'", $2)
		return 0
	  }
	|
	  CALL  COMMAND_REF  
	  {
	  	l := yylex.(*yyLexState)
		l.call = &call{
				command:	$2,
			}
	  }
	  '('  arg_list ')'
	  {
		l := yylex.(*yyLexState)
		cmd := $2

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
		for a := $5;  a != nil;  a = a.next {
			argc++
			if a.is_uint64() {
				has_uint64 = true
			}
		}

		//  the 'call(args ...)' expects all args to be strings,
		//  so reparent any is_uint64() nodes with CAST_STRING node
		//  and rewire the link list of arguments nodes.

		argv := $5
		if has_uint64 {

			al := make([]*ast, argc)

			//  copy nodes to temporary list, reparenting
			//  is_uint64() nodes

			for a, i := $5, 0;  a != nil;  a = a.next {
				if a.is_uint64() {
					al[i] = &ast{
							yy_tok:	CAST_STRING,
							left:	a,
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

		$$ = &ast{
			yy_tok:	ctok,
			call:	call,
			left:	&ast{
					yy_tok:	tok,
					left:	argv,
					uint64:	argc,
			},
		}
		l.call2ast[call.command.name] = $$
	  }
	;

query:
	  QUERY  NAME
	  {
		l := yylex.(*yyLexState)
		l.error("unknown query: '%s'", $2)
		return 0
	  }
	|
	  QUERY  SQL_QUERY_ROW_REF  
	  {
	  	l := yylex.(*yyLexState)
		l.sql_query_row = $2
	  }
	  '('  arg_list ')'
	  {
		l := yylex.(*yyLexState)
		q := $2

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
		for a := $5;  a != nil;  a = a.next {
			argc++
			if a.is_uint64() {
				has_uint64 = true
			}
		}

		//  the 'query(args ...)' expects all args to be strings,
		//  so reparent any is_uint64() nodes with CAST_STRING node
		//  and rewire the link list of arguments nodes.

		argv := $5
		if has_uint64 {

			al := make([]*ast, argc)

			//  copy nodes to temporary list, reparenting
			//  is_uint64() nodes

			for a, i := $5, 0;  a != nil;  a = a.next {
				if a.is_uint64() {
					al[i] = &ast{
							yy_tok:	CAST_STRING,
							left:	a,
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

		$$ = &ast{
			yy_tok:	QUERY_ROW,
			sql_query_row:	q,
			left:	&ast{
					yy_tok:	tok,
					left:	argv,
					uint64:	argc,
			},
		}
		l.query2ast[q.name] = $$
	  }
	|
	  QUERY  SQL_EXEC_REF  
	  {
	  	l := yylex.(*yyLexState)
		l.sql_exec = $2
	  }
	  '('  arg_list ')'
	  {
		l := yylex.(*yyLexState)
		ex := $2

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
		for a := $5;  a != nil;  a = a.next {
			argc++
			if a.is_uint64() {
				has_uint64 = true
			}
		}

		//  the 'query(args ...)' expects all args to be strings,
		//  so reparent any is_uint64() nodes with CAST_STRING node
		//  and rewire the link list of arguments nodes.

		argv := $5
		if has_uint64 {

			al := make([]*ast, argc)

			//  copy nodes to temporary list, reparenting
			//  is_uint64() nodes

			for a, i := $5, 0;  a != nil;  a = a.next {
				if a.is_uint64() {
					al[i] = &ast{
							yy_tok:	CAST_STRING,
							left:	a,
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

		$$ = &ast{
			yy_tok:	q_tok,
			sql_exec:	ex,
			left:	&ast{
					yy_tok:	tok,
					left:	argv,
					uint64:	argc,
			},
		}
		l.query2ast[ex.name] = $$
	  }
	;

cmd_stmt:
	  PATH  '='  STRING			//  string can't be ""
	  {
		l := yylex.(*yyLexState)
		cmd := l.command
		if cmd.path != "" {
			l.error("command: %s: path can not be set again",
								cmd.name)
			return 0
		}
		if $3 == "" {
			l.error("command: %s: path can not be empty", cmd.name)
			return 0
		}
		cmd.path = $3
	  }
	|
	  ARGV  {
		l := yylex.(*yyLexState)
		cmd := l.command

		if cmd.argv != nil {
			l.error("command %s: argv defined again", cmd.name)
			return 0
		}
	  }  '='  '('  string_list  ')'
	  {
		yylex.(*yyLexState).command.argv = $5
	  }
	|
	  //  Note: shouldn't the when clause be a true qualification?
	  //        when exit_status in {0, 1}.
	  EXIT_STATUS  IS  yy_OK  WHEN  IN 
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
	  '{'  exit_status_list  '}'
	;

exit_status_list:
	  UINT64
	  {
		l := yylex.(*yyLexState)

		if $1 > 255 {
			l.error("exit status > 255")
			return 0
		}
		u8 := uint8($1)

		l.command.OK_exit_status[u8 / 8] |= 0x1 << (u8 % 8)
	  }
	|
	  exit_status_list  ','  UINT64
	  {
		l := yylex.(*yyLexState)
		if $3 > 255 {
			l.error("exit status > 255")
			return 0
		}
		u8 := uint8($3)

		/*
		 *  Logical and in the bit.
		 */
		l.command.OK_exit_status[u8 / 8] |= 0x1 << (u8 % 8)
	  }
	;

sqlstate_list:
	  STRING
	  {
		if !(yylex.(*yyLexState)).put_sqlstate($1) {
			return 0
		}
	  }
	|
	  sqlstate_list  ','  STRING
	  {
		if !(yylex.(*yyLexState)).put_sqlstate($3) {
			return 0
		}
	  }
	;

cmd_stmt_list:
	  cmd_stmt  ';'
	|
	  cmd_stmt_list  cmd_stmt  ';'
	;

sqldb_stmt:
	  DRIVER_NAME  '='  STRING
	  {
		l := yylex.(*yyLexState)
		if l.seen_driver_name {
			l.error("sql database: %s: driver_name redefined",
							l.sql_database.name)
			return 0
		}
		l.seen_driver_name = true

		if $3 == "" {
			l.error("sql database: empty driver")
			return 0
		}
		l.sql_database.driver_name = $3
	  }
	|
	  DATA_SOURCE_NAME  '='  STRING
	  {
		l := yylex.(*yyLexState)
		if l.seen_data_source_name {
			l.error("sql database: %s: data_source_name redefined",
							l.sql_database.name)
			return 0
		}
		l.seen_data_source_name = true

		if $3 == "" {
			l.error("sql database: empty driver")
			return 0
		}
		l.sql_database.data_source_name = $3
	  }
	|
	  MAX_IDLE_CONNS  '='  UINT64
	  {
		l := yylex.(*yyLexState)
		if l.seen_max_idle_conns {
			l.error(
			     "sql database: %s: max_open_connections redefined",
							l.sql_database.name)
			return 0
		}
		l.seen_max_idle_conns = true
		l.sql_database.max_idle_conns = int($3)
	  }
	|
	  MAX_OPEN_CONNS  '='  UINT64
	  {
		l := yylex.(*yyLexState)
		if l.seen_max_open_conns {
			l.error(
			     "sql database: %s: max_open_connections redefined",
							l.sql_database.name)
			return 0
		}
		l.seen_max_open_conns = true
		l.sql_database.max_open_conns = int($3)
	  }
	;

sqldb_stmt_list:
	  sqldb_stmt  ';'
	|
	  sqldb_stmt_list  sqldb_stmt  ';'
	;

sql_decl_stmt:
	  DATABASE  IS  NAME
	  {
		l := yylex.(*yyLexState)
		l.error("unknown database: %s",  $3)
		return 0
	  }
	|
	  DATABASE  IS  SQL_DATABASE_REF
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
			q.sql_database = $3

		//  statement is in an 'sql exec <name>' block
		case l.sql_exec != nil:
			ex := l.sql_exec
			if ex.sql_database != nil {
				l.error("sql exec: %s: database redefined",
									ex.name)
				return 0
			}
			ex.sql_database = $3
		default:
			panic("sql database: " +
			      "both sql_query_row and sql_exec are nil")
		}
	  }
	|
	  STATEMENT  '='  STRING
	  {
		l := yylex.(*yyLexState)
		if $3 == "" {
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
			q.statement = $3

		//  statement is in an 'sql exec <name>' block
		case l.sql_exec != nil:
			ex := l.sql_exec
			if ex.statement != nil {
				l.error("sql exec: %s: statement redefined",
								ex.name)
				return 0
			}
			ex.statement = append(ex.statement, $3)
		default:
			panic("both sql_query_row and sql_exec are nil")
		}
	  }
	|
	  STATEMENT  '='  '('  string_list  ')'
	  {
		l := yylex.(*yyLexState)

		if l.sql_query_row != nil {
			l.error("sql query row: %s: transaction not allowed")
			return 0
		}
		ex := l.sql_exec
		if ex.statement != nil {
			l.error("sql exec txn: %s: statement redefined",
							ex.name)
			return 0
		}
		ex.statement = make([]string, len($4))
		for i, s := range $4 {
			ex.statement[i] = s
		}
	  }
	|
	  RESULT  ROW  IS
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
	  '('  sql_result_list  ')'
	|
	  SQLSTATE  IS  yy_OK  WHEN  IN  '{'  sqlstate_list  '}'
	;

sql_decl_stmt_list:
	  sql_decl_stmt  ';'
	|
	  sql_decl_stmt_list  sql_decl_stmt  ';'
	;

statement:
	  BOOT {
		l := yylex.(*yyLexState)
		if l.seen_boot {
			l.error("boot{...} defined again")
			return 0
		}
		l.seen_boot = true
		l.in_boot = true
	  } '{'
	  	boot_stmt_list
	  '}'
	  {
		yylex.(*yyLexState).in_boot = false
		$$ = &ast{
			yy_tok:	BOOT,
		}
	  }
	|
	  //  tail must have at least a path
	  TAIL  NAME  '{'  PATH  '='  STRING  ';'  '}'
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
			name:		$2,
			path:		$6,
		}
		$$ = &ast{
			yy_tok:	TAIL,
			tail:	l.config.tail,
		}
	  }
	|
	  COMMAND
	  {
		l := yylex.(*yyLexState)
		if l.command != nil {
			panic("command: yyLexState.command != nil")
		}
		l.command = &command{};
		$<command>$ = l.command

	  } NAME {$<command>2.name = $3}  '{'  cmd_stmt_list  '}'
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

		l.config.command[name] = $<command>2
		l.command = nil
		$$ = &ast{
			yy_tok:		COMMAND,
			command:	$<command>2,
		}
	  }
	|
	  call  ';'
	  {
		$1.right = &ast{
				yy_tok:	WHEN,
				left:	&ast {
						yy_tok: yy_TRUE,
				},
			}
	  	yylex.(*yyLexState).call = nil
	  }
	|
	  call  WHEN  qualify  ';'
	  {
		$1.right = &ast{
				yy_tok:	WHEN,
				left:	$3,
			}
	  	yylex.(*yyLexState).call = nil
	  }
	|
	  query  ';'
	  {
		$1.right = &ast{
				yy_tok:	WHEN,
				left:	&ast {
						yy_tok: yy_TRUE,
				},
			}
	  	yylex.(*yyLexState).sql_query_row = nil
	  	yylex.(*yyLexState).sql_exec = nil
	  }
	|
	  query  WHEN  qualify  ';'
	  {
		$1.right = &ast{
				yy_tok:	WHEN,
				left:	$3,
			}
	  	yylex.(*yyLexState).sql_query_row = nil
	  	yylex.(*yyLexState).sql_exec = nil
	  }
	|
	  SQL  DATABASE  NAME
	  {
		l := yylex.(*yyLexState)
	  	l.sql_database = &sql_database{
				name:	$3,
			  }
	  }  '{'  sqldb_stmt_list  '}'
	  {
		l := yylex.(*yyLexState)
		if l.sql_database.driver_name == "" {
			l.error("sql database: %s: driver_name is required",
							$3)
			return 0
		}
	  	$$ = &ast{
			yy_tok:		SQL_DATABASE,
			sql_database:	l.sql_database,
		}
		l.config.sql_database[$3] = l.sql_database
		l.sql_database = nil
	  }
	|
	  SQL  QUERY  NAME  ROW
	  {
		l := yylex.(*yyLexState)
		l.sql_query_row = &sql_query_row {
				name:	    $3,
				result_row: make([]sql_query_result_row, 0),
				name2result: make(
					map[string]*sql_query_result_row),
		}
	  }  '{'  sql_decl_stmt_list  '}'
	  {
		l := yylex.(*yyLexState)

		q := l.sql_query_row
		if q.sql_database == nil {
			l.error("sql query row: %s: missing database", q.name)
			return 0
		}
		if len(q.result_row) == 0 {
			l.error("sql query row: %s: missing result row", q.name)
		}
		l.config.sql_query_row[$3] = l.sql_query_row
		l.sql_query_row = nil
	  }
	|
	  SQL  yy_EXEC  NAME
	  {
		l := yylex.(*yyLexState)
		l.sql_exec = &sql_exec {
				name:	    $3,
		}
	  }  '{'  sql_decl_stmt_list  '}'
	  {
		l := yylex.(*yyLexState)

		ex := l.sql_exec
		if ex.sql_database == nil {
			l.error("sql exec: %s: missing database", ex.name)
			return 0
		}

		l.config.sql_exec[$3] = l.sql_exec
		l.sql_exec = nil
	  }
	;

sql_result_type:

	  //  only bools for now
	  yy_BOOL
	  {
	  	$$ = reflect.Bool
	  }
	;

sql_result:
	  NAME  sql_result_type
	  {
		l := yylex.(*yyLexState)
		q := l.sql_query_row

		if q.name2result[$1] != nil {
			l.error("sql query row: %s: attribute redefined: %s",
								q.name, $1)
			return 0
		}

		if len(q.result_row) == 16 {
			l.error("sql query row: %s: too many attributes: > 16",
								q.name)
			return 0
		}
		rr := &sql_query_result_row{
				name: $1,
				go_kind: $2,
				offset: uint8(len(q.result_row)),
		}
		q.result_row = append(q.result_row, *rr)
		q.name2result[$1] = rr
	  }
	;

sql_result_list:
	  sql_result
	|
	  sql_result_list  ','  sql_result
	;

statement_list:
	  statement
	  {
	  	yylex.(*yyLexState).ast_root = $1
	  }
	|
	  statement_list statement
	  {
	  	s := $1
		for ;  s.next != nil;  s = s.next {}	//  find last stmt

		s.next = $2
	  }
	;
%%

var keyword = map[string]int{
	"and":			yy_AND,
	"argv":			ARGV,
	"blob_size":		BLOB_SIZE,
	"bool":			yy_BOOL,
	"boot":			BOOT,
	"brr_capacity":		BRR_CAPACITY,
	"call":			CALL,
	"chat_history":		CHAT_HISTORY,
	"command":		COMMAND,
	"database":		DATABASE,
	"data_source_name":	DATA_SOURCE_NAME,
	"driver_name":		DRIVER_NAME,
	"exec":			yy_EXEC,
	"exit_status":		EXIT_STATUS,
	"false":		yy_FALSE,
	"fdr_roll_duration":	FDR_ROLL_DURATION,
	"flow_worker_count":	FLOW_WORKER_COUNT,
	"heartbeat_duration":	HEARTBEAT_DURATION,
	"in":			IN,
	"int64":		yy_INT64,
	"is":			IS,
	"log_directory":	LOG_DIRECTORY,
	"max_idle_conns":	MAX_IDLE_CONNS,
	"max_open_conns":	MAX_OPEN_CONNS,
	"memstats_duration":	MEMSTAT_DURATION,
	"netflow":		NETFLOW,
	"OK":			yy_OK,
	"or":			yy_OR,
	"os_exec_capacity":	OS_EXEC_CAPACITY,
	"os_exec_worker_count":	OS_EXEC_WORKER_COUNT,
	"path":			PATH,
	"qdr_roll_duration":	QDR_ROLL_DURATION,
	"query_duration":	QUERY_DURATION,
	"query":		QUERY,
	"result":		RESULT,
	"row":			ROW,
	"rows_affected":	ROWS_AFFECTED,
	"sql":			SQL,
	"sqlstate":		SQLSTATE,
	"start_time":		START_TIME,
	"statement":		STATEMENT,
	"tail":			TAIL,
	"true":			yy_TRUE,
	"udig":			UDIG,
	"verb":			VERB,
	"wall_duration":	WALL_DURATION,
	"when":			WHEN,
	"xdr_roll_duration":	XDR_ROLL_DURATION,
}

type yyLexState struct {
	config				*config

	//  command being actively parsed, nil when not in 'command {...}'
	*command

	//  call being actively parsed, nil when not in 'call (...) when ...'
	*call

	*sql_database
	*sql_query_row
	*sql_exec

	in				io.RuneReader	//  config source stream
	line_no				uint64	   //  lexical line number
	eof				bool       //  seen eof in token stream
	peek				rune       //  lookahead in lexer
	err				error

	//  various parsing boot states.

	in_boot				bool
	seen_boot			bool

	seen_brr_capacity		bool
	seen_data_source_name		bool
	seen_driver_name		bool
	seen_fdr_roll_duration		bool
	seen_flow_worker_count		bool
	seen_heartbeat_duration		bool
	seen_max_idle_conns		bool
	seen_max_open_conns		bool
	seen_memstats_duration		bool
	seen_os_exec_capacity		bool
	seen_os_exec_worker_count	bool
	seen_qdr_roll_duration		bool
	seen_xdr_roll_duration		bool

	ast_root			*ast

	depends				[]string
	call2ast			map[string]*ast
	query2ast			map[string]*ast
}

func (l *yyLexState) pushback(c rune) {

	if l.peek != 0 {
		panic("pushback(): push before peek")	/* impossible */
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
	if l.peek != 0 {		/* returned stashed char */
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
	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get() {
		if c == '\n' {
			return
		}
	}
	return err

}

func skip_space(l *yyLexState) (c rune, eof bool, err error) {

	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get() {
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
	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get() {
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
		l.pushback(c)		/* first character after word */
	}

	if keyword[w] > 0 {		/* got a keyword */
		return keyword[w], nil	/* return yacc generated token */
	}

	if utf8.RuneCountInString(w) > max_name_rune_count {
		return PARSE_ERROR, l.mkerror(
				"name too long: expected %d chars, got %d",
				max_name_rune_count, utf8.RuneCountInString(w),
		);
	}

	//  command reference?
	if c := l.config.command[w];  c != nil {
		yylval.command = c
		return COMMAND_REF, nil
	}

	//  tail file reference?
	if l.config.tail != nil && w == l.config.tail.name {
		yylval.tail = l.config.tail
		return TAIL_REF, nil
	}

	//  sql query reference?
	if q := l.config.sql_query_row[w];  q != nil {
		yylval.sql_query_row = q
		return SQL_QUERY_ROW_REF, nil
	}

	//  sql exec reference?
	if ex := l.config.sql_exec[w];  ex != nil {
		yylval.sql_exec = ex
		return SQL_EXEC_REF, nil
	}

	//  sql database reference?

	if db := l.config.sql_database[w];  db != nil {
		yylval.sql_database = db
		return SQL_DATABASE_REF, nil
	}

	if l.in_boot {
		return PARSE_ERROR,l.mkerror("unknown variable in boot{}: '%s'",
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
	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get() {
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
		l.pushback(c)		//  first character after ui64
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

	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get(){
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
	for c, eof, err = l.get();  !eof && err == nil;  c, eof, err = l.get() {
		
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
	if (c > 127) {
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
		lno := l.line_no	// reset line number on error

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
		lno := l.line_no	// reset line number on error

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
		tok, err = lookahead(l, '=', EQ, int('='))
		if err != nil {
			goto PARSE_ERROR
		}
		return tok
	}
	if c == '!' {
		tok, err = lookahead(l, '=', NEQ, int('!'))
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

func (l *yyLexState) mkerror(format string, args...interface{}) error {

	return errors.New(Sprintf("%s, near line %d",
		Sprintf(format, args...),
		l.line_no,
	))
}

func (l *yyLexState) error(format string, args...interface{}) {

	l.Error(Sprintf(format, args...))
}

func (l *yyLexState) Error(msg string) {

	if l.err == nil {			//  only report first error
		l.err = l.mkerror("%s", msg)
	}
}

//  topologically sort the dependency graph or panic.
//
//  Note:
//	do we need the tsort()?  is simply requiring a call()/query()
//	to be defined before a reference to it's value sufficent
//	to insure no cycles?

func (l *yyLexState) tsort() (depend_order []string) {

	tmp, err := ioutil.TempFile("", "flowd-tsort")
	if err != nil {
		panic(err)
	}
	defer func () {
		tmp.Close()
		syscall.Unlink(tmp.Name())
	}();

	out := bufio.NewWriter(tmp)

	/*
	 *  Write the vertices of the dependency graph built by the attribute
	 *  references in either the argv or the 'when' clause.
	 */
	for _, v := range l.depends {
		Fprintf(out, "%s\n", v)
	}
	/*
	 *  Note:
	 *	I (jmscott) can't find the stdio equivalent of a close on
	 *	for buffered output in package bufio.  Seems odd to have
	 *	to actually flush before closing.
	 */
	err = out.Flush()
	if err != nil {
		panic(err)
	}
	tmp.Close()

	//  Call either /usr/bin/tsort or path pointed to by $BLOBIO_TSORT.

	tsort := os.Getenv("BLOBIO_TSORT")
	if tsort == "" {
		tsort = "/usr/bin/tsort"
	}
	argv := make([]string, 2)
	argv[0] = tsort
	argv[1] = tmp.Name()
	gcmd := &exec.Cmd{
			Path:	tsort,
			Args:	argv[:],
	}

	//  run gnu tsort command, check after ps is fetched, since any
	//  non-zero exit returns err.  see comments in os_exec.go.

	output, _ := gcmd.CombinedOutput()

	ps := gcmd.ProcessState
	if ps == nil {
		panic("os/exec.CombinedOutput(): nil ProcessState")
	}
	sys := ps.Sys()
	if sys.(syscall.WaitStatus).Signaled() {
		panic("os/exec.CombinedOutput(): terminated with a signal")
	}

	//  non-zero error indicates failure to sort graph topologically.
	//  man pages for gnu tsort say nothing about meaning of non-zero
	//  exit status, so we can't do much smart error handling.

	ex := uint8((uint16(sys.(syscall.WaitStatus))) >> 8)
	if ex != 0 {
		//  Note:
		//	tsort writes the cycle, so why not capture?
		panic(errors.New("dependency graph has cycles"))
	}

	//  build the dependency order for the calls/queries
	conf := l.config
	depend_order = make([]string, 0)
	in := bufio.NewReader(strings.NewReader(string(output)))
	for i := 0;;  i++ {
		var name string

		name, err = in.ReadString(byte('\n'))
		if err == io.EOF {
			err = nil
			break
		}
		if err != nil {
			return
		}

		name = strings.TrimSpace(name)

		//  cheap sanity test for reasonable output from tsort
		switch {

		case conf.command[name] != nil:
		case conf.sql_query_row[name] != nil:
		case conf.sql_exec[name] != nil:
		case conf.tail.name == name:

		default:
			panic(errors.New(
				Sprintf("unknown dependency type: %s", name)))
		}
		depend_order = append(depend_order, name)
	}

	return
}

func (conf *config) parse(in io.RuneReader) (
				par *parse,
				err error,
) {
	l := &yyLexState {
		config:		conf,
		line_no:	1,
		in:		in,
		eof:		false,
		err:		nil,
		depends:	make([]string, 0),
		call2ast:	make(map[string]*ast),
		query2ast:	make(map[string]*ast),
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

	depend_order := l.tsort()

	//  reverse depend order so roots of the graph are first
	for i, j := 0, len(depend_order) - 1;  i < j;  i, j = i + 1, j - 1 {
		depend_order[i], depend_order[j] =
			depend_order[j], depend_order[i]
	}
	return &parse{
		config:		conf,
		ast_root:	l.ast_root,
		depend_order:	depend_order,
		call2ast:	l.call2ast,
		query2ast:	l.query2ast,
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
		if (a.yy_tok > __MIN_YYTOK) {
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
		for i := 0;  i < indent;  i++ {
			Print("  ")
		}
	}
	Println(a.to_string(true))

	//  print kids
	a.left.walk_print(indent + 1, true)
	a.right.walk_print(indent + 1, true)

	//  print siblings
	if is_first_sibling {
		for as := a.next;  as != nil;  as = as.next {
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
