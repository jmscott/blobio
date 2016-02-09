/*
 *  Synopsis:
 *	PostgreSQL schema to track blobs available in a particular biod server.
 *  Note:
 *	Please add sql comments.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
\set ON_ERROR_STOP

begin;
drop schema if exists blobio cascade;
create schema blobio;
comment on schema blobio is
  'blobs in service and brr history'
;

set search_path to blobio,public;

drop domain if exists blobio.brr_duration cascade;
create domain brr_duration as interval
  check (
	value >= '0 seconds'
  )
  not null
;
comment on domain brr_duration is
  'wall clock duration of a blob request'
;

drop domain if exists blobio.brr_timestamp cascade;
create domain brr_timestamp as timestamptz
  check (
	value >= '2008-05-17 10:06:42'
  )
  not null
;
comment on domain brr_timestamp is
  'starting time of request for a blob'
;

/*
 *  Most recently seen successfull take from this service.
 *  The connection chat history was ok,ok,ok, hence ok3.
 */
drop table if exists blobio.brr_take_ok3_recent cascade;
create table brr_take_ok3_recent
(
	blob			public.udig
					primary key,
	start_time		brr_timestamp,

	/*
	 *  Since the physical remove of the blob only occurs after the client
	 *  has the blob, a better estimate of when the blob was removed from
	 *  service is start_time + wall_duration.
	 */
	wall_duration		brr_duration
);
comment on table brr_take_ok3_recent is
  'most recently seen take request record for a particular blob'
;
create index brr_take_ok3_recent_start_time on brr_take_ok3_recent(start_time);

/*
 *  The blob size as observed in a brr record of an existing blob.
 *  Blob size should NEVER change.
 */
drop table if exists blobio.brr_blob_size cascade;
create table brr_blob_size
(
	blob		udig
				primary key,
	byte_count	bigint
				check (
					byte_count >= 0
				)
				not null
);
comment on table brr_blob_size is
  'number bytes (octets) in the blob'
;
revoke update on brr_blob_size from public;

/*
 *  A recently verified existence of a blob as seen in a brr record.
 */
drop table if exists blobio.brr_ok_recent cascade;
create table brr_ok_recent
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
comment on table brr_ok_recent is
  'most recently verified existence for a particular blob'
;
create index brr_ok_recent_start_time on brr_ok_recent(start_time);

/*
 *  A recently failed read of a blob, which implies the blob may not exist.
 *  Don't record successfull "take"s, since
 *  has been completely forgotten.
 */
drop table if exists blobio.brr_no_recent cascade;
create table brr_no_recent
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration
);
comment on table brr_no_recent is
  'most recently failed attempt to get or eat the blob'
;
create index brr_no_recent_start_time on brr_no_recent(start_time);

/*
 *  Earliest known existence of a blob.  Both the brr time
 *  and the insert/update of the record are recorded.
 */
drop table if exists blobio.brr_discover;
create table brr_discover
(
	blob		udig
				primary key,
	/*
	 *  Start time in blob request record.
	 */
	start_time	brr_timestamp
				not null,
	/*
	 *  Time this database record was inserted or updated with
	 *  an earlier time,  effective measuring discover latency.
	 */
	upsert_time	brr_timestamp
				default now()
				not null
);
create index brr_discover_start_time on brr_discover(start_time);
comment on table brr_discover
  is
  	'the earliest known existence of a digestable blob for this service'
;

comment on column brr_discover.start_time
  is
  	'start time of the earliest request for the discovered blob'
;

comment on column brr_discover.upsert_time
  is
  	'time of sql insert or update of this tuple'
;

drop table if exists blobio.brr_wrap_ok;
create table brr_wrap_ok
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration,
	insert_time	timestamptz
				default now()
				not null
);
revoke update on brr_wrap_ok from public;
comment on table brr_wrap_ok
  is
  	'history of successfull wrap requests'
;


drop table if exists blobio.brr_roll_ok;
create table brr_roll_ok
(
	blob		udig
				primary key,
	start_time	brr_timestamp,
	wall_duration	brr_duration,
	insert_time	timestamptz
				default now()
				not null
);
revoke update on brr_roll_ok from public;
comment on table brr_roll_ok
  is
  	'history of successfull roll requests'
;

/*
 *  Fetchable blobs with regard to the blobio network service.
 *  In other words, for, say, service bio4:localhost:1797,
 *  if <udig> is in the table service.blob then
 *
 *	blobio get --udig <udig> --service bio4:localhost:1797
 *
 *  fetches a living blob.
 */
drop view if exists blobio.service cascade;
create view service as
  select
	ok.blob,
	ok.start_time as "recent_time",
	d.start_time as "discover_time"
    from
  	brr_ok_recent ok
	  left outer join brr_take_ok3_recent take on
	  (
	  	take.blob = ok.blob
		and
		take.start_time >= ok.start_time
	  )
	  left outer join brr_no_recent no on
	  (
	  	no.blob = ok.blob
		and
		no.start_time >= ok.start_time
	  )
	  inner join brr_discover d on (d.blob = ok.blob)
    where
	take.blob is null
	and
	no.blob is null
;
comment on view service
  is
  	'all verified blobs managed by the server'
;

/*
 *  Blobs taken from the service and, therefore, not in service
 */
drop view if exists blobio.taken cascade;
create view taken as
  select
	take.blob,
	take.start_time as "recent_time"
    from
  	brr_take_ok3_recent take
	  left outer join brr_ok_recent ok on
	  (
	  	ok.blob = take.blob
		and
		ok.start_time >= take.start_time
	  )
    where
	ok.blob is null
;
comment on view taken
  is
  	'blobs taken from the service and, therefore, not in service'
;

/*
 *  View of once discovered blobs that are neither in service nor taken
 *  By design, the 'missing' set should always be empty.
 */
drop view if exists blobio.missing cascade;
create view missing as
  select
  	dis.blob,
	dis.start_time as "discover_time"
    from
    	brr_discover dis
	  left outer join service srv on (srv.blob = dis.blob)
	  left outer join taken take on (take.blob = dis.blob)
    where
    	srv.blob is null
	and
	take.blob is null
;
comment on view missing
  is
  	'once discovered blobs that are neither in service nor taken'
;
/*
 *  View of udigs for which no associated blob has been digested.
 */
drop view if exists blobio.quack cascade;
create view quack as
  select
  	no.blob,
	no.start_time as "discover_time"
    from
    	brr_no_recent no
	  left outer join service srv on (srv.blob = no.blob)
	  left outer join taken take on (take.blob = no.blob)
	  left outer join missing mis on (mis.blob = no.blob)
    where
    	srv.blob is null
	and
	take.blob is null
	and
	mis.blob is null
;
comment on view quack
  is
  	'blobs which have never been digested'
;

commit;
