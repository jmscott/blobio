#
#  Synopsis:
#	Write an html <table> of all rrd request samples since a certain time.
#

require 'dbi.pl';
require 'blobio.d/common.pl';

our %QUERY_ARG;

print <<END;
<table
  name="rrdreq"
  $QUERY_ARG{id_att}
  $QUERY_ARG{class_att}
>
END

my $sec = recent2sec($QUERY_ARG{since});

my $q = dbi_select(
	sql => <<END
SELECT
	*
  FROM
  	blobio.biod_request_stat
  WHERE
  	sample_time >= now() + '-$sec seconds'
  ORDER BY
  	sample_time DESC
;
END
);

print <<END;
</table>
END
