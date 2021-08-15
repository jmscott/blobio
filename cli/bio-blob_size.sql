/*
 *  Synopsis:
 *	SQL to calculate size of set of udigs read from stdin input.
 *  See:
 *	script $BLOBIO_ROOT/bin/bio-sizeof-stdin
 */

\set ON_ERROR_STOP

CREATE TEMP TABLE cli_bio_byte_count(
	blob	udig
);

\copy cli_bio_byte_count FROM PSTDIN

CREATE UNIQUE INDEX idx_cli_bio_byte_count ON cli_bio_byte_count(blob);
ANALYZE cli_bio_byte_count;

SELECT
	sum(sz.byte_count),
	pg_size_pretty(sum(sz.byte_count))
  FROM
  	cli_bio_byte_count cli
	  JOIN blobio.brr_blob_size sz ON (sz.blob = cli.blob)
;
