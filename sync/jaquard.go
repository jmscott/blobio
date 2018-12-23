/*
 *  Synopsis:
 *	Calulate jaqaurd pairwise metric across a set of blobio service.
 *  Uaage:
 *	jaquard <config-file>
 */

package main

import (
	"encoding/json"
	"fmt"
	"os"
)

type PGDatabase struct {
	tag		string
	PGHOST		string
	PGPORT		uint16
	PGUSER		string
	PGPASSWORD	string
	PGDATABASE	string
}

type Config struct {
	Databases	map[string]PGDatabase `json:"databases"`
}

const usage = "usage: jaquard <config-file-path>\n"

func die(format string, args ...interface{}) {

	fmt.Fprintf(os.Stderr, "ERROR: " + format + "\n", args...);
	os.Stderr.Write([]byte(usage))
	os.Exit(1)
}

func (pg *PGDatabase) frisk() {

	empty := func(field string) {
		die("frisk: database: " + pg.tag + ": empty or undefined: " + field)
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
}

func (conf *Config) frisk() {

	for tag, pg := range conf.Databases {
		pg.tag = tag
		pg.frisk()
	}
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

	conf.frisk()
	fmt.Printf("conf=%#v\n", conf)
}
