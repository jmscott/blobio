#
#  Synopsis:
#	Asynchronously probe a socket for a connection.
#  Usage:
#	probe_ip_port
#
use Errno;
use Fcntl;
use Socket;

sub probe_ip_port
{
        my ($host, $port, $timeout) = @_;
	$host = lc $host;

        my $ip;
        if($host eq "localhost") {
                $ip = '127.0.0.1';
        } elsif ($host =~ m/[a-z]/) {
                defined ($ip = gethostbyname($host)) or return 0;
                $ip = inet_ntoa($ip);
        } else {
                $ip = $host;		#  ipaddress
        }

        # Create socket.
        socket(my $con, PF_INET, SOCK_STREAM, getprotobyname('tcp'))
						or return 0;
	#  run probe code in eval, so socket is properly closed.
	#  not properly closing the socket seems to confuse xinetd
	#  on mac big sur.  details.

	my $is_open = eval
	{
		# autoflush
		$_ = select($con);  $| = 1;  select $_;

		$_ = fcntl($con, F_GETFL, 0) or return 0;
		fcntl($con, F_SETFL, $_ | FD_CLOEXEC) or return 0;

		if ($timeout > 0) {
			# Set O_NONBLOCK so we can time out connect().
			$_ = fcntl($con, F_GETFL, 0) or return 0;
			fcntl($con, F_SETFL, $_ | O_NONBLOCK) or return 0;
		}

		#  in async mode, connect() always answers,
		#  even when no listener, so defer "Connection refused"
		#  after connect().

		connect($con, pack_sockaddr_in($port, inet_aton($ip))) or
					$!{EINPROGRESS} or return 0;

		# reset connectio to synchronous

		$_ = fcntl($con, F_GETFL, 0) or return 0;
		fcntl($con, F_SETFL, $_ & ~O_NONBLOCK) or return 0;

		#  Use select() to poll for completion or error.

		my $vec;
		vec($vec, fileno($con), 1) = 1;
		select(undef, $vec, undef, $timeout);
		return 0 unless(vec($vec, fileno($con), 1));

		#
		#  Note:
		#	we get a connection, even on error, so see
		#	if getsockopt correctly builds the socket
		#	structute.  surely, a less hackish method
		#	exists.
		#
		$! = unpack("L", getsockopt($con, SOL_SOCKET, SO_ERROR));
		return 0 if $!;

		setsockopt(
			$con,
			SOL_SOCKET,
			SO_SNDTIMEO,
			pack("L!L!", $timeout, 0)
		) or return 0;
		setsockopt(
			$con,
			SOL_SOCKET,
			SO_RCVTIMEO,
			pack("L!L!", $timeout, 0)
		) or return 0;
		return 1;
	};
	close($con);
	return $is_open; 
}

1;
