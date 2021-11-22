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
my $service_tag = $POST_VAR{srvtag};

my $now = `RFC3339Nano`;
chomp $now;
die "RFC3339Nano failed: $!" unless $now =~ '^2.*T.*[0-9]$';

my $env = env2json(1);

my $request_blob = utf82blob(<<END);
{
	"noc.blob.io": {
		"blobnoc.www_service": {
			"delete": {
				"login_id":		"$BLOBIO_NOC_LOGIN",
				"service_tag":		"$service_tag"
			}
		}
	},
	"now": "$now",
	"cgi-bin-environment": $env
}
END

print STDERR "service/post.del: request blob: $request_blob";

#
#  Note:
#	Eventually, needs to be handled by flowd form blobnoc space
#
my $q = dbi_pg_exec(
	db =>	dbi_pg_connect(),
	tag =>	'service-del-insert',
	argv => [
			$BLOBIO_NOC_LOGIN,
			$service_tag,
		],
	sql =>	q(
DELETE FROM blobnoc.www_service
  WHERE
  	login_id = $1
	AND
	service_tag = $2
;
));

print <<END;
Status: 303
Location: /service

END

1;
