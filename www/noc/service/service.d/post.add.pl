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

require 'common-json.pl';
require 'utf82blob.pl';

our %POST_VAR;

my $srvtag = $POST_VAR{srvtag};
my $pghost = $POST_VAR{pghost};
my $pgport = $POST_VAR{pgport};
my $pguser = $POST_VAR{pguser};
my $pgdatabase = $POST_VAR{pgdatabase};
my $rrdhost = $POST_VAR{rrdhost};
my $rrdport = $POST_VAR{rrdport};

my $now = `RFC3339Nano`;
chomp $now;
die "RFC3339Nano failed: $!" unless $now =~ '^2.*T.*[0-9]$';

my $env = env2json(1);

my $request_blob = utf82blob(<<END);
{
	"noc.blob.io": {
		"srvtag":  "$srvtag",
		"pghost":  "$pghost",
		"pgport":  $pgport,
		"pguser":  "$pguser",
		"pgdatabase":  "$pgdatabase",
		"rrdhost":  "$rrdhost",
		"rrdport":  $rrdport
	},
	"now": "$now",
	"cgi-bin-environment": $env
}
END

print STDERR "service/post.add: request blob: $request_blob";

print <<END;
Status: 303
Location: $ENV{HTTP_REFERER}

END
