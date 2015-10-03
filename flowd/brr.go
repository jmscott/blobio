/*
Blob request records (brr) describe a single get/put request for a blob.

The format of a single request record is a new-line terminated list of
tab separated fields, i.e, the typical unixy ascii record:

	start_time		#  YYYY-MM-DDThh:mm:ss.ns[+-]HH:MM
	netflow			#  [a-z][a-z0-9]{0,7}~[[:graph:]]{1,128}
	verb			#  get/put/take/give/eat/wrap/roll
	algorithm:digest	#  udig of the blob in request
	chat_history		#  ok/no handshake between server&client
	blob_size		#  unsigned 64 bit long < 2^63
	wall_duration		#  request wall duration in sec.ns>=0
*/

package main

import (
	"errors"
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode"

	. "fmt"
)

type brr_field uint8

//  field offset for blob request records (brr)
const (
	brr_START_TIME = iota
	brr_NETFLOW
	brr_VERB
	brr_UDIG
	brr_CHAT_HISTORY
	brr_BLOB_SIZE
	brr_WALL_DURATION
)

func (bf brr_field) String() string {

	switch bf {
	case brr_START_TIME:
		return "start_time"
	case brr_NETFLOW:
		return "netflow"
	case brr_VERB:
		return "verb"
	case brr_UDIG:
		return "udig"
	case brr_CHAT_HISTORY:
		return "chat_history"
	case brr_BLOB_SIZE:
		return "blob_size"
	case brr_WALL_DURATION:
		return "wall_duration"
	default:
		panic("impossible brr field value")
	}
}

type brr [7]string

var netflow_re *regexp.Regexp

func init() {
	var err error

	// regular expressions used by brr for the network flow connections

	netflow_re, err = regexp.Compile(
		"^[a-z][a-z0-9]{0,7}~[[:graph:]]{1,128}$",
	)

	if err != nil {
		panic(err)
	}
}

//  Parse a string into a brr.
//
//  Note:
//	Need to replace Split() with inline parse directly into the brr[]
//
func (*brr) parse(s string) (*brr, error) {
	var err error

	b := strings.Split(s, "\t")

	if len(b) != 7 {
		return nil, errors.New(
			"wrong number of brr fields: expected 7, got " +
				strconv.Itoa(len(b)))
	}

	// start time of the request
	_, err = time.Parse(time.RFC3339Nano, b[0])
	if err != nil {
		return nil, errors.New("unparsable start time: " + err.Error())
	}

	if !netflow_re.MatchString(b[1]) {
		return nil, errors.New("unrecognized network flow: " + b[1])
	}

	/*
	 *  Verb
	 */
	v := b[2]
	if v != "get" && v != "put" && v != "give" && v != "take" &&
		v != "eat" && v != "wrap" && v != "roll" {
		return nil, errors.New("unknown verb: " + v)
	}

	/*
	 *  Udig
	 */
	udig := b[3]
	if len(udig) == 0 {
		return nil, errors.New("udig: 0 length")
	}
	if len(udig) > 15+1+128 {
		return nil, errors.New("udig: too many characters")
	}

	/*
	 *  Udig Algorithm
	 */
	co := strings.IndexRune(udig, ':')
	if co == -1 {
		return nil, errors.New("udig: no colon in algorithm")
	}
	if co == 0 {
		return nil, errors.New("udig: colon is first character")
	}
	if co > 8 {
		return nil, errors.New("udig: colon > 8th character")
	}
	for i := 0; i < co; i++ { // parse the algorithm
		c := rune(udig[i])

		if c > unicode.MaxASCII {
			return nil, errors.New("udig: algorithm: non ASCII")
		}
		if 'A' <= c && c <= 'Z' {
			return nil, errors.New(
				"udig: upper case not allowed in algorithm: " +
					string(c))
		}
		if i == 0 {
			if unicode.IsDigit(c) {
				return nil, errors.New(
					"udig: algorithm: first char is digit")
			}
		} else if !unicode.IsLetter(c) && !unicode.IsDigit(c) {
			return nil, errors.New(
				"udig: algorithm: char not lower|digit: " +
					string(c))
		}
	}

	/*
	 *  Udig Digest
	 */
	if co+1 > len(udig) {
		return nil, errors.New("udig: no digest after colon")
	}
	for i := co + 1; i < len(udig); i++ {
		c := rune(udig[i])
		if c > unicode.MaxASCII {
			return nil, errors.New("udig: digest: non ASCII")
		}
		if !unicode.IsGraphic(c) {
			return nil, errors.New("udig: digest: non graphic")
		}
	}

	/*
	 *  Chat History
	 *
	 *  Note:
	 *	Must check that chat history is appropriate for the
	 *	verb.  For example, when verb=="get" then chat_history can
	 *	only equal "ok" or "no".
	 */
	ch := b[4]
	switch {
	case ch == "ok" || ch == "no" || ch == "ok,ok" || ch == "ok,ok,ok" ||
		ch == "ok,no" || ch == "ok,ok,no":
	default:
		return nil, errors.New("chat history: unrecognized")
	}

	/*
	 *  Blob Size
	 */
	_, err = strconv.ParseUint(b[5], 10, 64)
	if err != nil {
		return nil, errors.New("blob size: " + err.Error())
	}

	/*
	 *  Wall Duration
	 *
	 *  Elapsed seconds.  The blobio spec says the fraction part
	 *  of the wall duration can't be more than 9 decimal places.
	 */
	_, err = time.ParseDuration(Sprintf("%ss", b[6]))
	if err != nil {
		return nil, errors.New("unparsable wall duration: " +
			err.Error())
	}

	ba := new(brr)
	copy(ba[:], b)
	return ba, err
}
