/*
 *  Synopsis:
 *	Generate Round Robin Database update samples on standard output.
 *  Description:
 *	Looking back two minutes, write a minute stepped set of tuples
 *	suitable for rrdupdate. A "zero" fdr record means both ok_count
 *	and fault_count are zero.
 *
 *		epoch:			#  rounded to a minute
 *		fdr_count:		#  number of flow desc records/min
 *		blob_count:		#  distinct number of blobs/min
 *		fdr_blob_size:		#  sum of byte count for fdr blobs
 *		ok_count:		#  total number of ok countd/min
 *		fault_count:		#  total number of faults countd/min
 *		wall_duration_sec:	#  sec rounded sum of wall durations
 *		zero_fdr_count:		#  number of zero flow records
 *		zero_blob_count:	#  distinct number of blobs zero fdr
 *  Note:
 *	Eventually replace this sql script with perl code.  Silly using
 *	expensive SQL server for simple, pure transformation.  Perhaps
 *	a job for sqllight.
 */

\pset null U
\pset tuples_only
\pset format unaligned
\pset fieldsep ':'

SET search_path TO blobio,public;

SELECT
	extract('epoch' from
		date_trunc('minute', st.start_time + st.wall_duration))
		AS start_minute,
	count(*)
		AS fdr_count,
	count(DISTINCT st.blob)
		AS blob_count,
	sum(bs.byte_count)
		AS fdr_blob_size,
	sum(st.ok_count)
		AS ok_count,
	sum(st.fault_count)
		AS fault_count,
	round(extract('second' from sum(st.wall_duration))::numeric, 0)
		AS wall_duration_sec,
	count(*)
		FILTER(WHERE st.ok_count = 0 AND st.fault_count = 0)
	  	AS zero_fdr_count,
	count(DISTINCT st.blob)
		FILTER(WHERE st.ok_count = 0 AND st.fault_count = 0)
		AS zero_blob_count
  FROM
	rrd_stage_fdr st
	  LEFT OUTER JOIN brr_blob_size bs ON (
	  	bs.blob = st.blob
	  )
  GROUP BY
  	start_minute
  ORDER BY
  	start_minute ASC
;
