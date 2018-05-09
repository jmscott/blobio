/*
 *  Synopsis:
 *	PostgreSQL schema to track blobs available in a particular biod server.
 *  Note:
 *	Please add sql comments.
 */
\set ON_ERROR_STOP on

BEGIN;
DROP SCHEMA IF EXISTS blobio CASCADE;
CREATE SCHEMA blobio;
COMMENT ON SCHEMA blobio IS
  'blobs in service and brr history'
;


DROP DOMAIN IF EXISTS blobio.brr_duration CASCADE;
CREATE DOMAIN blobio.brr_duration AS interval
  CHECK (
	value >= '0 seconds'
  )
  NOT NULL
;
COMMENT ON DOMAIN blobio.brr_duration IS
  'wall clock duration of a blob request'
;

DROP DOMAIN IF EXISTS blobio.brr_timestamp CASCADE;
CREATE DOMAIN blobio.brr_timestamp AS timestamptz
  CHECK (
	value >= '2008-05-17 10:06:42'		--  birthday of blobio
  )
  NOT NULL
;
COMMENT ON DOMAIN blobio.brr_timestamp IS
  'starting time of request for a blob'
;

DROP DOMAIN IF EXISTS blobio.ui63 CASCADE;
CREATE DOMAIN blobio.ui63 AS bigint
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN blobio.ui63 IS
  'integer in range 0 <= 2^63 - 1'
;

/*
 *  Most recently seen successfull take FROM this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
DROP TABLE IF EXISTS blobio.brr_take_ok3_recent CASCADE;
CREATE TABLE blobio.brr_take_ok3_recent
(
	blob			public.udig
					PRIMARY KEY,
	start_time		blobio.brr_timestamp,

	/*
	 *  Since the physical remove of the blob only occurs after the client
	 *  has the blob, a better estimate of when the blob was removed FROM
	 *  service is start_time + wall_duration.
	 */
	wall_duration		blobio.brr_duration
);
CREATE INDEX brr_take_ok3_recent_hash ON
	blobio.brr_take_ok3_recent USING hash(blob)
;
CREATE INDEX brr_take_ok3_recent_start_time ON
	blobio.brr_take_ok3_recent(start_time)
;
COMMENT ON TABLE blobio.brr_take_ok3_recent IS
  'most recently seen take request record for a particular blob'
;
REVOKE UPDATE ON blobio.brr_take_ok3_recent FROM public;

/*
 *  The immutable blob size AS observed in a brr record of an existing blob.
 */
