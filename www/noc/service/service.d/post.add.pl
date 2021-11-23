#
#  Synopsis:
#	Add a new noc service top sql table "www_service".
#  Note:
#	Need to prefix JMSCOTT_ROOT requires with
#
#		require 'jmscott/common-json.pl';
#
#	instead of pulling to noc's namespace!
#
#		require 'common-json.pl';
#

require 'dbi-pg.pl';
require 'common-json.pl';
require 'utf82blob.pl';

our %POST_VAR;

my $BLOBIO_NOC_LOGIN = $ENV{BLOBIO_NOC_LOGIN};
my $service_tag = $POST_VAR{srv};
my $PGHOST = $POST_VAR{pghost};
my $PGPORT = $POST_VAR{pgport};
my $PGUSER = $POST_VAR{pguser};
my $PGDATABASE = $POST_VAR{pgdatabase};
my $BLOBIO_SERVICE = $POST_VAR{blobsrv};
my $rrd_host = $POST_VAR{rrdhost};
my $rrd_port = $POST_VAR{rrdport};

my $now = `RFC3339Nano`;
chomp $now;
die "RFC3339Nano failed: $!" unless $now =~ '^2.*T.*[0-9]$';

my $env = env2json(1);

my $request_blob = utf82blob(<<END);
{
	"noc.blob.io": {
		"blobnoc.www_service": {
			"insert": {
				"login_id":		"$BLOBIO_NOC_LOGIN",
				"service_tag":		"$service_tag",
				"PGHOST":  		"$PGHOST",
				"PGPORT":  		$PGPORT,
				"PGUSER":  		"$PGUSER",
				"PGDATABASE":		"$PGDATABASE",
				"BLOBIO_SERVICE":	"$BLOBIO_SERVICE",
				"rrd_host":		"$rrd_host",
				"rrd_port":		$rrd_port,
				"insert_time":		"$now"
			}
		}
	},
	"now": "$now",
	"cgi-bin-environment": $env
}
END

print STDERR "service/post.add: request blob: $request_blob";

#
#  Note:
#	Eventually, needs to be handled by flowd form blobnoc space
#
my $q = dbi_pg_exec(
	db =>	dbi_pg_connect(),
	tag =>	'service-add-insert',
	argv => [
			$BLOBIO_NOC_LOGIN,
			$service_tag,
			$PGHOST,
			$PGPORT,
			$PGUSER,
			$PGDATABASE,
			$BLOBIO_SERVICE,
			$rrd_host,
			$rrd_port,
			$now
		],
	sql =>	q(
INSERT INTO blobnoc.www_service(
	login_id,
	service_tag,
	pghost,
	pgport,
	pguser,
	pgdatabase,
	blobio_service,
	rrd_host,
	rrd_port,
	insert_time
) VALUES (
	$1,
	$2,
	$3,
	$4,
	$5,
	$6,
	$7,
	$8,
	$9,
	$10
);
));

print <<END;
Status: 303
Location: /service

END

1;
