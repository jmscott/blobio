/*
 *  Synopsis:
 *	Schema that tracks blobs available in a service (bio4, fs, cache).
 *  Note:
 *	No function to get owner of current_database!  Need to write a function
 *	in future pgfabric system: pgfabic.database_owner()!
 *
 *	Set COLLATIONS to C!  blob request records are by definition ascii.
 *
 *	Still have not resolved if verb from any transport should have same
 *	chat history!  See discuss in ../README.
 */
\set ON_ERROR_STOP on

\echo UDIG SQL: :UDIG_PATH
\echo DATABASE OWNER: :DBOWNER

BEGIN TRANSACTION;

DROP SCHEMA IF EXISTS blobio CASCADE;
CREATE SCHEMA blobio;
COMMENT ON SCHEMA blobio IS
  'Blobs in service and summaries of historical blob request records'
;
ALTER SCHEMA blobio OWNER TO :DBOWNER;

\echo create udig type in schema blobio
\echo include :UDIG_PATH
SET search_path to blobio,public;
\include :UDIG_PATH

ALTER TYPE udig SET SCHEMA blobio;

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
ALTER DOMAIN brr_duration OWNER TO :DBOWNER;

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
ALTER DOMAIN brr_timestamp OWNER TO :DBOWNER;

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
ALTER DOMAIN ui63 OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS ui31 CASCADE;
CREATE DOMAIN ui31 AS int
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui31 IS
  'non null integer in range 0 <= 2^31 - 1'
;
ALTER DOMAIN ui31 OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS ui15 CASCADE;
CREATE DOMAIN ui15 AS smallint
  CHECK (
	value >= 0
  )
  NOT NULL
;
COMMENT ON DOMAIN ui15 IS
  'non null integer in range 0 <= 2^15 - 1'
;
ALTER DOMAIN ui15 OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS ui8 CASCADE;
CREATE DOMAIN ui8 AS smallint
  CHECK (
	value >= 0
	AND
	value < 256
  )
  NOT NULL
;
COMMENT ON DOMAIN ui8 IS
  'non null integer in range 0 <= 2^8 - 1'
;
ALTER DOMAIN ui8 OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS brr_transport CASCADE;
CREATE DOMAIN brr_transport AS text
  CHECK (
  	value ~ '[a-z][a-z0-9]{0,7}~[[:graph:]]{1,128}'
	AND
	value IS NOT NULL
  )
;
COMMENT ON DOMAIN brr_transport IS
  'Network transport for a blob request record'
;
ALTER DOMAIN brr_transport OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS brr_verb CASCADE;
CREATE DOMAIN brr_verb AS text
  CHECK (
  	value IN (
		'get',
		'put',
		'give',
		'take',
		'wrap',
		'roll',
		'eat',
		'cat'
	)
	AND
	value IS NOT NULL
);
COMMENT ON DOMAIN brr_verb IS
  'The 8 deadly verbs of a blob request record'
;
ALTER DOMAIN brr_verb OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS brr_chat_history CASCADE;
CREATE DOMAIN brr_chat_history AS text
  CHECK (
  	value IN (
		'ok',
		'no',
		'ok,ok',
		'ok,no',
		'ok,ok,ok',
		'ok,ok,no'
	)
	AND
	value IS NOT NULL
);
COMMENT ON DOMAIN brr_chat_history IS
  'Blob Request Chat (ok/no) History'
;
ALTER DOMAIN brr_chat_history OWNER TO :DBOWNER;

--  Note: create type forbids "NOT NULL" for a field

DROP DOMAIN IF EXISTS brr_udig CASCADE;
CREATE DOMAIN brr_udig AS blobio.udig CHECK (
	--  Note: create type forbids "NOT NULL" for a field
	value IS NOT NULL
);
COMMENT ON DOMAIN brr_udig IS
  'A uniform digest in a blob request record (never null)'
;
ALTER DOMAIN brr_udig OWNER TO :DBOWNER;

DROP TYPE IF EXISTS blob_request_record CASCADE;
CREATE TYPE blob_request_record AS (
	start_time	brr_timestamp,
	transport	brr_transport,
	verb		brr_verb,
	blob		brr_udig,
	chat_history	brr_chat_history,
	blob_size	ui63,
	wall_duration	brr_duration
);
COMMENT ON TYPE blob_request_record IS
  'A blob request record type'
;
ALTER TYPE blob_request_record OWNER TO :DBOWNER;

DROP DOMAIN IF EXISTS brr CASCADE;
CREATE DOMAIN brr AS blob_request_record
  CHECK (
  	(
		(value).verb = 'get'
		AND
		(value).chat_history IN ('ok', 'no')
	)
	OR
  	(
		(value).verb = 'put'
		AND
		(value).chat_history IN ('no', 'ok,ok', 'ok,no')
	)
	OR
  	(
		(value).verb IN ('eat', 'wrap', 'roll')
		AND
		(value).chat_history IN ('no', 'ok')
		AND
		(value).blob_size = 0
	)
	OR
  	(
		(value).verb IN ('give', 'take')
		AND
		(value).chat_history IN ('no', 'ok,ok,ok', 'ok,no', 'ok,ok,no')
	)
  )
;
COMMENT ON DOMAIN brr IS
  'Qualified Blob Request Record, (no quals in SQL TYPE blob_request record)'
;
ALTER DOMAIN brr OWNER TO :DBOWNER;

/*
 *  The immutable blob size AS observed in a brr record of an existing blob.
 */
