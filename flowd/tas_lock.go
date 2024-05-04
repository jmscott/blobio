package main

import (
	"sync"
)

var the_tas_lock tas_lock

type tas_lock struct { 

	mux	sync.Mutex
	locked	map[string]bool
}

func (tas *tas_lock) test_and_set(val string) (set bool) {

	tas.mux.Lock()
	if _, ok := tas.locked[val];  !ok {
		tas.locked[val] = true
	}
	tas.mux.Unlock()
	return set
}

func (tas *tas_lock) unset(val string) {
	tas.mux.Lock()
	tas.locked[val] = false
	tas.mux.Unlock()
}
