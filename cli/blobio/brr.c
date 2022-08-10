/*
 *  Synopsis:
 *	
static void
brr_write()
{
	struct tm *t;
	char brr[BRR_SIZE + 1];
	struct timespec	end_time;
	long int sec, nsec;
	size_t len;

	/*
	 *  Build the ascii version of the start time.
	 */
	t = gmtime(&start_time.tv_sec);
	if (!t)
		die2("gmtime() failed", strerror(errno));
	/*
	 *  Get end time.
	 */
	if (clock_gettime(CLOCK_REALTIME, &end_time) < 0)
		die2("clock_gettime(end REALTIME) failed", strerror(errno));
	/*
	 *  Calculate the elapsed time in seconds and
	 *  nano seconds.  The trick of adding 1000000000
	 *  is the same as "borrowing" a one in grade school
	 *  subtraction.
	 */
	if (end_time.tv_nsec - start_time.tv_nsec < 0) {
		sec = end_time.tv_sec - start_time.tv_sec - 1;
		nsec = 1000000000 + end_time.tv_nsec - start_time.tv_nsec;
	} else {
		sec = end_time.tv_sec - start_time.tv_sec;
		nsec = end_time.tv_nsec - start_time.tv_nsec;
	}

	/*
	 *  Verify that seconds and nanoseconds are positive values.
	 *  This test is added until I (jmscott) can determine why
	 *  I peridocally see negative times on Linux hosted by VirtualBox
	 *  under Mac OSX.
	 */
	if (sec < 0)
		sec = 0;
	if (nsec < 0)
		nsec = 0;

	/*
	 *  Format the record buffer.
	 */
	snprintf(brr, BRR_SIZE + 1, brr_format,
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		start_time.tv_nsec,
		transport,
		verb,
		algorithm[0] ? algorithm : "",
		algorithm[0] && ascii_digest[0] ? ":" : "",
		ascii_digest[0] ? ascii_digest : "",
		chat_history,
		blob_size,
		sec, nsec
	);

	len = strlen(brr);
	/*
	 *  Open brr file with shared lock, not exclusive.
	 *  fs_wrap() opens with exclusive, to quickly rename
	 *  to fs-<time()>.brr, for wrapping.
	 *
	 *  Note:
	 *	verify that brr file is NOT a sym link
	 */
	int fd = jmscott_open(
		brr_path,
		O_WRONLY|O_CREAT|O_APPEND|O_SHLOCK,
		S_IRUSR|S_IWUSR|S_IRGRP
	);
	if (fd < 0)
		die2("open(brr-path) failed", strerror(errno));
	/*
	 *  Write the entire blob request record in a single write().
	 */
	if (jmscott_write(fd, brr, len) < 0)
		die2("write(brr-path) failed", strerror(errno));
	if (jmscott_close(fd) < 0)
		die2("close(brr-path) failed", strerror(errno));
}