DROP TABLE IF EXISTS brr_blob_size CASCADE;
CREATE TABLE brr_blob_size
(
	blob		blobio.udig
				PRIMARY KEY,
	byte_count	ui63	NOT NULL,

	CONSTRAINT size_check CHECK (
	  (
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
COMMENT ON TABLE brr_blob_size IS
  'number bytes (octets) in the blob as reported by blob request record'
;
CREATE INDEX idx_brr_blob_size_byte_count
	ON brr_blob_size(byte_count)
;
CREATE INDEX idx_brr_blob_size_hash
	ON brr_blob_size USING hash(blob)
;
CLUSTER brr_blob_size USING idx_brr_blob_size_byte_count;
ALTER TABLE brr_blob_size OWNER TO :DBOWNER;

/*
 *  Earliest known existence of a blob.  Both the brr time
 *  and the insert/update of the record are recorded.
 */
DROP TABLE IF EXISTS brr_discover;
CREATE TABLE brr_discover
(
	blob		blobio.udig
				PRIMARY KEY,
	/*
	 *  Start time in blob request record.
	 */
	start_time	brr_timestamp NOT NULL
);
COMMENT ON TABLE brr_discover
  IS
  	'the earliest known existence of a digestable blob for this service'
;
COMMENT ON COLUMN brr_discover.start_time
  IS
  	'start time of the earliest request for the discovered blob'
;
CREATE INDEX idx_brr_discover_hash ON brr_discover USING hash(blob);
CREATE INDEX idx_brr_discover_start_time ON brr_discover(start_time);
CLUSTER brr_discover USING idx_brr_discover_start_time;
ALTER TABLE brr_discover OWNER TO :DBOWNER;

/*
 *  Fetchable blobs with regard to the blobio network service.
 */
DROP VIEW IF EXISTS service CASCADE;
CREATE VIEW service AS
  SELECT
	blob,
	start_time AS discover_time
    FROM
    	brr_discover
;
COMMENT ON VIEW service
  IS
  	'all fetchable blobs managed by the server'
;
ALTER VIEW service OWNER TO :DBOWNER;

/*
 *  Synopsis:
 *	Find all blobs in blobio.service with missing data.
 */

DROP VIEW IF EXISTS rummy CASCADE;
CREATE VIEW rummy AS
  SELECT
	srv.blob
    FROM
  	blobio.service srv
	  LEFT OUTER JOIN blobio.brr_blob_size sz ON (sz.blob = srv.blob)
  WHERE
	sz.blob IS null
;
COMMENT ON view rummy IS
  'Blobs with known unknown attributes'
;
ALTER VIEW rummy OWNER TO :DBOWNER;

DROP FUNCTION IF EXISTS interval_terse_english(interval) CASCADE;
CREATE OR REPLACE FUNCTION interval_terse_english(duration interval)
  RETURNS text AS $$
  DECLARE
  	elapsed numeric;
	english text;

  	yr numeric;
  	yr_unit text;

  	mon numeric;
  	mon_unit text;

  	day numeric;
  	day_unit text;

  	hr numeric;
  	hr_unit text;

  	min numeric;
  	min_unit text;

	sec numeric;
	sec_unit text;

	sec_per_yr numeric;
	sec_per_mon numeric;
	sec_per_day numeric;
	sec_per_hr numeric;
	sec_per_min numeric;
  BEGIN
  	elapsed = round(extract(epoch from duration));

	sec_per_yr = extract(epoch from '1 year'::interval);
	sec_per_mon = extract(epoch from '1 mon'::interval);
	sec_per_day = extract(epoch from '1 day'::interval);
	sec_per_hr = extract(epoch from '1 hour'::interval);
	sec_per_min = extract(epoch from '1 minute'::interval);

	yr = 0;
	yr_unit = ' ';

	mon = 0;
	mon_unit = ' ';

	day = 0;
	day_unit = ' ';

	hr = 0;
	hr_unit = ' ';

	min = 0;
	min_unit = ' ';

	sec = 0;
	sec_unit = ' ';

	-- years

	if elapsed >= sec_per_yr then
		yr = round(elapsed / sec_per_yr);
		yr_unit = 'yr ';

		elapsed = elapsed - (yr * sec_per_yr);
	end if;

	--  months

	if elapsed >= sec_per_mon then
		mon = round(elapsed / sec_per_mon);
		mon_unit = 'mon ';

		elapsed = elapsed - (mon * sec_per_mon);
	end if;

	--  days

	if elapsed >= sec_per_day then
		day = round(elapsed / sec_per_day);
		day_unit = 'day ';

		elapsed = elapsed - (day * sec_per_day);
	end if;

	--  hours
	if elapsed >= sec_per_hr then
		hr = round(elapsed / sec_per_hr);
		hr_unit = 'hr ';

		elapsed = elapsed - (hr * sec_per_hr);
	end if;

	--  minutes
	if elapsed >= sec_per_min then
		min = round(elapsed / sec_per_min);
		min_unit = 'min ';

		elapsed = elapsed - (min * sec_per_min);
	end if;

	--  seconds

	if elapsed > 0 then
		sec = round(elapsed);
		sec_unit = 'sec ';
	end if;

	english =
		yr || yr_unit ||
		mon || mon_unit ||
		day || day_unit ||
		hr || hr_unit ||
		min || min_unit ||
		sec || sec_unit
	;
	english = regexp_replace(english, ' *0 *', '', 'g');
	english = regexp_replace(english, '^( *0)+', '');
	english = regexp_replace(english, '( *0 *)+$', '');
	if english = '' then
		english = '0sec';
	end if;

	return english;

  END $$ LANGUAGE plpgsql IMMUTABLE STRICT
;
COMMENT ON FUNCTION interval_terse_english(interval) IS
  'Convert time interval to full precision, terse english, e.g. 3hr12min5sec'
;

COMMIT TRANSACTION;
