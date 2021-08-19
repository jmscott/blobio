/*
 *  Synopsis:
 *	SQL to calculate size of set of udigs read from stdin input.
 *  Usage:
 *	bio-blob_size <<END
 *	bc160:a8affdfc1058fe9191b7730aaa0e1b6765d12f05
 *	sha:8e8475241113dcdb461873a329748c8ec9f50363
 *	sha:18f74f6332e37185b2216c646d3617de4427fadc
 *	END
 *
 *	2 MB	1048576	1	2
 *  See:
 *	script $BLOBIO_ROOT/bin/bio-sizeof-stdin
 *  Note:
 *	We can know the size of a blob that is not in service.
 */

\set ON_ERROR_STOP

CREATE TEMP TABLE cli_bio_byte_count(
	blob	udig
);

\copy cli_bio_byte_count FROM PSTDIN

CREATE UNIQUE INDEX idx_cli_bio_byte_count ON cli_bio_byte_count(blob);
ANALYZE cli_bio_byte_count;

SELECT
	--  human readable of total known size
	pg_size_pretty(sum(sz.byte_count)),

	--  total known byte count
	sum(sz.byte_count),

	--  total blob count with known sizes
	count(cli.blob),

	--  total blob count with unknown sizes
	(SELECT
		count(cli2.blob) - count(cli.blob)
	  FROM
	  	cli_bio_byte_count cli2
	),

	--  total blob count not in service
	(SELECT
		count(uno.blob)
	  FROM
	  	cli_bio_byte_count uno
	  WHERE
	  	NOT EXISTS (
		  SELECT
		  	srv.blob
		    FROM
		    	blobio.service srv
		    WHERE
		    	srv.blob = uno.blob
		)
	)
  FROM
  	cli_bio_byte_count cli
	  JOIN blobio.brr_blob_size sz ON (sz.blob = cli.blob)
;
