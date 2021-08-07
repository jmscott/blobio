/*
 *  Synopsis:
 *	Find blobs in postgres blobio.service with missing data since a PIT.
 *  Usage:
 *	psql -f select-rummy-since.sql
 *  See:
 *	sbin/cron-rummy
 */
SELECT
	srv.blob
  FROM
  	blobio.service srv
	  LEFT OUTER JOIN blobio.brr_blob_size sz ON (sz.blob = srv.blob)
  WHERE
  	srv.discover_time > now() + :'since'
	AND
	sz.blob IS null
;
