#!/usr/bin/perl

use strict;
use warnings;

use Device::SerialPort;

my ($port);

$port = Device::SerialPort->new($ARGV[0]) or die "$ARGV[0]: $!";
$port->baudrate(57600);
$port->parity("none");
$port->databits(8);
$port->stopbits(1);        # POSIX does not support 1.5 stopbits

my $now = time();
while ($now == time()) {}

$now = time();
while ($now == time()) {}

$port->write("date set " . time() . "\n");
sleep 1;
my ($n, $t) = $port->read(100);

print "<< $t\n";
