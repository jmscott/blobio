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
	'system_identifier',  'System Identifier',

	'database_size_english',  'Database Size',
	'database_size_bytes',  'Database Size Bytes',

	'blob_count_sec',  '1 Second Blob Count',
	'blob_size_sec',  '1 Second Blobs Size',
	'blob_size_english_sec',  '1 Second Blobs Bytes',

	'blob_count_min',  '1 Minute Blob Count',
	'blob_size_min',  '1 Minute Blobs Size',
	'blob_size_english_min',  '1 Minute Blobs Bytes',

	'blob_count_hr',  '1 Hour Blob Count',
	'blob_size_hr',  '1 Hour Blobs Size',
	'blob_size_english_hr',  '1 Hour Blobs Bytes',

	'blob_count_24hr',  '24 Hour Blob Count',
	'blob_size_24hr',  '24 Hour Blobs Size',
	'blob_size_english_24hr',  '24 Hour Blobs Bytes',

	'blob_count_72hr',  '72 Hour Blob Count',
	'blob_size_72hr',  '72 Hour Blobs Size',
	'blob_size_english_72hr',  '72 Hour Blobs Bytes',
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
  <caption>$service_count Database Requested</caption>
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
SELECT
	ctl.system_identifier,
	pg_size_pretty(pg_database_size($1))
		AS database_size_english,
	pg_database_size($1)
		AS database_size_bytes,

	--  previous second of blob traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 second')
		AS blob_count_sec,
	coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 second'
	), 0) AS blob_size_sec,
	pg_size_pretty(coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 second'
	), 0)) AS blob_size_english_sec,

	--  previous minute of blob traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 minute')
		AS "blob_count_min",
	coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 minute'
	), 0) AS blob_size_min,
	pg_size_pretty(coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 minute'
	), 0)) AS blob_size_english_min,

	--  previous hour of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-1 hour')
		AS "blob_count_hr",
	coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 hour'
	), 0) AS blob_size_hr,
	pg_size_pretty(coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-1 hour'
	), 0)) AS blob_size_english_hr,

	--  previous 24 hours of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-24 hour')
		AS "blob_count_24hr",
	coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-24 hours'
	), 0) AS blob_size_24hr,
	pg_size_pretty(coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-24 hours'
	), 0)) AS blob_size_english_24hr,

	--  previous 72 hours of traffic
	count(*) FILTER(WHERE srv.discover_time >= now() + '-72 hours')
		AS "blob_count_72hr",
	coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-72 hours'
	), 0) AS blob_size_72hr,
	pg_size_pretty(coalesce(sum(sz.byte_count) FILTER(
		WHERE srv.discover_time >= now() + '-72 hours'
	), 0)) AS blob_size_english_72hr
  FROM
  	blobio.service srv
	  LEFT OUTER JOIN blobio.brr_blob_size sz ON (
	  	sz.blob = srv.blob
	  ),
	pg_control_system() ctl
  WHERE
  	srv.discover_time >= now() + '-72 hours'
  GROUP BY
  	system_identifier
;
));
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
