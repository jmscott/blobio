/*
 *  Synopsis:
 *	Find "quack" blobs.
 * Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
\set ON_ERROR_STOP 1
\pset tuples_only
\pset format unaligned

CREATE TEMPORARY TABLE wrapped
(
	blob	udig
			PRIMARY KEY
);

\copy wrapped from pstdin

VACUUM ANALYZE WRAPPED;

SELECT
	w.blob
  FROM
  	wrapped w
	  LEFT OUTER JOIN blobio.brr_discover dis ON (dis.blob = w.blob)
	  LEFT OUTER JOIN blobio.brr_no_recent no ON (no.blob = w.blob)
  where
  	dis.blob is NULL
	and
	no.blob is NULL
;
