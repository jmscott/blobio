/*
 *  Synopsis:
 *	Generate a provable bug in v14.1: XX000: cache lookup failed for type 0
 */
set search_path to blobio,public;
\set VERBOSITY verbose

CREATE OR REPLACE FUNCTION bug_cast_jsonb_brr(doc jsonb)
  RETURNS brr
  AS $$
  DECLARE
  	r brr;
  BEGIN
	IF doc->>'verb' = 'get' then 
		SELECT INTO r ROW(
			(doc->>'start_time')::brr_duration,
			(doc->>'transport'),
			doc->>'verb',
			doc->>'blob',
			doc->>'chat_history',
			(doc->>'blob_size')::ui63,
			(doc->>'wall_duration')::brr_duration
		)::blobio.brr;
	END IF;
	RETURN r;
	END
  $$
  LANGUAGE plpgsql
;
COMMENT ON FUNCTION cast_jsonb_brr(jsonb) IS
  'Demonstrate bug in PG14 type cache'
;

select bug_cast_jsonb_brr(null);
