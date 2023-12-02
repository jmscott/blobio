/*
 *  Synopsis:
 *	Find all blobs in postgres blobio.service with missing data.
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
	sz.blob IS null
;
