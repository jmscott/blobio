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
	source		tag,
	start_time	brr_timestamp,
	sequence	ui63,
	blob		udig NOT NULL,
	ok_count	ui16,
	fault_count	ui16,
	wall_duration	brr_duration,
	PRIMARY KEY	(start_time, source, sequence)
);
CREATE INDEX idx_rrd_stage_fdr
  ON rrd_stage_fdr USING brin (start_time)
; 
COMMENT ON TABLE rrd_stage_fdr IS
  'Round Robin DB Staging Table for Flow Description Records'
;
COMMENT ON COLUMN rrd_stage_fdr.source IS
  'tag that segegrates root, sync/remote or schema'
;

END;
