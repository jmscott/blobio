SELECT
	srv.blob
  FROM
  	blobio.service srv
	  JOIN blobio.brr_blob_size sz ON (sz.blob = srv.blob)
  WHERE
  	srv.discover_time > now() + :'since'
	AND
	sz.blob IS null
;
