#
#  Synopsis:
#	Redirect a full text search to pdfbox/index page seeded with query.
#  Note:
#	-  Why are utf chars allowed into JSON objects.  See blob
#
#		bc160:ab52408675d0e0b3d6854871b0dc90bbdbbbdaf6		
#		bc160:5aafe3ae324412941eb19de3f95d6fe429072c06
#
#	-  Mozilla appears to be putting in non-printable chars into
#	   the json "title" field.  For an example, see this json blob:
#
#		bc160:294496c00c3a112753c08dda71f7a8cc2f0f2499
#
#	   Some kind of unicode space is at the end of
#
#		"title": "Computational Social Choice "
#
#	   Perhaps the perl REG could recognize the unicode space!
#

sub gripe
{
	print STDERR 'post.mytitle: ERROR: ', join(':', @_), "\n";
}

our %POST_VAR;

my $service_tag = $POST_VAR{}
die 'POST_VAR: blob is empty' if $blob eq '""';
my $title = $POST_VAR{title};
utf8::upgrade($title);

#  convert new line, carriage return or form feed to space
$title =~ s/[\n\r\f]+/ /g;

#  strip leading/trailing white space
$title =~ s/^[[:space:]]*|[[:space:]]*$//g;

#  compress white space
$title =~ s/[[:space:]]+/ /g;

my $error_403;
if (length($title) == 0) {
	$error_403 = 'Title is empty';
} elsif (length($title) > 255) {
	$error_403 = 'Submitted title > 255 UTF8 characters';
} elsif ($title !~ m/[[:graph:]]/) {
	$error_403 = 'No graphics characters in title';
}

if ($error_403) {
	gripe $error_403;
	print <<END;
Status: 403
Content-Type: text/plain

$error_403
END
	return 1;
}

$title = utf82json_string($title);
my $env = env2json(2);
my $request_epoch = time();

my $request_blob = utf82blob(<<END);
{
	"mycore.schema.setspace.com": {
		"title": {
			"blob": $blob,
			"title": $title
		}
	},
	"mydash.schema.setspace.com": {
		"mycore.title": {
			"blob": $blob,
			"title": $title
		}
	},
	"request-unix-epoch": $request_epoch,
	"cgi-bin-environment": $env
}
END

#  wait for json request blob to appear in request table.

my $cmd =
   "spin-wait-blob mycore.title_request request_blob $timeout $request_blob";

unless (system($cmd) == 0) {
	my $status = $?;
	gripe "system(spin-wait-blob request) failed: exit status=$status";
	gripe "spin-wait-blob request: $request_blob";
	$status = ($status >> 8) & 0xFF;
	if ($status > 1) {
		print <<END;
Status: 500
Content-Type: text/plain

ERROR: unexpected reply for json request: status=$status: $request_blob
END
	} elsif ($status == 1) {
		print <<END;
Status: 503
Content-Type: text/plain

ERROR: no json request blob in sql database after $timeout seconds: $request_blob
END
	}
	return 1;
}

#  wait for any title to appear in table mycore.title

$cmd = "spin-wait-blob mycore.title blob $timeout $blob";
unless (system($cmd) == 0) {
	my $status = $?;

	gripe "system(spin-wait-blob title) failed: exit status=$status";
	gripe "spin-wait-blob title: $blob";
	$status = ($status >> 8) & 0xFF;
	if ($status > 1) {
		print <<END;
Status: 500
Content-Type: text/plain

ERROR: unexpected reply for title existence: status=$status: $blob
END
	} elsif ($status == 1) {
		print <<END;
Status: 503
Content-Type: text/plain

ERROR: no title for blob in sql database after $timeout seconds: $blob
END
	}
	return 1;
}

print <<END;
Status: 303
Location: $ENV{HTTP_REFERER}

END
