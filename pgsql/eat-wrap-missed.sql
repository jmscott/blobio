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

create temporary table wrapped
(
	blob	udig
			primary key
);

\copy wrapped from pstdin

vacuum analyze wrapped;

select
	w.blob
  from
  	wrapped w
	  left outer join blobio.brr_discover dis on (dis.blob = w.blob)
	  left outer join blobio.brr_no_recent no on (no.blob = w.blob)
  where
  	dis.blob is null
	and
	no.blob is null
;
