sub recent2sec
{
	my %recent2sec = (
		'1hr' =>	3600,
	);

	my $sec = $recent2sec{$_[0]};

	die "can not map recent to seconds: $_[0]" unless $sec;
	return $sec;
}

1;
