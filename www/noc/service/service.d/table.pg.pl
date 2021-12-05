#
#  Synopsis:
#	Put html <table> comparing stats in <= 3 databases for "blobio" schema.
#  Usage:
#	/cgi-bin/service?out=table.pg&srv=jmsdesk-ess
#	/cgi-bin/service?out=table.pg&srv=jmsdesk-ess,jmsdesk-wwuptown
#

use Data::Dumper;

require 'dbi-pg.pl';
require 'httpd2.d/common.pl';
require 'service.d/probe-port.pl';

my @stat2title = (
	'system_identifier',		'System Identifier',

	'database_size_english',	'Database Size',
	'database_size_bytes',		'Database Size Bytes',

	'blob_count_10sec',		'10 Second Blob Count',
	'blob_size_10sec',		'10 Second Blobs Size',
	'blob_size_english_10sec',	'10 Second Blobs Bytes',

	'blob_count_1min',		'1 Minute Blob Count',
	'blob_size_1min',		'1 Minute Blobs Size',
	'blob_size_english_1min',	'1 Minute Blobs Bytes',

	'blob_count_15min',		'15 Minute Blob Count',
	'blob_size_15min',		'15 Minute Blobs Size',
	'blob_size_english_15min',	'15 Minute Blobs Bytes',

	'blob_count_1hr',		'1 Hour Blob Count',
	'blob_size_1hr',		'1 Hour Blobs Size',
	'blob_size_english_1hr',	'1 Hour Blobs Bytes',

	'blob_count_3hr',		'3 Hour Blob Count',
	'blob_size_3hr',		'3 Hour Blobs Size',
	'blob_size_english_3hr',	'3 Hour Blobs Bytes',

	'blob_count_6hr',		'6 Hour Blob Count',
	'blob_size_6hr',		'6 Hour Blobs Size',
	'blob_size_english_6hr',	'6 Hour Blobs Bytes',

	'blob_count_12hr',		'12 Hour Blob Count',
	'blob_size_12hr',		'12 Hour Blobs Size',
	'blob_size_english_12hr',	'12 Hour Blobs Bytes',

	'blob_count_24hr',		'24 Hour Blob Count',
	'blob_size_24hr',		'24 Hour Blobs Size',
	'blob_size_english_24hr',	'24 Hour Blobs Bytes',

	'blob_count_72hr',		'72 Hour Blob Count',
	'blob_size_72hr',		'72 Hour Blobs Size',
	'blob_size_english_72hr',	'72 Hour Blobs Bytes',
);

STDOUT->autoflush(1);

our %QUERY_ARG;

my $srv = $QUERY_ARG{srv};
my ($srv1, $srv2, $srv3) = split(',', $srv);
my %pg_service;

my $service_count = 0;
$service_count++ if $srv1;
$service_count++ if $srv2;
$service_count++ if $srv3;
my $service_plural = 's';
$service_plural = '' if $service_count == 1;

print <<END;
<table
  $QUERY_ARG{id_att}
  $QUERY_ARG{class_att}
>
 <thead>
  <caption>$service_count Database$service_plural Requested</caption>
  <tr>
    <th>Statistic</th>
END


#  fetch from the "blobnoc" schema the details for up to three postgresql
#  databases bounded to up to three www services.

my $db = dbi_pg_connect();
my $q = dbi_pg_select(
	db =>	$db,
	tag =>	'select-blobnoc-service-pg',
	argv =>	[
		$ENV{BLOBIO_NOC_LOGIN},
		$srv1,
		$srv2,
		$srv3
	],
	#pgdump => 1,
	sql =>	q(
SELECT
	service_tag,
	service_title,
	pghost,
	pgport,
	pguser,
	pgdatabase
  FROM
  	blobnoc.www_service
  WHERE
  	login_id = $1
	AND
	service_tag IN ($2, $3, $4)
  ORDER BY
  	service_tag ASC
;
));

my $r;
while ($r = $q->fetchrow_hashref()) {
	$pg_service{$r->{service_tag}} = $r;
}
$q->finish();
dbi_pg_disconnect($db);

