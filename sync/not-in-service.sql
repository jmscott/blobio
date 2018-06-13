/*
 *  Synopsis:
 *	Find udigs from stdin that are not in service table.
 *  Usage:
 *	sync-brr-chain						|
 *		grep '^[0-9a-z][0-9a-z]*'			|
 *		psql -f not-in-service.sql >not-in-service.udig
 *	wc -l chain-not-in-service.udig
 */
\set ON_ERROR_STOP 1
\timing 1

SET search_path TO blobio,public;

CREATE TEMP TABLE chain
(
	blob			udig
					PRIMARY KEY
);
CREATE INDEX chain_hash ON
	chain USING hash(blob)
;

\copy chain from pstdin
ANALYZE chain;

\pset tuples_only
\pset format unaligned
SELECT
	count(*) AS chain_count
  FROM
  	chain
\gset
\echo
\echo Chain count is :chain_count

WITH out_of_service AS (
  SELECT
	c.blob
  FROM
  	chain c
	  LEFT OUTER JOIN service s ON (s.blob = c.blob)
  WHERE
  	s.blob is null
)
  SELECT
  	o.blob
    FROM
    	out_of_service o
	  LEFT OUTER JOIN taken t ON (t.blob = o.blob)
    WHERE
    	o.blob is null
;
