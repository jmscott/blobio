/*
 *  Synopsis:
 *	PostgreSQL schema tracks blobs available in a particular bio4d server.
 *  Note:
 *	Why does column brr_no_recent.wall_duration exist?
 */
\set ON_ERROR_STOP on

SET search_path to blobio,public;

BEGIN;
DROP SCHEMA IF EXISTS blobio CASCADE;
CREATE SCHEMA blobio;
COMMENT ON SCHEMA blobio IS
  'blobs in service and brr history'
;

DROP DOMAIN IF EXISTS brr_duration CASCADE;
CREATE DOMAIN brr_duration AS interval
  CHECK (
	value >= '0 seconds'
  )
  NOT NULL
;
COMMENT ON DOMAIN brr_duration IS
  'wall clock duration of a blob request'
;

DROP DOMAIN IF EXISTS brr_timestamp CASCADE;
CREATE DOMAIN brr_timestamp AS timestamptz
  CHECK (
	value >= '2008-05-17 10:06:42'		--  birthday of blobio
  )
  NOT NULL
;
COMMENT ON DOMAIN brr_timestamp IS
  'starting time of request for a blob'
;

DROP DOMAIN IF EXISTS ui63 CASCADE;
CREATE DOMAIN ui63 AS bigint
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui63 IS
  'non null integer in range 0 <= 2^63 - 1'
;

DROP DOMAIN IF EXISTS ui32 CASCADE;
CREATE DOMAIN ui32 AS int
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui32 IS
  'non null integer in range 0 <= 2^31 - 1'
;

DROP DOMAIN IF EXISTS ui16 CASCADE;
CREATE DOMAIN ui16 AS smallint
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui16 IS
  'non null integer in range 0 <= 2^15 - 1'
;

DROP DOMAIN IF EXISTS ui8 CASCADE;
CREATE DOMAIN ui8 AS smallint
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui8 IS
  'non null integer in range 0 <= 2^8 - 1'
;

DROP DOMAIN IF EXISTS tag CASCADE;
CREATE DOMAIN tag AS text
  CHECK (
  	value ~ '^[a-z][a-z0-9_]{0,15}$'
  )
  NOT NULL
;
COMMENT ON DOMAIN tag IS
  'no null, up to 16 ascii tag matching [a-z][a-z0-9_]{0,15}'
;

/*
 *  Most recently seen successfull take FROM this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
DROP TABLE IF EXISTS brr_take_ok_recent CASCADE;
CREATE TABLE brr_take_ok_recent
(
	blob			udig
					PRIMARY KEY,
	start_time		brr_timestamp,

	/*
	 *  Since the physical remove of the blob only occurs after the client
	 *  has the blob, a better estimate of when the blob was removed FROM
	 *  service is start_time + wall_duration.
	 */
	wall_duration		brr_duration
);
CREATE INDEX brr_take_ok_recent_hash ON
	brr_take_ok_recent USING hash(blob)
;
CREATE INDEX brr_take_ok_recent_start_time ON
	brr_take_ok_recent(start_time)
;
COMMENT ON TABLE brr_take_ok_recent IS
  'most recently seen take request record for a particular blob'
;

/*
 *  The immutable blob size AS observed in a brr record of an existing blob.
 */
DROP TABLE IF EXISTS brr_blob_size CASCADE;
CREATE TABLE brr_blob_size
(
	blob		udig
				PRIMARY KEY,
	byte_count	ui63,
	CONSTRAINT size_check CHECK ((
		udig_is_empty(blob)
		AND
		byte_count = 0
	  )
	  OR
	  (
		NOT udig_is_empty(blob)
		AND
		byte_count > 0
	))
);
CREATE INDEX brr_blob_size_byte_count
	ON brr_blob_size(byte_count)
;
CREATE INDEX brr_blob_size_hash
	ON brr_blob_size USING hash(blob)
;
COMMENT ON TABLE brr_blob_size IS
  'number bytes (octets) in the blob as reported by blob request record'
;

/*
 *  A recently verified existence of a blob AS seen in a brr record.
 */
