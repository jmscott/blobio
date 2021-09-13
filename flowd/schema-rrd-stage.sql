/*
 *  Synopsis:
 *	Schema for crunching Flow Descr Records into RRD samples.
 */
\set ON_ERROR_STOP 1

SET search_path TO blobio,public;

BEGIN;
DROP TABLE IF EXISTS rrd_stage_fdr;
CREATE UNLOGGED TABLE rrd_stage_fdr
(
	start_time	timestamptz CHECK (
				--  epoch for RRD graphs.
				start_time >= '2021-08-01'
			) NOT NULL,
	sequence	bigint CHECK (
				sequence > 0
			),
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
	PRIMARY KEY	(start_time, sequence)
);
COMMENT ON TABLE rrd_stage_fdr IS
  'Round Robin DB Staging Table for Flow Description Records'
;

END;
