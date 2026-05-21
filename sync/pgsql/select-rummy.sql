/*
 *  Synopsis:
 *	Find all blobs in postgres blobio.service with missing data.
 *  Usage:
 *	psql --file select-rummy-since.sql --quiet ...
 *  See:
 *	sbin/cron-rummy
 */
SET search_path TO blobio;

SELECT
	srv.blob
  FROM
  	service srv
	  LEFT OUTER JOIN brr_blob_size sz ON (sz.blob = srv.blob)
  WHERE
	sz.blob IS null
;
