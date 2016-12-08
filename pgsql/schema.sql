/*
 *  Synopsis:
 *	PostgreSQL schema to track blobs available in a particular biod server.
 *  Note:
 *	Please add sql comments.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
\set ON_ERROR_STOP on

BEGIN;
DROP SCHEMA IF EXISTS blobio CASCADE;
CREATE SCHEMA blobio;
COMMENT ON SCHEMA blobio IS
  'blobs in service and brr history'
;


DROP DOMAIN IF EXISTS blobio.brr_duration CASCADE;
CREATE DOMAIN brr_duration AS interval
  CHECK (
	value >= '0 seconds'
  )
  NOT NULL
;
COMMENT ON DOMAIN brr_duration IS
  'wall clock duration of a blob request'
;

DROP DOMAIN IF EXISTS blobio.brr_timestamp CASCADE;
CREATE DOMAIN brr_timestamp AS timestamptz
  CHECK (
	value >= '2008-05-17 10:06:42'
  )
  NOT NULL
;
COMMENT ON DOMAIN brr_timestamp IS
  'starting time of request for a blob'
;

/*
 *  Most recently seen successfull take FROM this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
DROP TABLE IF EXISTS blobio.brr_take_ok3_recent CASCADE;
CREATE TABLE brr_take_ok3_recent
(
	blob			public.udig
					primary key,
	start_time		brr_timestamp,

	/*
	 *  Since the physical remove of the blob only occurs after the client
	 *  has the blob, a better estimate of when the blob was removed FROM
	 *  service is start_time + wall_duration.
	 */
	wall_duration		brr_duration
);
COMMENT ON TABLE brr_take_ok3_recent IS
  'most recently seen take request record for a particular blob'
;
CREATE INDEX brr_take_ok3_recent_start_time ON brr_take_ok3_recent(start_time);

/*
 *  The blob size AS observed in a brr record of an existing blob.
 *  Blob size should NEVER change.
 */
DROP TABLE IF EXISTS blobio.brr_blob_size CASCADE;
CREATE TABLE brr_blob_size
(
	blob		udig
				primary key,
	byte_count	bigint
				check (
					byte_count >= 0
				)
				not null
);
COMMENT ON TABLE brr_blob_size IS
  'number bytes (octets) in the blob'
;
REVOKE UPDATE ON brr_blob_size FROM public;

/*
 *  A recently verified existence of a blob AS seen in a brr record.
 */
DROP TABLE IF EXISTS blobio.brr_ok_recent CASCADE;
CREATE TABLE brr_ok_recent
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
COMMENT ON TABLE brr_ok_recent IS
  'most recently verified existence for a particular blob'
;
CREATE INDEX brr_ok_recent_start_time ON brr_ok_recent(start_time);

/*
 *  A recently failed read of a blob, which implies the blob may not exist.
 *  Don't record successfull "take"s, since
 *  has been completely forgotten.
 */
DROP TABLE IF EXISTS blobio.brr_no_recent CASCADE;
CREATE TABLE brr_no_recent
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
COMMENT ON TABLE brr_no_recent IS
  'most recently failed attempt to get or eat the blob'
;
CREATE INDEX brr_no_recent_start_time ON brr_no_recent(start_time);

/*
 *  Earliest known existence of a blob.  Both the brr time
 *  and the insert/update of the record are recorded.
 */
DROP TABLE IF EXISTS blobio.brr_discover;
CREATE TABLE brr_discover
(
	blob		udig
				primary key,
	/*
	 *  Start time in blob request record.
	 */
	start_time	brr_timestamp
				not null,
	/*
	 *  Time this database record was inserted or updated with
	 *  an earlier time,  effective measuring discover latency.
	 */
	upsert_time	brr_timestamp
				default now()
				not null
);
CREATE INDEX brr_discover_start_time ON brr_discover(start_time);
COMMENT ON TABLE brr_discover
  IS
  	'the earliest known existence of a digestable blob for this service'
;

COMMENT ON COLUMN brr_discover.start_time
  IS
  	'start time of the earliest request for the discovered blob'
;

COMMENT ON COLUMN brr_discover.upsert_time
  IS
  	'time of sql insert or update of this tuple'
;

DROP TABLE IF EXISTS blobio.brr_wrap_ok;
CREATE TABLE brr_wrap_ok
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration,
	insert_time	timestamptz
				default now()
				not null
);
REVOKE UPDATE ON brr_wrap_ok FROM public;
COMMENT ON TABLE brr_wrap_ok
  IS
  	'history of successfull wrap requests'
;


DROP TABLE IF EXISTS blobio.brr_roll_ok;
CREATE TABLE brr_roll_ok
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration,
	insert_time	timestamptz
				default now()
				not null
);
REVOKE UPDATE ON brr_roll_ok FROM public;
COMMENT ON TABLE brr_roll_ok
  IS
  	'history of successfull roll requests'
;

/*
 *  Fetchable blobs with regard to the blobio network service.
 *  In other words, for, say, service bio4:localhost:1797,
 *  if <udig> is in the table service.blob then
 *
 *	blobio get --udig <udig> --service bio4:localhost:1797
 *
 *  fetches a living blob.
 */
DROP VIEW IF EXISTS blobio.service CASCADE;
CREATE VIEW service AS
  SELECT
	ok.blob,
	ok.start_time AS "recent_time",
	d.start_time AS "discover_time"
    FROM
  	brr_ok_recent ok
	  LEFT OUTER JOIN brr_take_ok3_recent take ON
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
DROP VIEW IF EXISTS blobio.taken CASCADE;
CREATE VIEW taken AS
  SELECT
	take.blob,
	take.start_time AS "recent_time"
    FROM
  	brr_take_ok3_recent take
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
DROP VIEW IF EXISTS blobio.missing CASCADE;
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
DROP VIEW IF EXISTS blobio.quack CASCADE;
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

COMMIT;
