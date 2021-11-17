/*
 *  Synopsis:
 *	SQL Schema for Public Network Operations of BlobIO System.
 */

\set ON_ERROR_STOP 1
\set search_path to blobnoc,public;

BEGIN;
DROP SCHEMA IF EXISTS blobnoc CASCADE;
CREATE SCHEMA blobnoc;
COMMENT ON SCHEMA blobnoc IS 'Public BlobIO Network Operations';

DROP DOMAIN IF EXISTS noctime CASCADE;
CREATE DOMAIN noctime AS timestamptz
  CHECK (
  	value BETWEEN '2021/01/01' AND '2121/01/01'
  ) NOT NULL
;

DROP DOMAIN IF EXISTS noctag CASCADE;
CREATE DOMAIN noctag AS text
  CHECK (
  	value ~ '^[a-z][a-z0-9_-]{0,63}'
  ) NOT NULL
;

DROP TABLE IF EXISTS www_secret CASCADE;
CREATE TABLE www_secret (
	secret_blob	udig
				PRIMARY KEY,
	create_time	noctime
			DEFAULT now()
);
COMMENT ON TABLE www_secret IS 'Public WWW Login Secret';

DROP TABLE IF EXISTS www_secret_recent CASCADE;
CREATE TABLE www_secret_recent (
	secret_blob	udig
				REFERENCES www_secret(secret_blob)
				ON DELETE CASCADE,
	recent_time	noctime
);
COMMENT ON TABLE www_secret_recent IS 'Recent Read Access of Public WWW Secret';

DROP TABLE IF EXISTS www_state CASCADE;
CREATE TABLE www_state (
	secret_blob	udig
				REFERENCES www_secret(secret_blob)
				ON DELETE CASCADE,
	key		noctag,	
	value		jsonb NOT NULL,

	PRIMARY KEY	(secret_blob, key)
);
COMMENT ON TABLE www_state
  IS 'Keyed States Associated with a WWW Login Session'
;

/*
 *  Note:
 *	Need to add rule to insure rolname exists on PG database.
 *	Really wish PG allowed foreign key to pg_catalog.
 */
DROP TABLE IF EXISTS www_login CASCADE;
CREATE TABLE www_login (
	rolname	pg_catalog.name
			PRIMARY KEY
);
COMMENT ON TABLE www_login IS 'Users for noc.blob.io';

DROP TABLE IF EXISTS www_pgdatabase CASCADE;
CREATE TABLE www_service (
	rolname	pg_catalog.name
			REFERENCES www_login(rolname),
	service_tag	noctag,

	pghost		inet NOT NULL,
	pgport		int CHECK (
				pgport > 0
				AND
				pgport < 65536
			) NOT NULL,
	pguser		name NOT NULL,
	pgpasswd	text,
	pgdatabase	name,

	blobio_service	text CHECK
				blobio_service ~ '^
			)
);

COMMIT;
