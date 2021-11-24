#
#  Synopsis:
#	Write html <table> of stats for tables in "blobio" schema.
#  Usage:
#	/cgi-bin/service?out=table.pg
#

use Data::Dumper;

require 'dbi-pg.pl';
require 'httpd2.d/common.pl';
require 'service.d/probe-port.pl';

STDOUT->autoflush(1);

our %QUERY_ARG;

my $srv = $QUERY_ARG{srv};

print <<END;
<table
  $QUERY_ARG{id_att}
  $QUERY_ARG{class_att}
>
 <thead>
  <caption>
END

#  fetch the tuple from table www_service
my $db = dbi_pg_connect();
my $q = dbi_pg_select(
	db =>	$db,
	tag =>	'select-blobnoc-service-pg',
	argv =>	[
		$ENV{BLOBIO_NOC_LOGIN},
		$srv
	],
	#pgdump => 1,
	sql =>	q(
SELECT
	pghost,
	pgport,
	pguser,
	pgdatabase
  FROM
  	blobnoc.www_service
  WHERE
  	login_id = $1
	AND
	service_tag = $2
;
));
my (
	$PGHOST,
	$PGPORT,
	$PGUSER,
	$PGDATABASE,
) = $q->fetchrow();

unless ($PGDATABASE) {
	print <<END;
   <span class="err">Can not fetch PG vars for service "$srv"</span>
  </caption>
 </thead>
</table>
END
	return 1;
}

$q->finish();

dbi_pg_disconnect($db);

$db = dbi_pg_connect(
	PGHOST =>	$PGHOST,
	PGPORT =>	$PGPORT,
	PGUSER =>	$PGUSER,
	PGDATABASE =>	$PGDATABASE,
);

$q = dbi_pg_select(
	db =>	$db,
	tag =>	'select-blobnoc-service-pg-stat',
	argv =>	[
			$PGDATABASE,
			$$PGDATABASE,	# Note: zap me!
		],
	sql => q(
SELECT
	pg_size_pretty(pg_database_size($1))
		AS database_size_english,
	pg_database_size($2)
		AS database_size_bytes,

	--  previous second of blob traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 second')
		AS blob_count_sec,
	sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 second'
	) AS blob_size_sec,
	pg_size_pretty(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 second'
	)) AS blob_size_english_sec,

	--  previous minute of blob traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 minute')
		AS "blob_count_min",
	sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 minute'
	) AS blob_size_min,
	pg_size_pretty(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 minute'
	)) AS blob_size_english_min,

	--  previous hour of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 hour')
		AS "blob_count_hr",
	sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 hour'
	) AS blob_size_hr,
	pg_size_pretty(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 hour'
	)) AS blob_size_english_hr,

	--  previous 24 hours of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-24 hour')
		AS "blob_count_24hr",
	sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-24 hours'
	) AS blob_size_24hr,
	pg_size_pretty(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-24 hours'
	)) AS blob_size_english_24hr,

	--  previous 72 hours of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-72 hours')
		AS "blob_count_72hr",
	sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-72 hours'
	) AS blob_size_72hr,
	pg_size_pretty(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-72 hours'
	)) AS blob_size_english_72hr
  FROM
  	blobio.service srv
	  LEFT OUTER JOIN blobio.brr_blob_size sz ON (
	  	sz.blob = srv.blob
	  )
  WHERE
  	srv.discover_time >= now() + '-72 hours'
;
));
my $pg_title = "$PGUSER\@$PGDATABASE\@$PGHOST:$PGPORT";

my (
	$database_size_english,
	$database_size_bytes,

	$blob_count_sec,
	$blob_size_sec,
	$blob_size_english_sec,

	$blob_count_min,
	$blob_size_min,
	$blob_size_english_min,

	$blob_count_hr,
	$blob_size_hr,
	$blob_size_english_hr,

	$blob_count_24hr,
	$blob_size_24hr,
	$blob_size_english_24hr,

	$blob_count_72hr,
	$blob_size_72hr,
	$blob_size_english_72hr,
) = $q->fetchrow();

unless ($database_size_english) {
	print <<END;
   <span class="err">
     Can not fetch size of database $PGDATABASE
   </span>
  </caption>
 </thead>
</table>
END
	return 1;
}

#  Note: reset for null sums: ought to be in sql case expression

($blob_size_sec, $blob_size_english_sec) = ('0', '0') unless $blob_size_sec > 0;
($blob_size_min, $blob_size_english_min) = ('0', '0') unless $blob_size_min > 0;
($blob_size_hr, $blob_size_english_hr) = ('0', '0') unless $blob_size_hr > 0;
($blob_size_24hr, $blob_size_english_24hr) = ('0', '0')
	unless $blob_size_24hr > 0
;
($blob_size_72hr, $blob_size_english_72hr) = ('0', '0')
	unless $blob_size_72hr > 0
;

print <<END;
   <h1>$pg_title</h1>
  </caption>
 </thead>
 <tbody>
  <tr>
   <th>Statistic</th>
   <th>Value</th>
  </tr>

  <tr>
   <td>Database Size English</td>
   <td>$database_size_english</td>
  </tr>

  <tr>
   <td>Database Size Bytes</td>
   <td>$database_size_bytes bytes</td>
  </tr>

  <tr>
   <td>1 Second Blob Count</td>
   <td>$blob_count_sec blobs</td>
  </tr>
  <tr>
   <td>1 Second Blob Size</td>
   <td>$blob_size_english_sec ($blob_size_sec bytes)</td>
  </tr>

  <tr>
   <td>1 Minute Blob Count</td>
   <td>$blob_count_min blobs</td>
  </tr>
  <tr>
   <td>1 Minute Blob Size</td>
   <td>$blob_size_english_min ($blob_size_min bytes)</td>
  </tr>

  <tr>
   <td>1 Hour Blob Count</td>
   <td>$blob_count_hr blobs</td>
  </tr>
  <tr>
   <td>1 Hour Blob Size</td>
   <td>$blob_size_english_hr ($blob_size_hr bytes)</td>
  </tr>

  <tr>
   <td>24 Hour Blob Count</td>
   <td>$blob_count_24hr blobs</td>
  </tr>
  <tr>
   <td>24 Hour Blob Size</td>
   <td>$blob_size_english_24hr ($blob_size_24hr bytes)</td>
  </tr>

  <tr>
   <td>72 Hour Blob Count</td>
   <td>$blob_count_72hr blobs</td>
  </tr>
  <tr>
   <td>72 Hour Blob Size</td>
   <td>$blob_size_english_72hr ($blob_size_72hr bytes)</td>
  </tr>

 </tbody>
</table>

END

1;
