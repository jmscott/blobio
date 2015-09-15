//  Synopsis:
//	Compile a flow configuration into a network of channels.
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
package main

import (
	. "fmt"
)

type compile struct {
	*parse
	*flow

	os_exec_chan
	fdr_log_chan  file_byte_chan
	xdr_log_chan  file_byte_chan
	qdr_log_chan  file_byte_chan
	info_log_chan file_byte_chan
}

func (cmpl *compile) compile() fdr_chan {

	type call_output struct {
		out_chans []xdr_chan
		next_chan int
	}
	type query_output struct {
		out_chans []qdr_chan
		next_chan int
	}

	par := cmpl.parse
	conf := par.config
	flow := cmpl.flow

	//  map command xdr output onto list of broadcast channels
	command2xdr := make(map[string]*call_output)

	//  map query qdr output onto list of broadcast channels
	query2qdr := make(map[string]*query_output)

	//  map abstract syntax tree nodes to compiled channels
	a2b := make(map[*ast]bool_chan)
	a2u := make(map[*ast]uint64_chan)
	a2s := make(map[*ast]string_chan)
	a2a := make(map[*ast]argv_chan)
	a2x := make(map[*ast]xdr_chan)
	a2q := make(map[*ast]qdr_chan)

	var compile func(a *ast)

	compile = func(a *ast) {
		if a == nil {
			return
		}

		compile(a.left)
		compile(a.right)

		switch a.yy_tok {
		case UINT64:
			a2u[a] = flow.const_uint64(a.uint64)
		case CAST_UINT64:
			a2u[a] = flow.cast_uint64(a2s[a.left])
		case STRING:
			a2s[a] = flow.const_string(a.string)
		case CAST_STRING:
			a2s[a] = flow.cast_string(a2u[a.left])
		case ARGV0:
			a2a[a] = flow.argv0()
		case ARGV1:
			a2a[a] = flow.argv1(a2s[a.left])
		case ARGV:
			//  argv() needs a string_chan slice
			in := make([]string_chan, a.uint64)

			//  first arg node already compiled
			aa := a.left
			in[0] = a2s[aa]
			aa = aa.next

			//  compile arg nodes 2 ... n
			for i := 1; aa != nil; aa = aa.next {
				compile(aa)
				in[i] = a2s[aa]
				i++
			}
			a2a[a] = flow.argv(in)

		case CALL0:
			cmd := a.call.command

			//  any command with path = "true" is optimized
			//  to call the builtin that always returns 0,
			//  like the borne shell.  the call() must
			//  have no arguments

			a2x[a] = flow.call0(cmd.name, a2a[a.left], a2b[a.right])

			//  broadcast to all dependent rules plus log_xdr
			command2xdr[cmd.name] =
				&call_output{
					out_chans: flow.fanout_xdr(
						a2x[a],
						cmd.depend_ref_count+1,
					),
					next_chan: 1,
				}

		//  call an os executable
		case CALL:
			cmd := a.call.command
			a2x[a] = flow.call(
				cmd,
				cmpl.os_exec_chan,
				a2a[a.left],
				a2b[a.right],
			)

			//  broadcast to all dependent rules plus log_xdr
			command2xdr[cmd.name] =
				&call_output{
					out_chans: flow.fanout_xdr(
						a2x[a],
						cmd.depend_ref_count+1,
					),
					next_chan: 1,
				}
		case QUERY_ROW:
			q := a.sql_query_row
			a2q[a] = flow.sql_query_row(
				q,
				a2a[a.left],
				a2b[a.right],
			)

			//  broadcast to all dependent rules plus log_qdr
			query2qdr[q.name] = &query_output{
				out_chans: flow.fanout_qdr(
					a2q[a],
					q.depend_ref_count+1,
				),
				next_chan: 1,
			}
		case QUERY_EXEC:
			ex := a.sql_exec
			a2q[a] = flow.sql_exec(
				ex,
				a2a[a.left],
				a2b[a.right],
			)

			//  broadcast to all dependent rules plus log_qdr
			query2qdr[ex.name] = &query_output{
				out_chans: flow.fanout_qdr(
					a2q[a],
					ex.depend_ref_count+1,
				),
				next_chan: 1,
			}
		case QUERY_EXEC_TXN:
			ex := a.sql_exec
			a2q[a] = flow.sql_exec_txn(
				ex,
				a2a[a.left],
				a2b[a.right],
			)

			//  broadcast to all dependent rules plus log_qdr
			query2qdr[ex.name] = &query_output{
				out_chans: flow.fanout_qdr(
					a2q[a],
					ex.depend_ref_count+1,
				),
				next_chan: 1,
			}
		case yy_TRUE:
			a2b[a] = flow.const_bool(true)
		case yy_FALSE:
			a2b[a] = flow.const_bool(false)
		case WHEN:
			a2b[a] = a2b[a.left]
		case PROJECT_XDR_EXIT_STATUS:
			cx := command2xdr[a.string]

			//  Note: zap this test and add a sanity test at the
			//	  end of the compile that verifys that
			//	  cx.next_chan == len(cx.out_chans)

			if cx == nil {
				panic("missing command -> xdr map for " +
					a.command.name)
			}
			a2u[a] = flow.project_xdr_exit_status(
				cx.out_chans[cx.next_chan],
			)
			cx.next_chan++
		case PROJECT_SQL_QUERY_ROW_BOOL:
			qq := query2qdr[a.string]

			//  Note: zap this test and add a sanity test at the
			//	  end of the compile that verifys that
			//	  cx.next_chan == len(cx.out_chans)

			if qq == nil {
				panic("row_bool: query2qdr map is nil")
			}
			a2b[a] = flow.project_sql_query_row_bool(
				qq.out_chans[qq.next_chan],
				a.uint8,
			)
			qq.next_chan++

		case PROJECT_QDR_ROWS_AFFECTED:
			qq := query2qdr[a.string]

			//  Note: zap this test and add a sanity test at the
			//	  end of the compile that verifys that
			//	  cx.next_chan == len(cx.out_chans)

			if qq == nil {
				panic("rows_affected: query2qdr map is nil")
			}
			a2u[a] = flow.project_qdr_rows_affected(
				qq.out_chans[qq.next_chan],
			)
			qq.next_chan++
		case PROJECT_QDR_SQLSTATE:
			qq := query2qdr[a.string]

			//  Note: zap this test and add a sanity test at the
			//	  end of the compile that verifys that
			//	  cx.next_chan == len(cx.out_chans)

			if qq == nil {
				panic("sqlstate: query2qdr map is nil")
			}
			a2s[a] = flow.project_qdr_sqlstate(
				qq.out_chans[qq.next_chan],
			)
			qq.next_chan++

		case PROJECT_BRR:
			a2s[a] = flow.project_brr(a.brr_field)
		case EQ_UINT64:
			a2b[a] = flow.eq_uint64(a.right.uint64, a2u[a.left])
		case NEQ_UINT64:
			a2b[a] = flow.neq_uint64(a.right.uint64, a2u[a.left])
		case EQ_STRING:
			a2b[a] = flow.eq_string(a.right.string, a2s[a.left])
		case NEQ_STRING:
			a2b[a] = flow.neq_string(a.right.string, a2s[a.left])
		case EQ_BOOL:
			a2b[a] = flow.eq_bool(a.right.bool, a2b[a.left])
		case yy_OR:
			a2b[a] = flow.bool2(or, a2b[a.left], a2b[a.right])
		case yy_AND:
			a2b[a] = flow.bool2(and, a2b[a.left], a2b[a.right])
		default:
			panic(Sprintf("impossible yy_tok in ast: %d", a.yy_tok))
		}
	}

	//  compile nodes from least dependent to most dependent order
	for _, n := range par.depend_order {

		//  skip tail dependency
		if n == conf.tail.name {
			continue
		}

		var root *ast
		if root = par.call2ast[n]; root == nil {
			root = par.query2ast[n]
		}
		if root == nil {
			panic(Sprintf("map to abstract syntax tree: %s", n))
		}
		compile(root)
	}

	//  Wait for all xdr to flow in before reducing the whole set
	//  into a single fdr record

	xdr_out := make([]xdr_chan, len(command2xdr))
	i := 0
	for n, cx := range command2xdr {

		//  cheap sanity test that all output channels have consumers
		if cx.next_chan != len(cx.out_chans) {
			panic(Sprintf(
				"%s: expected %d consumed chans, got %d",
				n,
				len(cx.out_chans),
				cx.next_chan,
			))
		}

		//  wait for the xdr log entry to be written.
		//
		//  Note:
		//	why make log_xdr_error() wait on log_xdr()?

		xdr_out[i] = flow.log_xdr_error(
			cmpl.info_log_chan,
			flow.log_xdr(
				cmpl.xdr_log_chan,
				cx.out_chans[0],
			))
		i++
	}
	//  Wait for all qdr to flow in before reducing the whole set
	//  into a single fdr record

	qdr_out := make([]qdr_chan, len(query2qdr))
	i = 0
	for n, qq := range query2qdr {

		//  cheap sanity test that all output channels have consumers
		if qq.next_chan != len(qq.out_chans) {
			panic(Sprintf(
				"%s: expected %d consumed chans, got %d",
				n,
				len(qq.out_chans),
				qq.next_chan,
			))
		}

		//  wait for the qdr log entry to be written.
		//
		//  Note:
		//	why make log_qdr_error() wait on log_qdr()?

		qdr_out[i] = flow.log_qdr_error(
			cmpl.info_log_chan,
			flow.log_qdr(
				cmpl.qdr_log_chan,
				qq.out_chans[0],
			))
		i++
	}
	return flow.log_fdr(cmpl.fdr_log_chan, flow.reduce(xdr_out, qdr_out))
}
