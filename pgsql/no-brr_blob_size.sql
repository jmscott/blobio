/*
 *  Synopsis:
 *	Select in service blobs with no brr_size_record since a point in time.
 *  Usage:
 *	psql									\
 *		--file no-brr_blob_size.sql					\
 *		--set since="'$SINCE'"						\
 *		--quiet								\
 *		--no-align							\
 *		--no-psqlrc							\
 *		--tuples-only							\
 */

SELECT
	s.blob
  FROM
  	blobio.service s
  WHERE
  	s.discover_time >= now() + :since
	AND
  	NOT EXISTS (
	  SELECT
	  	bs.blob
	    FROM
	    	blobio.brr_blob_size bs
	    WHERE
	    	bs.blob = s.blob
	)
;
