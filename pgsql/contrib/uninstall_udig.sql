\set ON_ERROR_STOP
SET search_path = public;

DROP OPERATOR FAMILY IF EXISTS udig_clan USING btree cascade;
DROP TYPE IF EXISTS udig_sha cascade;
DROP TYPE IF EXISTS udig cascade;
