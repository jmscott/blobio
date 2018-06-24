/*
 *  Synopsis:
 *	Select blobs in service during an inclusive time interval
 *  Usage:
 *	psql --file service-by-interval.sql				\
 *		--set min-time="'yesterday'"				\
 *		--set max-time="'today'"
 *	
 */
\pset tuples_only
\set ON_ERROR_STOP

SELECT
	blob
  FROM
  	service
  WHERE
	recent_time BETWEEN :min_time AND :max_time
	OR
	discover_time BETWEEN :min_time AND :max_time
;
