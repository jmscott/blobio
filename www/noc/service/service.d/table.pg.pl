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
			$PGDATABASE,
		],
	sql => q(
SELECT
	pg_size_pretty(pg_database_size($1))
		AS database_size_english,
	pg_database_size($2)
		AS database_size_bytes
;
));
my $pg_title = "$PGUSER\@$PGDATABASE\@$PGHOST:$PGPORT";

my (
	$database_size_english,
	$database_size_bytes,
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

print <<END;
   <h1>$pg_title</h1>
   <h2>$database_size_english ($database_size_bytes bytes)</h2>
  </caption>
 </thead>
</table>
END

1;
