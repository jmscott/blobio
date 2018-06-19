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

\echo select blobs in chain that are known to be taken
\o chain-taken.udig
SELECT
	c.blob
  FROM
  	chain c
  	  JOIN taken t ON (t.blob = c.blob)
  ORDER BY
    	c.blob
;

\echo select blobs in chain that are known to be missing
\o chain-missing.udig
SELECT
	c.blob
  FROM
  	chain c
  	  JOIN missing m ON (m.blob = c.blob)
  ORDER BY
    	c.blob
;

\echo Select blobs not in service
\o chain-not-in-service.udig
SELECT
  	c.blob
  FROM
	chain c
    	  LEFT OUTER JOIN service s ON (s.blob = c.blob)
	  LEFT OUTER JOIN taken t ON (t.blob = c.blob)
	  LEFT OUTER JOIN missing m ON (m.blob = c.blob)
  WHERE
    	s.blob IS NULL		--  not in service
	AND
    	t.blob IS NULL		--  not known to be taken
	AND
	m.blob IS NULL		--  not known to be missing
  ORDER BY
    	c.blob
;

\echo Select :chain_count blobs in chain
\o chain.udig
SELECT
	c.blob
  FROM
  	chain c
  ORDER BY
  	c.blob
;
