\set ON_ERROR_STOP 1
\timing 1

/*
 *  Most recently seen successfull take FROM this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
CREATE TEMP TABLE rolled
(
	blob			udig
					PRIMARY KEY
);
CREATE INDEX rolled_hash ON
	rolled USING hash(blob)
;

\copy rolled from go.udig
ANALYZE rolled;

\pset tuples_only
\pset format unaligned
\o rolled.udig
WITH out_of_service AS (
  SELECT
	r.blob
  FROM
  	rolled r LEFT OUTER JOIN service s ON (s.blob = r.blob)
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
