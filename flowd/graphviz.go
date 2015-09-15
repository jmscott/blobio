//  Synopsis:
//	Action to translate a flow file into graphviz dot layout.
//  See:
//	http://www.graphviz.org/
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
package main

import (
	"os"
	"strings"

	. "fmt"
)

type graphviz struct {
	call_root        map[string]*call
	statement_prefix string
	arc_history      map[string]bool
}

var graphviz_action = &graphviz{}

func (gv *graphviz) exit(status int) {

	os.Exit(status)
}

func (gv *graphviz) WARN(format string, args ...interface{}) {

	Fprintf(stderr, "WARN: %s", Sprintf(format, args...))
}

func (gv *graphviz) die(format string, args ...interface{}) {

	Fprintf(stderr, "ERROR: %s", Sprintf(format, args...))
	gv.exit(255)
}

func (gv *graphviz) put_prefix(depth uint64) {

	for i := uint64(0); i < depth; i++ {
		Printf(gv.statement_prefix)
	}
}

func (gv *graphviz) put_branch(c *call, depth uint64) {

	n := c.command.name

	//  write the call_send branches
	for nc, child := range c.command.call_send {
		if gv.call_root[n] != nil {
			gv.put_prefix(depth)
			Printf("%s [shape=box]\n", n)
		}
		gv.put_prefix(depth)

		//  only write the edge once
		a := n + "\t" + nc
		if !gv.arc_history[a] {
			Printf("%s -> %s\n", n, nc)
			gv.arc_history[a] = true
		}
		gv.put_branch(child, depth+1)
	}
}

func (gv *graphviz) start(conf *config) {

	argc := len(os.Args)

	//  parse the --start-root option
	var start_root map[string]bool
	for i := 3; i < argc; i++ {
		a := os.Args[i]

		switch {
		case a == "--start-root":
			if start_root == nil {
				start_root = make(map[string]bool)
			}
			i++
			if i == len(os.Args) {
				gv.die("option --start-root: missing call()")
			}
			start_root[os.Args[i]] = true
		case strings.HasPrefix(a, "--"):
			gv.die("unknown option: %s", a)
		default:
			gv.die("unknown command argument: %s", a)
		}
	}

	//  verify the requested --start-root is a command
	for n, _ := range start_root {
		if conf.call[conf.command[n]] == nil {
			gv.die(
				"option --start-root: call does not exist: %s", n)
		}
	}
	gv.call_root = conf.find_call_roots(start_root)

	//  verify the requested --start-root is a true root
	//  eventually may allow other nodes as starting points.
	//  this option would be called --start-node
	for n, _ := range start_root {
		if gv.call_root[n] == nil {
			gv.die("option --start-root: call is not a root: %s", n)
		}
	}
	gv.statement_prefix = "\t"
	gv.arc_history = make(map[string]bool)

	Printf("digraph \"%s\" {\n", conf.path)

	for _, c := range gv.call_root {
		gv.put_branch(c, 1)
	}
	Printf("}\n")
	gv.exit(0)
}
