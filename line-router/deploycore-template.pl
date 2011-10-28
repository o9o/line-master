#!/usr/bin/perl
my $netdev = "REMOTE_DEDICATED_IF_ROUTER";
my $gateway = "REMOTE_DEDICATED_IP_HOSTS";

sub command {
	my ($cmd) = @_;

	print "$cmd\n";
	system "sudo $cmd";
}

# Flush the routing table - only the 10/8 routes
foreach my $route (`netstat -rn | grep '^10.'`) {
	my ($dest, $gate, $linuxmask) = split ' ',$route;
	command("/sbin/route delete -net $dest netmask $linuxmask");
}

# enable routing
command "sh -c \'echo 1 > /proc/sys/net/ipv4/ip_forward\'";

# disable stuff that might interfere with our simulation
command "sh -c \'echo 0 > /proc/sys/net/ipv4/conf/$netdev/rp_filter\'";
command "sh -c \'echo 1 > /proc/sys/net/ipv4/conf/$netdev/accept_local\'";
command "sh -c \'echo 0 > /proc/sys/net/ipv4/conf/$netdev/accept_redirects\'";
command "sh -c \'echo 0 > /proc/sys/net/ipv4/conf/$netdev/send_redirects\'";

print "Gateway: $gateway\n";
command "/sbin/route add -net 10.0.0.0/8 gw $gateway";
