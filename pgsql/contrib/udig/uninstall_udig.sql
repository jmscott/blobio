\set ON_ERROR_STOP
SET search_path = public;

DROP OPERATOR FAMILY IF EXISTS udig_clan USING btree CASCADE;
DROP TYPE IF EXISTS udig_sha CASCADE;
DROP TYPE IF EXISTS udig_bc160 CASCADE;
DROP TYPE IF EXISTS udig CASCADE;
