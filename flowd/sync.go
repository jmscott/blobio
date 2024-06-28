package main

import (
	"sync"
)

const (
	unknown_type = iota

	sync_map_go_string
	sync_map_go_bool

)

type sync_map struct {
	name		string
	mapx		sync.Map

	//  Note: ought to be stored in parser, not run time
	referenced	bool
	line_no		uint64

	//  eventually will support dynamic types 
	go_domain	int
	go_range	int

	loaded_count	int64
	store_count	int64
}
