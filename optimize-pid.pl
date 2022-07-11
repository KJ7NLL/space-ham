#!/usr/bin/perl


# This code is a bit hacked together but it works pretty well.  Define '$rotor'
# as the rotor you will be optimizing and specify your initial kp,ki,kd values
# that are acceptable, even if not perfect because it will use these initial
# values to "home" the rotor after each iteration.
#
# You might notice that serial IO calls are doubled in many places in the code:
# this is to prevent loss over the serial port that breaks the simulation.
# Really we should make a smarter serial protocol for optimizing the variables
# and checking rotor status, but that isn't the target of the project.
#
# See the PDL::Opt::Simplex::Simple documentation for Simplex-specific details.

use strict;
use warnings;

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

use PDL::Opt::Simplex::Simple;
use Time::HiRes qw/time sleep/;

use Device::SerialPort;

my ($port);

$port = Device::SerialPort->new($ARGV[0]) or die "$ARGV[0]: $!";

my $rotor = 'phi';
our $init_deg = 0;

my $timeout = 10;
my $req_good_count = 20;

my %var_init = (
	########## theta
	#kp => 45,
	#ki => 15,
	#kd => 5,
	#tau => 3
	
	# theta
#	"rotor $rotor pid kp" => {
#		values => 190.90,
#		perturb_scale => 5
#	},
#	"rotor $rotor pid ki" => {
#		values => 166.33,
#		perturb_scale => 5
#	},
#	"rotor $rotor pid kd" => {
#		values => 1.02,
#		perturb_scale => 0.5
#	},
#	"rotor $rotor pid tau" => {
#		values => 15.48,
#		perturb_scale => 1
#	},

	# phi
	"rotor $rotor pid kp" => {
		values => 46,
		minmax => [0, 100],
		perturb_scale => 2
	},
	"rotor $rotor pid ki" => {
		values => 43,
		minmax => [0, 100],
		perturb_scale => 2
	},
	"rotor $rotor pid kd" => {
		values => 22,
		minmax => [0, 100],
		perturb_scale => 2
	},
	"rotor $rotor pid tau" => {
		values => 20,
		minmax => [0, 100],
		perturb_scale => 2
	},
	#"motor $rotor hz" => {
	#		values => 1047,
	#		perturb_scale => 100,
	#		minmax => [900, 20000]
	#}
	
	########## phi
	#kp => 15,
	#ki => 20,
	#kd => 10,
	#tau => 3
	
#	kp => 46.07,
#	ki => 43.29,
#	kd => 22.90,
#	tau => 20.09
	);


#print cmd("rotor $rotor detail");
#print Dumper rotor_stat($rotor);
#exit;

reset_pos($init_deg);

my $s = PDL::Opt::Simplex::Simple->new(
	vars => {
		%var_init
	},
	ssize => .01,
	tolerance => 0.01,
	log => sub { my ($vars, $state) = @_; print "LOG: " . Dumper($vars, $state); },
	f => \&run_test
	);

print Dumper $s->optimize;


exit 0;


###########################################################################
sub run_test
{
	my ($vars) = @_;

	my $ret = 0;


	#reset_pos(360);
	#$ret += run_test_ang(360+30, $vars);
	#reset_pos(360+30);
	#$ret += run_test_ang(360, $vars);

	reset_pos(0);
	$ret += run_test_ang(45, $vars);
	reset_pos(45);
	$ret += run_test_ang(0, $vars);

	$ret /= 2;

	print "RUN FINISHED: Average score: $ret\n";

	return $ret;
}

my $rms_pid;
sub rms_start
{
	$rms_pid = fork();

	if (!$rms_pid)
	{
		exec("parecord -r -d alsa_input.usb-046d_HD_Pro_Webcam_C920_FE6841EF-02.analog-stereo simplex.wav");
	}
}

sub rms_end
{

	print "pid=$rms_pid\n";
	kill('INT', $rms_pid);
	waitpid($rms_pid, 0);

	my $rms = `sox simplex.wav -n stat 2>&1 | grep RMS.*amp | cut -f2 -d: | tr -cd 0-9.`;

	#print "RMS amplitude: $rms\n";

	return $rms;
}