DROP TABLE IF EXISTS brr_ok_recent CASCADE;
CREATE TABLE brr_ok_recent
(
	blob		udig
				PRIMARY KEY,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
CREATE INDEX brr_ok_recent_hash
	ON brr_ok_recent USING hash(blob)
;
CREATE INDEX brr_ok_recent_start_time
	ON brr_ok_recent(start_time)
;
COMMENT ON TABLE brr_ok_recent IS
  'most recently verified existence for a particular blob'
;

/*
 *  A recently failed read of a blob, which implies the blob may not exist.
 *  Don't record successfull "take"s.
 */
DROP TABLE IF EXISTS brr_no_recent CASCADE;
CREATE TABLE brr_no_recent
(
	blob		udig
				PRIMARY KEY,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
CREATE INDEX brr_no_recent_hash
	ON brr_no_recent USING hash(blob)
;
CREATE INDEX brr_no_recent_start_time
	ON brr_no_recent(start_time)
;
COMMENT ON TABLE brr_no_recent IS
  'most recently failed attempt to get or eat the blob'
;

/*
 *  Earliest known existence of a blob.  Both the brr time
 *  and the insert/update of the record are recorded.
 */
DROP TABLE IF EXISTS brr_discover;
CREATE TABLE brr_discover
(
	blob		udig
				PRIMARY KEY,
	/*
	 *  Start time in blob request record.
	 */
	start_time	brr_timestamp
);
CREATE INDEX brr_discover_hash
	ON brr_discover USING hash(blob)
;
CREATE INDEX brr_discover_start_time
	ON brr_discover(start_time)
;
COMMENT ON TABLE brr_discover
  IS
  	'the earliest known existence of a digestable blob for this service'
;
COMMENT ON COLUMN brr_discover.start_time
  IS
  	'start time of the earliest request for the discovered blob'
;

DROP TABLE IF EXISTS brr_wrap_ok;
CREATE TABLE brr_wrap_ok
(
	blob		udig
				PRIMARY KEY,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
CREATE INDEX brr_wrap_ok_hash
	ON brr_wrap_ok USING hash(blob)
;
CREATE INDEX brr_wrap_ok_start_time
	ON brr_wrap_ok(start_time)
;
COMMENT ON TABLE brr_wrap_ok
  IS
  	'history of successfull wrap requests'
;

DROP TABLE IF EXISTS brr_roll_ok;
CREATE TABLE brr_roll_ok
(
	blob		udig
				PRIMARY KEY,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
CREATE INDEX brr_roll_ok_hash
	ON brr_roll_ok USING hash(blob)
;
CREATE INDEX brr_roll_ok_start_time
	ON brr_roll_ok(start_time)
;
COMMENT ON TABLE brr_roll_ok
  IS
  	'history of successfull roll requests'
;

/*
 *  Fetchable blobs with regard to the blobio network service.
 *
 *  Note:
 *	I have seen service tuples when more recent entries
 *	existed in brr_no_recent.
 */
DROP VIEW IF EXISTS service CASCADE;
CREATE VIEW service AS
  SELECT
	ok.blob,
	ok.start_time AS "recent_verify_time",
	d.start_time AS "discover_time"
    FROM
  	brr_ok_recent ok
	  LEFT OUTER JOIN brr_take_ok_recent take ON
	  (
	  	take.blob = ok.blob
		AND
		take.start_time >= ok.start_time
	  )
	  LEFT OUTER JOIN brr_no_recent no ON
	  (
	  	no.blob = ok.blob
		AND
		no.start_time >= ok.start_time
	  )
	  INNER JOIN brr_discover d ON (d.blob = ok.blob)
    WHERE
	take.blob IS null
	AND
	no.blob IS null
;
COMMENT ON VIEW service
  IS
  	'all verified blobs managed by the server'
;

/*
 *  Blobs taken FROM the service and, therefore, not in service
 */
DROP VIEW IF EXISTS taken CASCADE;
CREATE VIEW taken AS
  SELECT
	take.blob,
	take.start_time AS "recent_verify_time"
    FROM
  	brr_take_ok_recent take
	  LEFT OUTER JOIN brr_ok_recent ok ON
	  (
	  	ok.blob = take.blob
		AND
		ok.start_time >= take.start_time
	  )
    WHERE
	ok.blob IS null
;
COMMENT ON VIEW taken
  IS
  	'blobs taken FROM the service and, therefore, not in service'
;

/*
 *  View of once discovered blobs that are neither in service nor taken
 *  By design, the 'missing' set should always be empty.
 */
DROP VIEW IF EXISTS missing CASCADE;
CREATE VIEW missing AS
  SELECT
  	dis.blob,
	dis.start_time AS "discover_time"
    FROM
    	brr_discover dis
	  LEFT OUTER JOIN service srv ON (srv.blob = dis.blob)
	  LEFT OUTER JOIN taken take ON (take.blob = dis.blob)
    WHERE
    	srv.blob IS null
	AND
	take.blob IS null
;
COMMENT ON VIEW missing
  IS
  	'once discovered blobs that are neither in service nor taken'
;
/*
 *  View of udigs for which no associated blob has been digested.
 */
DROP VIEW IF EXISTS quack CASCADE;
CREATE VIEW quack AS
  SELECT
  	no.blob,
	no.start_time AS "discover_time"
    FROM
    	brr_no_recent no
	  LEFT OUTER JOIN service srv ON (srv.blob = no.blob)
	  LEFT OUTER JOIN taken take ON (take.blob = no.blob)
	  LEFT OUTER JOIN missing mis ON (mis.blob = no.blob)
    WHERE
    	srv.blob IS null
	AND
	take.blob IS null
	AND
	mis.blob IS null
;
COMMENT ON VIEW quack
  IS
  	'blobs which have never been digested'
;

DROP TABLE IF EXISTS bio4d_stat CASCADE;
CREATE TABLE bio4d_stat
(
	sample_time	brr_timestamp
				PRIMARY KEY,

	success_count	ui63,
	error_count	ui63,
	timeout_count	ui63,
	signal_count	ui63,
	fault_count	ui63,

	get_count	ui63,
	put_count	ui63,
	give_count	ui63,
	take_count	ui63,
	eat_count	ui63,
	wrap_count	ui63,
	roll_count	ui63,

	chat_ok_count	ui63,
	chat_no_count	ui63,
	chat_no2_count	ui63,
	chat_no3_count	ui63,
	eat_no_count	ui63,
	take_no_count	ui63
);

DROP TABLE IF EXISTS wrap2stat_json CASCADE;
CREATE TABLE wrap2stat_json
(
	blob			udig
					PRIMARY KEY,
	run_time		brr_timestamp,

	wrap_blob		udig NOT NULL,
	previous_wrap_udig	udig,

	distinct_udig_count	ui63,
					
	min_start_time		brr_timestamp,
	max_start_time		brr_timestamp,	
	max_wall_duration	brr_duration,
	max_blob_size		ui63,

	no_count		ui63,
	ok_count		ui63

);
COMMENT ON TABLE wrap2stat_json IS
  'Summarize json output for command sbin/wrap2stat_json'
;

COMMIT;
