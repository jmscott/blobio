/*
 *  Synopsis:
 *	SQL Schema for Public Network Operations of BlobIO System.
 */

\set ON_ERROR_STOP 1
set search_path to blobnoc,public;

BEGIN;
DROP SCHEMA IF EXISTS blobnoc CASCADE;
CREATE SCHEMA blobnoc;
COMMENT ON SCHEMA blobnoc IS 'Public BlobIO Network Operations';

DROP DOMAIN IF EXISTS noc_time CASCADE;
CREATE DOMAIN noc_time AS timestamptz
  CHECK (
  	value BETWEEN '2021/01/01' AND '2121/01/01'
  ) NOT NULL
;

DROP DOMAIN IF EXISTS noc_port CASCADE;
CREATE DOMAIN noc_port AS INTEGER
  CHECK (
  	value BETWEEN 0 AND 65535
  ) NOT NULL
;

DROP DOMAIN IF EXISTS noc_tag CASCADE;
CREATE DOMAIN noc_tag AS text
  CHECK (
  	value ~ '^[a-z][a-z0-9_-]{0,63}'
  ) NOT NULL
;

DROP TABLE IF EXISTS www_secret CASCADE;
CREATE TABLE www_secret (
	secret_blob	udig
				PRIMARY KEY,
	create_time	noc_time
			DEFAULT now()
);
COMMENT ON TABLE www_secret IS 'Public WWW Login Secret';

DROP TABLE IF EXISTS www_secret_recent CASCADE;
CREATE TABLE www_secret_recent (
	secret_blob	udig
				REFERENCES www_secret(secret_blob)
				ON DELETE CASCADE,
	recent_time	noc_time
);
COMMENT ON TABLE www_secret_recent IS 'Recent Read Access of Public WWW Secret';

DROP TABLE IF EXISTS www_state CASCADE;
CREATE TABLE www_state (
	secret_blob	udig
				REFERENCES www_secret(secret_blob)
				ON DELETE CASCADE,
	key		noc_tag,	
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
	login_id	pg_catalog.name
				PRIMARY KEY
);
COMMENT ON TABLE www_login IS 'Users for noc.blob.io';
INSERT INTO www_login VALUES ('jmscott');

DROP TABLE IF EXISTS www_service CASCADE;
CREATE TABLE www_service (
	login_id	pg_catalog.name
				REFERENCES www_login(login_id),
	service_tag	noc_tag,

	/*
	 *  PostgreSQL Database with the blobio schema.
	 */
	pghost		inet NOT NULL,
	pgport		noc_port,
	pguser		name NOT NULL,
	pgpasswd	text,
	pgdatabase	name NOT NULL,

	/*
	 *  Where to fetch immutable blobs.
	 */
	blobio_service	text CHECK (
				blobio_service ~
				      '^[a-z][a-z0-9]{0,8}:[[:graph:]]{1,128}$'
			) NOT NULL,
	/*
	 *  Where to fetch the round robin database graphs.
	 */
	rrd_host	inet NOT NULL,
	rrd_port	noc_port NOT NULL
);

INSERT INTO www_service VALUES
	('jmscott', 'jmsdesk-ess',
		'10.187.1.5', 5432, 'postgres', '', 'jmsdesk',
		'bio4:10.187.1.5:1797',
		'10.187.1.5', 2324
	),
	('jmscott', 'jmsdesk-wework',
		'10.187.1.3', 5432, 'postgres', '', 'jmsdesk',
		'bio4:10.187.1.3:1797',
		'10.187.1.3', 2324
	),
	('jmscott', 'jmsdesk-lct',
		'10.187.1.2', 5432, 'postgres', '', 'jmsdesk',
		'bio4:10.187.1.2:1797',
		'10.187.1.2', 2324
	)
;

COMMIT;