sub run_test_ang
{
	my ($ang, $vars) = @_;

#print Dumper $vars;
	printf "========================== Kp=%.2f Ki=%.2f Kd=%.2f tau=%.2f target=%.2f\n",
		$vars->{"rotor $rotor pid kp"},
		$vars->{"rotor $rotor pid ki"},
		$vars->{"rotor $rotor pid kd"},
		$vars->{"rotor $rotor pid tau"},
		$ang;

	cmd("reset");
	cmd("reset");
	cmd("rotor $rotor target off");
	cmd("rotor $rotor target off");
	cmd("rotor $rotor pid reset");
	cmd("rotor $rotor pid reset");
	foreach my $var (keys %$vars) 
	{
		$vars->{$var} = 0.1 if $vars->{$var} <= 0;
		cmd("$var $vars->{$var}");
		cmd("$var $vars->{$var}");
	}

	cmd(sprintf("mv $rotor %f", $ang));
	cmd(sprintf("mv $rotor %f", $ang));
	cmd("rotor $rotor target on");
	cmd("rotor $rotor target on");

	#rms_start();

	my $dist = 0;
	my $count = 0;
	my $good_count = 0;
	my $start = time();
	my $elapsed = 0;
	my @hist;
	while ($elapsed < $timeout && $good_count < $req_good_count)
	{
		$count++;
		my $stat = rotor_stat($rotor);
		#print Dumper $stat;
		
		push @hist, $stat;

		my $last_dist = $stat->{dist};

		$dist += $last_dist;

		if (@hist > 50)
		{
			my $first = $hist[$#hist-50]->{dist};
			my $last = $hist[$#hist]->{dist};

			if ($last_dist > 3 && $first - $last < 5)
			{
				print "*** ABORT: first=$first last=$last\n";
				$dist = $dist**2;
				last;
			}
		}

		# it is "good" if the distance between the target and current position
		# is less than 1 degree.  Break the loop after enough "good" counts:
		if ($last_dist < 1)
		{
			$good_count++;
		}
		else
		{
			$good_count = 0;
		}


		$elapsed = (time()-$start);
		printf "%2d [%3.2f]. dist=%3.2f last_dist=%3.2f good=%2d %+3.2f deg -> %+3.2f: P=%+3.2f I=%+3.2f D=%+3.2f speed=%+3.2f%%\n",
			$count,
			$elapsed,
			$dist, $last_dist,
			$good_count,
			$stat->{position},
			$stat->{target},
			$stat->{proportional}*100,
			$stat->{integrator}*100,
			$stat->{differentiator}*100,
			$stat->{speed}*100,
			;

		sleep(0.05);
	}
	my $rms = 0;
	#$rms = rms_end();

	my $i;
	# oscillation
	my $osc_dist = 0;
	my $osc_speed = 0;
	my ($min, $max);

	for ($i = 0; $i < @hist; $i++)
	{
		my $cur = $hist[$i];

		my $pos = $cur->{position};

		$min = $pos if (!defined($min) || $pos < $min);
		$max = $pos if (!defined($max) || $pos > $max);

		# Below this line $i > 0:
		next if ($i == 0);
		my $prev = $hist[$i-1];

		my $cur_dist = $cur->{target} - $cur->{position};
		my $prev_dist = $prev->{target} - $prev->{position};

		# If the distance alternates past the target
		# then count the number of oscillations over the target point:
		if ($prev_dist < 0 && $cur_dist > 0
			|| $prev_dist > 0 && $cur_dist < 0)
		{
			$osc_dist++;
		}

		# Work to minimize the back-and-forth motion.  If we
		# need to alternate between a positive and negative speed
		# then hopefully those adjustments are very small:
		if ($prev->{speed} < 0 && $cur->{speed} > 0
			|| $prev->{speed} > 0 && $cur->{speed} < 0)
		{
			$osc_speed += abs($prev->{speed} - $cur->{speed})*100;
		}


		#print "$i. d=$cur_dist s=$cur->{speed} osc-dist=$osc_dist osc-speed=$osc_speed\n";

	}

	$rms *= 100;

	my $range = $max - $min;
	my $score = $dist + $range + $elapsed**2 + 10*2**$osc_dist + $osc_speed**2 + $rms**2;
	#my $score = 0.5*$dist + $range**2 + 10*$elapsed**2 + 10*2**$osc;
	printf "\nScore=%.2f: elapsed=%.2f osc-dist=%.2f osc-speed=%.2f dist=%.2f range=%.2f rms=%.2f\n\n",
		$score,
		$elapsed,
		$osc_dist,
		$osc_speed,
		$dist,
		$range,
		$rms,
		;

	return $score
}

sub rotor_stat
{
	my ($rotor) = @_;
	my %vars;

	do {
		my $stat = cmd("rotor $rotor detail"); 
		while ($stat =~ /(\w+):\s*(\S+)/gs)
		{
			$vars{lc $1} = $2;
		}

	} while (!defined($vars{out}));
	
	$vars{dist} = abs($vars{target} - $vars{position});
	
	# PID output "out" is (should be) instantaneous speed, so alias:
	$vars{speed} = $vars{out};
	return \%vars;
}

sub reset_pos
{
	my $ang = shift;

	cmd("reset");
	cmd("reset");
	sleep 0.1;

	foreach my $var (keys %var_init) 
	{
		my $val = $var_init{$var};
		$val = $val->{values} if (ref($val));
		cmd("$var $val");
	}

	my $stat;

	cmd("mv $rotor $ang") if defined $ang;
	cmd("mv $rotor $ang") if defined $ang;
	cmd("rotor $rotor target on");
	cmd("rotor $rotor target on");
	do
	{
		$stat = rotor_stat($rotor);
		print "reset: $stat->{position} => $stat->{target}\n";
		sleep 0.5;
	}
	while (abs($stat->{target} - $stat->{position}) > 1);



}

sub cmd
{
	my $cmd = shift;
	my $ret = 0;
	while (!$ret)
	{
		$ret = _cmd($cmd);
		if (!$ret)
		{
			print "Error: Failed command $cmd\n";
		}
	}

	return $ret;
}

sub _cmd
{
	my ($cmd) = @_;
	my $success = 1;

	print ">> $cmd\n" if $cmd !~ /rotor.*detail/;
	sleep(0.01);
	$port->write("$cmd\n");
	my $ret = '';
	my $in = '';
	while (1)
	{
		my ($count,$saw) = $port->read(255);
		$in .= $saw if ($count > 0);
		if ($in =~ s/(^[^\n]+\n)//)
		{
			my $line;
			$line .= $1;
			$ret .= $line;

			if ($line =~ /Unknown|usage/i)
			{
				print "\nInvalid line:\n  $cmd\n  $line\n";
				$success = 0;
			}

			#print "<< $line" if $cmd !~ /rotor.*detail/;;

		}

		# Exit when the prompt is presented:
		last if $in =~ /\]# $/;
	}
	#print "$in\n";
	
	$ret = undef if !$success;

	return $ret;
}
