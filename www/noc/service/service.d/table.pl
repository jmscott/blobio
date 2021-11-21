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

STDOUT->autoflush(1);

sub probe_port {
        my ($name, $port, $timeout) = @_;

        my $ip;
        if(lc $name eq "localhost") {
                $ip = "127.0.0.1";  # Don't depend on a DNS for this triviality.
        } elsif($name =~ qr~[a-zA-Z]~s) {
                defined ($ip = gethostbyname($name)) or return 0;
                $ip = inet_ntoa($ip);
        } else {
                $ip = $name;
        }

        # Create socket.
        socket(my $connection, PF_INET, SOCK_STREAM, getprotobyname('tcp'))
		or return 0;

        # Set autoflushing.
        $_ = select($connection); $| = 1; select $_;

        # Set FD_CLOEXEC.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ | FD_CLOEXEC) or return 0;

        if($timeout) {
                # Set O_NONBLOCK so we can time out connect().
                $_ = fcntl($connection, F_GETFL, 0) or return 0;
                fcntl($connection, F_SETFL, $_ | O_NONBLOCK) or return 0;
        }

        # Connect returns immediately because of O_NONBLOCK.
        connect($connection, pack_sockaddr_in($port, inet_aton($ip))) or
		$!{EINPROGRESS} or return 0;

        # Reset O_NONBLOCK.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ & ~ O_NONBLOCK) or return 0;

        #  Use select() to poll for completion or error.
	#  When connect succeeds we can write.
        my $vec = "";
        vec($vec, fileno($connection), 1) = 1;
        select(undef, $vec, undef, $timeout);
        return 0 unless(vec($vec, fileno($connection), 1));

        # This is how we see whether it connected or there was an error. Document Unix, are you kidding?!
        $! = unpack("L", getsockopt($connection, SOL_SOCKET, SO_ERROR));
        return 0 if $!;

        setsockopt($connection, SOL_SOCKET, SO_SNDTIMEO, pack("L!L!",
		$timeout, 0)) or return 0;
        setsockopt($connection, SOL_SOCKET, SO_RCVTIMEO, pack("L!L!",
		$timeout, 0)) or return 0;

        return 1
}

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
			unless probe_port($PGHOST, $PGPORT, 1)
	;

	my $blob_span_status = '<span class="ok">✓</span>';
	if ($BLOBIO_SERVICE =~ m/^bio4:(.*):(\d{1,5})$/) {
		$blob_span_status = '<span class="err">✗</span>'
			unless probe_port($1, $2, 1)
		;
	}

	my $rrd_span_status = '<span class="ok">✓</span>';
	$rrd_span_status = '<span class="err">✗</span>'
			unless probe_port($rrd_host, $rrd_port, 1)
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
