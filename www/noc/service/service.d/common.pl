sub probe_port {
        my ($name, $port, $timeout) = @_;

        my $ip;
        if(lc $name eq "localhost") {
                $ip = "127.0.0.1";  # Don't depend on a DNS for this triviality.
        } elsif($name =~ qr~[a-zA-Z]~s) {
                defined ($ip = gethostbyname($name)) or return 0;
                $ip = inet_ntoa($ip);
        } else {
                $ip = $name;
        }

        # Create socket.
        socket(my $connection, PF_INET, SOCK_STREAM, getprotobyname('tcp'))
		or return 0;

        # Set autoflushing.
        $_ = select($connection); $| = 1; select $_;

        # Set FD_CLOEXEC.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ | FD_CLOEXEC) or return 0;

        if($timeout) {
                # Set O_NONBLOCK so we can time out connect().
                $_ = fcntl($connection, F_GETFL, 0) or return 0;
                fcntl($connection, F_SETFL, $_ | O_NONBLOCK) or return 0;
        }

        # Connect returns immediately because of O_NONBLOCK.
        connect($connection, pack_sockaddr_in($port, inet_aton($ip))) or
		$!{EINPROGRESS} or return 0;

        # Reset O_NONBLOCK.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ & ~ O_NONBLOCK) or return 0;

        #  Use select() to poll for completion or error.
	#  When connect succeeds we can write.
        my $vec = "";
        vec($vec, fileno($connection), 1) = 1;
        select(undef, $vec, undef, $timeout);
        return 0 unless(vec($vec, fileno($connection), 1));

	#
	#  Note:
        #	we get a connection, even on error, but cannot unpack structure.
	#	got to be a better method!
	#
        $! = unpack("L", getsockopt($connection, SOL_SOCKET, SO_ERROR));
        return 0 if $!;

        setsockopt($connection, SOL_SOCKET, SO_SNDTIMEO, pack("L!L!",
						$timeout, 0)) or return 0;
        setsockopt($connection, SOL_SOCKET, SO_RCVTIMEO, pack("L!L!",
						$timeout, 0)) or return 0;
        return 1
}

1;
