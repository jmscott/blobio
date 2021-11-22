#
#  Synopsis:
#	Write html <table> of sql table "www_service".
#  Usage:
#	/cgi-bin/service?out=table
#  Note:
#	Need to put summary footer at bottom of table!
#

use Errno;
use Fcntl;
use Socket;

require 'dbi-pg.pl';
require 'httpd2.d/common.pl';
require 'service.d/common.pl';

STDOUT->autoflush(1);

our %QUERY_ARG;

my $db = dbi_pg_connect();

my $q = dbi_pg_select(
	db =>	$db,
	tag =>	'select-blobnoc-www-service-count',
	argv =>	[
		$ENV{BLOBIO_NOC_LOGIN}
	],
	sql => q(
SELECT
	count(*)
  FROM
  	blobnoc.www_service
  WHERE
  	login_id = $1
;
));

my $service_count = $q->fetchrow();
my $plural_service;
$plural_service = 's' unless $service_count == 1;

print <<END;
<table
  $QUERY_ARG{id_att}
  $QUERY_ARG{class_att}
>
 <thead>
  <caption>
   <h1>$service_count Service$plural_service</h1>
  </caption>
  <tr>
   <th>Service Tag</th>
   <th>PostgreSQL</th>
   <th>BlobIO</th>
   <th>Round Robin Database</th>
  </tr>
 </thead>
 <tbody>
END

$q = dbi_pg_select(
	db =>	$db,
	tag =>	'select-blobnoc-www-service',
	argv =>	[
		$ENV{BLOBIO_NOC_LOGIN}
	],
	sql => q(
SELECT
	service_tag,
	pghost,
	pgport,
	pguser,
	pgdatabase,
	blobio_service,
	rrd_host,
	rrd_port
  FROM
  	blobnoc.www_service
  WHERE
  	login_id = $1
  ORDER BY
  	login_id ASC
));

while (my (
	$service_tag,
	$PGHOST,
	$PGPORT,
	$PGUSER,
	$PGDATABASE,
	$BLOBIO_SERVICE,
	$rrd_host,
	$rrd_port
	) = $q->fetchrow()) {

	my $pg_span_status = '<span class="ok">✓</span>';
	$pg_span_status = '<span class="err">✗</span>'
			unless probe_ip_port($PGHOST, $PGPORT, 1)
	;

	my $blob_span_status = '<span class="ok">✓</span>';
	if ($BLOBIO_SERVICE =~ m/^bio4:(.*):(\d{1,5})$/) {
		$blob_span_status = '<span class="err">✗</span>'
			unless probe_ip_port($1, $2, 1)
		;
	}

	my $rrd_span_status = '<span class="ok">✓</span>';
	$rrd_span_status = '<span class="err">✗</span>'
			unless probe_ip_port($rrd_host, $rrd_port, 1)
	;
print <<END;
  <tr>
   <td>$service_tag</td>
   <td>$pg_span_status $PGUSER\@$PGDATABASE/$PGHOST:$PGPORT</td>
   <td>$blob_span_status $BLOBIO_SERVICE</td>
   <td>$rrd_span_status $rrd_host:$rrd_port</td>
  </tr>
END
}

print <<END;
 </tbody>
</table>
END
