/*
 *  Synopsis:
 *	Schema for crunchying flow,query,process description records.
 */
\set ON_ERROR_STOP 1

SET search_path TO blobio,public;

BEGIN;
DROP TABLE IF EXISTS fdr_stage;
CREATE UNLOGGED TABLE fdr_stage
(
	start_time	timestamptz NOT NULL,
	blob		udig NOT NULL,
	ok_count	smallint CHECK (
				ok_count >= 0
			) NOT NULL,
	fault_count	smallint CHECK (
				fault_count >= 0
			),
	wall_duration	interval CHECK (
				wall_duration >= '0 sec'
			)NOT NULL,
	sequence	bigint CHECK (
				sequence > 0
			),
	PRIMARY KEY	(start_time, sequence)
);
COMMENT ON TABLE fdr_stage IS
  'Staging table for FDR records being written into Round Robin Database'
;

END;