# Determine PostgreSQL state: Up, Down, Unknown
sub pg_status
{
	my $s = $_[0];

	$r = $pg_service{$s};
	return 'Unknown' unless $r && $pg_service{$s};

	#  connect() to the db, returning 'No Answer' if open fails.

	eval
	{
		$r->{db} = dbi_pg_connect(
			PGHOST =>	$r->{pghost},
			PGPORT =>	$r->{pgport},
			PGUSER =>	$r->{pguser},
			PGDATABASE =>	$r->{pgdatabase},
		);
	} or return 'Down';


	#  service is up, so fetch stats for schema "blobio".
	#  Note: need to catch error in an eval{}!
	my $q = dbi_pg_select(
		db =>	$r->{db},
		tag =>	'blobnoc-select-stats',
		argv =>	[
				$r->{pgdatabase},
			],
		sql =>	q(
WITH blob_72hr AS (
  SELECT
  	srv.blob,
	sz.byte_count,
	srv.discover_time
  FROM
  	blobio.service srv
	  LEFT OUTER JOIN blobio.brr_blob_size sz ON (
	  	sz.blob = srv.blob
	  )
  WHERE
  	srv.discover_time >= now() + '-72 hours'
), service_10sec AS (
  SELECT
	--  Look back 10sec at blob history
	count(b.*) AS blob_count_10sec,
	sum(b.byte_count) AS blob_size_10sec
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-10 sec'
), service_1min AS (
  SELECT
	--  Look back 1min at blob history
	count(b.*) AS blob_count_1min,
	sum(b.byte_count) AS blob_size_1min
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-1 minute'
), service_15min AS (
  SELECT
	--  Look back 15min at blob history
	count(b.*) AS blob_count_15min,
	sum(b.byte_count) AS blob_size_15min
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-15 minute'
), service_1hr AS (
  SELECT
	--  Look back 1hr at blob history
	count(b.*) AS blob_count_1hr,
	sum(b.byte_count) AS blob_size_1hr
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-1 hour'
), service_3hr AS (
  SELECT
	--  Look back 3hr at blob history
	count(b.*) AS blob_count_3hr,
	sum(b.byte_count) AS blob_size_3hr
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-3 hour'
), service_6hr AS (
  SELECT
	--  Look back 6hr at blob history
	count(b.*) AS blob_count_6hr,
	sum(b.byte_count) AS blob_size_6hr
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-6 hour'
), service_12hr AS (
  SELECT
	--  Look back 12hr at blob history
	count(b.*) AS blob_count_12hr,
	sum(b.byte_count) AS blob_size_12hr
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-12 hour'
), service_24hr AS (
  SELECT
	--  Look back 24hr at blob history
	count(b.*) AS blob_count_24hr,
	sum(b.byte_count) AS blob_size_24hr
  FROM
  	blob_72hr b
  WHERE
  	b.discover_time >= now() + '-24 hour'
), service_72hr AS (
  SELECT
	--  Look back 72hr at blob history
	count(b.*) AS blob_count_72hr,
	sum(b.byte_count) AS blob_size_72hr
  FROM
  	blob_72hr b
)
SELECT
	ctl.system_identifier,
	pg_database_size($1)
	  AS database_size_bytes,
	pg_size_pretty(pg_database_size($1))
	  AS database_size_english,

	--  most recent 10secs
	COALESCE(s10s.blob_count_10sec, 0)
	  AS blob_count_10sec,
	COALESCE(s10s.blob_size_10sec, 0)
	  AS blob_size_10sec,
	pg_size_pretty(COALESCE(s10s.blob_size_10sec, 0))
	  AS blob_size_english_10sec,

	--  most recent 1 minute
	COALESCE(s1m.blob_count_1min, 0)
	  AS blob_count_1min,
	COALESCE(s1m.blob_size_1min, 0)
	  AS blob_size_1min,
	pg_size_pretty(COALESCE(s1m.blob_size_1min, 0))
	  AS blob_size_english_1min,

	--  most recent 15 minutes
	COALESCE(s15m.blob_count_15min, 0)
	  AS blob_count_15min,
	COALESCE(s15m.blob_size_15min, 0)
	  AS blob_size_15min,
	pg_size_pretty(COALESCE(s15m.blob_size_15min, 0))
	  AS blob_size_english_15min,

	--  most recent 1 hour
	COALESCE(s1h.blob_count_1hr, 0)
	  AS blob_count_1hr,
	COALESCE(s1h.blob_size_1hr, 0)
	  AS blob_size_1hr,
	pg_size_pretty(COALESCE(s1h.blob_size_1hr, 0))
	  AS blob_size_english_1hr,

	--  most recent 3 hours
	COALESCE(s3h.blob_count_3hr, 0)
	  AS blob_count_3hr,
	COALESCE(s3h.blob_size_3hr, 0)
	  AS blob_size_3hr,
	pg_size_pretty(COALESCE(s3h.blob_size_3hr, 0))
	  AS blob_size_english_3hr,

	--  most recent 6 hours
	COALESCE(s6h.blob_count_6hr, 0)
	  AS blob_count_6hr,
	COALESCE(s6h.blob_size_6hr, 0)
	  AS blob_size_6hr,
	pg_size_pretty(COALESCE(s6h.blob_size_6hr, 0))
	  AS blob_size_english_6hr,

	--  most recent 12 hours
	COALESCE(s12h.blob_count_12hr, 0)
	  AS blob_count_12hr,
	COALESCE(s12h.blob_size_12hr, 0)
	  AS blob_size_12hr,
	pg_size_pretty(COALESCE(s12h.blob_size_12hr, 0))
	  AS blob_size_english_12hr,

	--  most recent 24 hours
	COALESCE(s24h.blob_count_24hr, 0)
	  AS blob_count_24hr,
	COALESCE(s24h.blob_size_24hr, 0)
	  AS blob_size_24hr,
	pg_size_pretty(COALESCE(s24h.blob_size_24hr, 0))
	  AS blob_size_english_24hr,

	--  most recent 72 hours
	COALESCE(s72h.blob_count_72hr, 0)
	  AS blob_count_72hr,
	COALESCE(s72h.blob_size_72hr, 0)
	  AS blob_size_72hr,
	pg_size_pretty(COALESCE(s72h.blob_size_72hr, 0))
	  AS blob_size_english_72hr
    FROM
	pg_control_system() ctl,
	service_10sec s10s,
	service_1min s1m,
	service_15min s15m,
	service_1hr s1h,
	service_3hr s3h,
	service_6hr s6h,
	service_12hr s12h,
	service_24hr s24h,
	service_72hr s72h
;));
	$r->{stats} = $q->fetchrow_hashref();
	return 'Up'; 
}

