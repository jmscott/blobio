/*
 *  Synopsis:
 *	Load request stats into table blobio.biod_request_stat
 */
\set ON_ERROR_STOP on

\echo create temp table load_biod_request_stat
CREATE TEMP TABLE load_biod_request_stat AS
  SELECT
  	0::bigint as "sample_epoch",

	success_count,
	error_count,
	timeout_count,
	signal_count,
	fault_count,

	get_count,
	put_count,
	give_count,
	take_count,
	eat_count,
	wrap_count,
	roll_count,

	chat_ok_count,
	chat_no_count,
	chat_no2_count,
	chat_no3_count,

	eat_no_count,
	take_no_count
    FROM
    	blobio.biod_request_stat
    WHERE
    	0 = 1
;

\echo load stats into temp load table from stdin
\copy load_biod_request_stat from pstdin DELIMITER ':'

\echo analyze table load_biod_request_stat
ANALYZE load_biod_request_stat;

\echo insert new stats into load_biod_request_stat
INSERT INTO blobio.biod_request_stat(
	sample_time,

	success_count,
	error_count,
	timeout_count,
	signal_count,
	fault_count,

	get_count,
	put_count,
	give_count,
	take_count,
	eat_count,
	wrap_count,
	roll_count,

	chat_ok_count,
	chat_no_count,
	chat_no2_count,
	chat_no3_count,

	eat_no_count,
	take_no_count
) SELECT
	TIMESTAMP 'epoch' + sample_epoch * INTERVAL '1 second',

	success_count,
	error_count,
	timeout_count,
	signal_count,
	fault_count,

	get_count,
	put_count,
	give_count,
	take_count,
	eat_count,
	wrap_count,
	roll_count,

	chat_ok_count,
	chat_no_count,
	chat_no2_count,
	chat_no3_count,

	eat_no_count,
	take_no_count
  FROM
  	load_biod_request_stat
  ON CONFLICT
  	DO NOTHING
;

\echo vacuum/analyze table biod_request_stat
VACUUM ANALYZE blobio.biod_request_stat;
