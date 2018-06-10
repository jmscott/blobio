\set ON_ERROR_STOP 1
\timing 1

/*
 *  Most recently seen successfull take FROM this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
CREATE TEMP TABLE chain
(
	blob			udig
					PRIMARY KEY
);
CREATE INDEX chain_hash ON
	chain USING hash(blob)
;

\copy chain from chain.udig
ANALYZE chain;

\pset tuples_only
\pset format unaligned
\o chain-not-in-service.udig
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
