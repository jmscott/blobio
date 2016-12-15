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
	to_timestamp(sample_epoch),

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

\x on
\echo summarize loaded samples (not all)
WITH sample_interval AS (
  SELECT
  	to_timestamp(min(sample_epoch)) as "min_time",
  	to_timestamp(max(sample_epoch)) as "max_time",
	count(*) as "sample_count"
    FROM
    	load_biod_request_stat
)
SELECT
	'' AS "LOADED",
	si.sample_count as "Sample Count",
	(si.max_time - si.min_time) || ' hours' AS "Duration",
	si.min_time AS "Start Time",
	si.max_time AS "End Time",
	'' AS " ",
	'TOTAL/AVERAGE' AS "REQUESTS",
	sum(success_count) || '/' || avg(success_count)::int
		AS "Success",
	sum(error_count) || '/' || avg(error_count)::int
		AS "Error",
	sum(signal_count) || '/' || avg(signal_count)::int
		AS "Signaled",
	sum(fault_count) || '/' || avg(fault_count)::int
		AS "Faulted",
	'' AS " ",
	'TOTAL/AVERAGE' AS "VERBS",
	sum(get_count) || '/' || avg(get_count)::int AS "Gets",
	sum(put_count) || '/' || avg(put_count)::int AS "Puts",
	sum(give_count) || '/' || avg(give_count)::int AS "Gives",
	sum(eat_count) || '/' || avg(eat_count)::int AS "Eats",
	sum(wrap_count) || '/' || avg(wrap_count)::int AS "Wraps",
	sum(roll_count) || '/' || avg(roll_count)::int AS "Rolls",
	'' AS " ",
	'TOTAL/AVERAGE' AS "CHAT HISTORIES",
	sum(chat_ok_count) || '/' || avg(chat_ok_count)::int AS "ok*",
	sum(chat_no_count) || '/' || avg(chat_no_count)::int AS "no",
	sum(chat_no2_count) || '/' || avg(chat_no2_count)::int AS "no,no",
	sum(chat_no3_count) || '/' || avg(chat_no3_count)::int AS "no,no,no",
	sum(eat_no_count) || '/' || avg(eat_no_count)::int AS "Eats no",
	sum(take_no_count) || '/' || avg(take_no_count)::int AS "Takes no"
  FROM
  	blobio.biod_request_stat brs,
	sample_interval si
  WHERE
  	brs.sample_time BETWEEN si.min_time AND si.max_time
  GROUP BY
  	si.min_time,
	si.max_time,
	si.sample_count
;
