/*
 *  Synopsis:
 *	Output blobs not in service, udigs read from standard input.	
 */

\set ON_ERROR_STOP true
\pset format unaligned
\pset recordsep '\n'
\pset tuples_only

CREATE TEMP TABLE not_in_service(
	blob	udig
);

\copy not_in_service FROM PSTDIN

CREATE UNIQUE INDEX idx_not_in_service
  ON
  	not_in_service(blob)
;
ANALYZE not_in_service;

SELECT
	blob
  FROM
  	not_in_service ns
  WHERE NOT EXISTS (
    SELECT
    	srv.blob
    FROM
    	blobio.service srv
    WHERE
    	srv.blob = ns.blob
  )
;
