/*
 *  Synopsis:
 *	Select biod rrd success counts group by day, since a certain 
 *  Command Line Variables:
 *	since	text
 */
SELECT
	date_part('year', sample_time) as "Year",
	date_part('month', sample_time) as "Month",
	date_part('day', sample_time) as "Day",
	sum(success_count)
  FROM
  	blobio.biod_request_stat
  WHERE
  	sample_time >= now() + :since::interval
  GROUP BY
  	1, 2, 3
  ORDER BY
  	1, 2, 3
;
