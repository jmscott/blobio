/*
 *  Synopsis:
 *	Fetch blob count and size aggregates since a point in time.
 *  Usage:
 *	psql --file go.sql --set since="'-24 hours'" --set udig_out=local.udig
 *  Note:
 *	Think about measuring synchronicity when bandwidth is asymetrical
 *	between two hosts.  For example, the start_times of two sunk blobs
 *	will be close but the wall_duration will be large, so the blob
 *	is technically not sunk ... which this report does not clarify.
 */
\timing on
set search_path to blobio,public;

\x on
SELECT
	now() AS "Now",
	inet_server_addr() || ':' || inet_server_port() AS "Server:Port",
	:since || ' ago' AS "Since",
	count(*) AS "Blob Count",
	sum(bs.byte_count) || ' bytes (' || 
		pg_size_pretty(sum(bs.byte_count)) || ')'
		AS "Total Byte Count",
	max(bs.byte_count) || ' bytes (' ||
		pg_size_pretty(max(bs.byte_count)) || ') / ' ||
	min(bs.byte_count) || ' bytes (' ||
		pg_size_pretty(min(bs.byte_count)) || ')'
		AS "Max / Min Blob",
	round(avg(bs.byte_count)::numeric, 2) || ' bytes (' ||
		pg_size_pretty(round(avg(bs.byte_count)::numeric)) || ')'
			AS "Average Blob Size",
	round(stddev(bs.byte_count)::numeric, 2) || ' bytes (' ||
		pg_size_pretty(round(stddev(bs.byte_count)::numeric, 2)) || ')'
	AS "StdDev Blob Size"
  FROM
  	service s
	  JOIN brr_blob_size bs ON (bs.blob = s.blob)
  WHERE
  	s.recent_time >= now() + :since
;

\timing off
\x off
\o :udig_out
SELECT
	s.blob
  FROM
  	service s
	  JOIN brr_blob_size bs ON (bs.blob = s.blob)
  WHERE
  	s.recent_time >= now() + :since
  ORDER BY
  	s.blob
;
