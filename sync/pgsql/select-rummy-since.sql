/*
 *  Synopsis:
 *	Find blobs in postgres blobio.service with missing data since a PIT.
 *  Usage:
 *	psql -f select-rummy-since.sql -v since='-1 week'
 *  See:
 *	sbin/cron-rummy
 */
SELECT
	r.blob
  FROM
  	blobio.service srv
	  JOIN blobio.rummy r ON (r.blob = srv.blob)
  WHERE
  	srv.discover_time > now() + :'since'
;