DROP TABLE IF EXISTS blobio.brr_blob_size CASCADE;
CREATE TABLE blobio.brr_blob_size
(
	blob		public.udig
				PRIMARY KEY,
	byte_count	blobio.ui63
				NOT NULL,
	CONSTRAINT size_check CHECK ((
		udig_is_empty(blob) = true
		AND
		byte_count = 0
	  )
	  OR
	  (
		udig_is_empty(blob) = false
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
COMMENT ON TABLE blobio.brr_blob_size IS
  'number bytes (octets) in the blob as reported by blob request record'
;
REVOKE UPDATE ON blobio.brr_blob_size FROM public;

/*
 *  A recently verified existence of a blob AS seen in a brr record.
 */
DROP TABLE IF EXISTS blobio.brr_ok_recent CASCADE;
CREATE TABLE blobio.brr_ok_recent
(
	blob		public.udig
				PRIMARY KEY,
	start_time	blobio.brr_timestamp,
	wall_duration	blobio.brr_duration
);
CREATE INDEX brr_ok_recent_hash
	ON blobio.brr_ok_recent USING hash(blob)
;
CREATE INDEX brr_ok_recent_start_time
	ON blobio.brr_ok_recent(start_time)
;
COMMENT ON TABLE blobio.brr_ok_recent IS
  'most recently verified existence for a particular blob'
;
REVOKE UPDATE ON blobio.brr_ok_recent FROM public;

/*
 *  A recently failed read of a blob, which implies the blob may not exist.
 *  Don't record successfull "take"s.
 */
DROP TABLE IF EXISTS blobio.brr_no_recent CASCADE;
CREATE TABLE blobio.brr_no_recent
(
	blob		public.udig
				PRIMARY KEY,
	start_time	blobio.brr_timestamp,
	wall_duration	blobio.brr_duration
);
CREATE INDEX brr_no_recent_hash
	ON blobio.brr_no_recent USING hash(blob)
;
CREATE INDEX brr_no_recent_start_time
	ON blobio.brr_no_recent(start_time)
;
COMMENT ON TABLE blobio.brr_no_recent IS
  'most recently failed attempt to get or eat the blob'
;
REVOKE UPDATE ON blobio.brr_no_recent FROM public;

/*
 *  Earliest known existence of a blob.  Both the brr time
 *  and the insert/update of the record are recorded.
 */
DROP TABLE IF EXISTS blobio.brr_discover;
CREATE TABLE blobio.brr_discover
(
	blob		public.udig
				PRIMARY KEY,
	/*
	 *  Start time in blob request record.
	 */
	start_time	blobio.brr_timestamp,

	/*
	 *  Time this database record was inserted or updated with
	 *  an earlier time,  effective measuring discover latency.
	 */
	upsert_time	blobio.brr_timestamp
				DEFAULT now()
);
CREATE INDEX brr_discover_hash
	ON blobio.brr_discover USING hash(blob)
;
CREATE INDEX brr_discover_start_time
	ON blobio.brr_discover(start_time)
;
COMMENT ON TABLE blobio.brr_discover
  IS
  	'the earliest known existence of a digestable blob for this service'
;
COMMENT ON COLUMN blobio.brr_discover.start_time
  IS
  	'start time of the earliest request for the discovered blob'
;
COMMENT ON COLUMN blobio.brr_discover.upsert_time
  IS
  	'time of sql insert or update of this tuple'
;
REVOKE UPDATE ON blobio.brr_discover FROM public;

DROP TABLE IF EXISTS blobio.brr_wrap_ok;
CREATE TABLE blobio.brr_wrap_ok
(
	blob		public.udig
				PRIMARY KEY,
	start_time	blobio.brr_timestamp,
	wall_duration	blobio.brr_duration,
	insert_time	blobio.brr_timestamp
				DEFAULT now()
);
CREATE INDEX brr_wrap_ok_hash
	ON blobio.brr_wrap_ok USING hash(blob)
;
CREATE INDEX brr_wrap_ok_start_time
	ON blobio.brr_wrap_ok(start_time)
;
COMMENT ON TABLE blobio.brr_wrap_ok
  IS
  	'history of successfull wrap requests'
;
REVOKE UPDATE ON blobio.brr_wrap_ok FROM public;

DROP TABLE IF EXISTS blobio.brr_roll_ok;
CREATE TABLE blobio.brr_roll_ok
(
	blob		public.udig
				PRIMARY KEY,
	start_time	blobio.brr_timestamp,
	wall_duration	blobio.brr_duration,
	insert_time	blobio.brr_timestamp
				DEFAULT now()
);
CREATE INDEX brr_roll_ok_hash
	ON blobio.brr_roll_ok USING hash(blob)
;
CREATE INDEX brr_roll_ok_start_time
	ON blobio.brr_roll_ok(start_time)
;
COMMENT ON TABLE blobio.brr_roll_ok
  IS
  	'history of successfull roll requests'
;
REVOKE UPDATE ON blobio.brr_roll_ok FROM public;

/*
 *  Fetchable blobs with regard to the blobio network service.
 */
DROP VIEW IF EXISTS blobio.service CASCADE;
CREATE VIEW blobio.service AS
  SELECT
	ok.blob,
	ok.start_time AS "recent_time",
	d.start_time AS "discover_time"
    FROM
  	blobio.brr_ok_recent ok
	  LEFT OUTER JOIN blobio.brr_take_ok3_recent take ON
	  (
	  	take.blob = ok.blob
		AND
		take.start_time >= ok.start_time
	  )
	  LEFT OUTER JOIN blobio.brr_no_recent no ON
	  (
	  	no.blob = ok.blob
		AND
		no.start_time >= ok.start_time
	  )
	  INNER JOIN blobio.brr_discover d ON (d.blob = ok.blob)
    WHERE
	take.blob IS null
	AND
	no.blob IS null
;
COMMENT ON VIEW blobio.service
  IS
  	'all verified blobs managed by the server'
;

/*
 *  Blobs taken FROM the service and, therefore, not in service
 */
DROP VIEW IF EXISTS blobio.taken CASCADE;
CREATE VIEW blobio.taken AS
  SELECT
	take.blob,
	take.start_time AS "recent_time"
    FROM
  	blobio.brr_take_ok3_recent take
	  LEFT OUTER JOIN blobio.brr_ok_recent ok ON
	  (
	  	ok.blob = take.blob
		AND
		ok.start_time >= take.start_time
	  )
    WHERE
	ok.blob IS null
;
COMMENT ON VIEW blobio.taken
  IS
  	'blobs taken FROM the service and, therefore, not in service'
;

/*
 *  View of once discovered blobs that are neither in service nor taken
 *  By design, the 'missing' set should always be empty.
 */
DROP VIEW IF EXISTS blobio.missing CASCADE;
CREATE VIEW blobio.missing AS
  SELECT
  	dis.blob,
	dis.start_time AS "discover_time"
    FROM
    	blobio.brr_discover dis
	  LEFT OUTER JOIN blobio.service srv ON (srv.blob = dis.blob)
	  LEFT OUTER JOIN blobio.taken take ON (take.blob = dis.blob)
    WHERE
    	srv.blob IS null
	AND
	take.blob IS null
;
COMMENT ON VIEW blobio.missing
  IS
  	'once discovered blobs that are neither in service nor taken'
;
/*
 *  View of udigs for which no associated blob has been digested.
 */
DROP VIEW IF EXISTS blobio.quack CASCADE;
CREATE VIEW blobio.quack AS
  SELECT
  	no.blob,
	no.start_time AS "discover_time"
    FROM
    	blobio.brr_no_recent no
	  LEFT OUTER JOIN blobio.service srv ON (srv.blob = no.blob)
	  LEFT OUTER JOIN blobio.taken take ON (take.blob = no.blob)
	  LEFT OUTER JOIN blobio.missing mis ON (mis.blob = no.blob)
    WHERE
    	srv.blob IS null
	AND
	take.blob IS null
	AND
	mis.blob IS null
;
COMMENT ON VIEW blobio.quack
  IS
  	'blobs which have never been digested'
;

DROP TABLE IF EXISTS blobio.biod_request_stat CASCADE;
CREATE TABLE blobio.biod_request_stat
(
	sample_time	blobio.brr_timestamp
				PRIMARY KEY,

	success_count	blobio.ui63,
	error_count	blobio.ui63,
	timeout_count	blobio.ui63,
	signal_count	blobio.ui63,
	fault_count	blobio.ui63,

	get_count	blobio.ui63,
	put_count	blobio.ui63,
	give_count	blobio.ui63,
	take_count	blobio.ui63,
	eat_count	blobio.ui63,
	wrap_count	blobio.ui63,
	roll_count	blobio.ui63,

	chat_ok_count	blobio.ui63,
	chat_no_count	blobio.ui63,
	chat_no2_count	blobio.ui63,
	chat_no3_count	blobio.ui63,
	eat_no_count	blobio.ui63,
	take_no_count	blobio.ui63
);

COMMIT;
