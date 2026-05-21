/*
 *  Synopsis:
 *	Find blobs in postgres blobio.service with missing data since a PIT.
 *  Usage:
 *	psql --file select-rummy-since.sql -v since='-1 week' --quiet
 *  See:
 *	sbin/cron-rummy
 */
SET search_path TO blobio;

SELECT
	r.blob
  FROM
  	service srv
	  JOIN rummy r ON (r.blob = srv.blob)
  WHERE
  	srv.discover_time > now() + :'since'
;
