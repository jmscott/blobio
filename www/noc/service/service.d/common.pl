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
        socket(my $connection, PF_INET, SOCK_STREAM, getprotobyname('tcp'))
						or return 0;

        # autoflush
        $_ = select($connection); $| = 1; select $_;

        # Set FD_CLOEXEC.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ | FD_CLOEXEC) or return 0;

        if($timeout) {
                # Set O_NONBLOCK so we can time out connect().
                $_ = fcntl($connection, F_GETFL, 0) or return 0;
                fcntl($connection, F_SETFL, $_ | O_NONBLOCK) or return 0;
        }

        # in async mode, connect() always answers, even when no listener.  
        connect($connection, pack_sockaddr_in($port, inet_aton($ip))) or
		$!{EINPROGRESS} or return 0;

        # disable O_NONBLOCK.
        $_ = fcntl($connection, F_GETFL, 0) or return 0;
        fcntl($connection, F_SETFL, $_ & ~O_NONBLOCK) or return 0;

        #  Use select() to poll for completion or error.
	#  When connect succeeds we can write.
        my $vec;
        vec($vec, fileno($connection), 1) = 1;
        select(undef, $vec, undef, $timeout);
        return 0 unless(vec($vec, fileno($connection), 1));

	#
	#  Note:
        #	we get a connection, even on error, so unpack structure to
	#	determine error!  got to be a better method!
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