#  check status of each database
$pg_service{$srv1}->{status} = pg_status($srv1);
$pg_service{$srv2}->{status} = pg_status($srv2) if $service_count > 1;
$pg_service{$srv3}->{status} = pg_status($srv3) if $service_count > 2;

sub put_th
{
	my $s = $_[0];

	print '   <th>';
	my $r = $pg_service{$s};
	if ($r->{service_title}) {
		print encode_html_entities($r->{service_title});
	} else {
		print encode_html_entities($s);
	}
	print '</th>', "\n";
}

sub put_col
{
	#  service index: 1, 2, 3
	my ($si, $col) = @_;
	return unless $service_count > $si;	#  service not requested

	my $s = "srv$i";
	my $r = $pg_service{$s};

}

put_th $srv1;
put_th $srv2 if $service_count > 1;
put_th $srv3 if $service_count > 2;

my $status = $pg_service{$srv1}->{status};
print <<END;
  </tr>
 </thead>
 <tbody>
  <tr>
   <th>Status</th>
   <td><span class="$status">$status</span></td>
END

#  finish the db status for next two columns

$status = $pg_service{$srv2}->{status};
print <<END if $service_count > 1;
   <td><span class="$status">$status</td>
END

$status = $pg_service{$srv3}->{status};
print <<END if $service_count > 2;
   <td><span class="$status">$status</td>
END

print <<END;
  </tr>
END

#  write the remander of the rows of stats

for (my $i = 0;  $i < @stat2title;  $i += 2) {

	my $v = $stat2title[$i];
	#  the particular value
	print <<END;
  <tr>
   <th>$stat2title[$i+1]</th>
END

	print '<td>', $pg_service{$srv1}->{stats}->{$v}, '</td>', "\n";
	print '<td>', $pg_service{$srv2}->{stats}->{$v}, '</td>', "\n"
		if $service_count > 1
	;
	print '<td>', $pg_service{$srv3}->{stats}->{$v}, '</td>', "\n"
		if $service_count > 2
	;
	print <<END;
   </tr>
END
}

print <<END;
 </tbody>
</table>
END

1;
