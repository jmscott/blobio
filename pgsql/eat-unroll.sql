/*
 *  Synopsis:
 *	Unroll all wraps and eat new blobs not in service or taken.
 */	

\set ON_ERROR_STOP
\timing on

\echo create temp table unroll_all
create temp table unroll_all
(
	blob		udig
				primary key,
	syncability	text
				default 'Raw'
);

\echo copy unroll set from file unroll-all.udig to table unroll_all
\copy unroll_all(blob) from 'unroll-all.udig'

\echo vacuum analyze fresh unroll_all
vacuum analyze unroll_all;

\echo tagging In Service blobs
update unroll_all un
  set
  	syncability = 'In Service'
  where
  	exists (
	  select
	  	s.blob
	    from
	    	blobio.service s
	    where
	    	s.blob = un.blob
	)
	and
	un.syncability = 'Raw'
;
\echo revacuum analyze unroll_all after tagging In Service udigs
vacuum analyze unroll_all;

\echo tagging Taken blobs
update unroll_all un
  set
  	syncability = 'Taken'
  where
  	exists (
	  select
	  	t.blob
	    from
	    	blobio.taken t
	    where
	    	t.blob = un.blob
	)
	and
	syncability = 'Raw'
;
\echo revacuum analyze unroll_all after tagging Taken udigs
vacuum analyze unroll_all;

\echo tagging Missing blobs
update unroll_all un
  set
  	syncability = 'Missing'
  where
  	exists (
	  select
	  	m.blob
	    from
	    	blobio.missing m
	    where
	    	m.blob = un.blob
	)
	and
	un.syncability = 'Raw'
;
\echo revacuum analyze unroll_all after tagging Missing udigs
vacuum analyze unroll_all;

\echo tagging Quack blobs
update unroll_all un
  set
  	syncability = 'Quack'
  where
  	exists (
	  select
	  	q.blob
	    from
	    	blobio.quack q
	    where
	    	q.blob = un.blob
	)
	and
	un.syncability = 'Raw'
;
\echo revacuum analyze unroll_all after tagging Quack udigs
vacuum analyze unroll_all;

\echo summarize unroll_all syncability
select
	'All',
	count(*)
  from
  	unroll_all
union all
(select
	syncability,
	count(*) as "Blob Count"
  from
  	unroll_all
  group by
  	syncability
  order by
  	"Blob Count" desc
)
;

\echo writing file raw.udig ...
\pset format unaligned
\pset fieldsep ' '
\pset recordsep '\n'
\pset tuples_only
\o raw.udig

\echo selecting udigs of all Raw blobs into file raw.udig
select
	blob
  from
  	unroll_all
  where
  	syncability = 'Raw'
;
