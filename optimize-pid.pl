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

use lib '/home/ewheeler/src/perl-PDL-Opt-Simplex-Simple/lib';

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

use PDL::Opt::Simplex::Simple;
use Time::HiRes qw/time sleep/;

use Device::SerialPort;

my ($port);

$port = Device::SerialPort->new($ARGV[0]) or die "$ARGV[0]: $!";

my $rotor = 'theta';

my $init_deg = 360;
my $next_deg = 390;

# Abort if it gets dangerous:
my $position_hi_limit = 410;
my $position_lo_limit = 310;

my $iteration_delay = 0.5;
my $timeout = 30;
my $req_good_count = 10;
my $target_accuracy_deg = 2;

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
		values =>  52.891,
		round_each => 0.001,
		minmax => [0, 200],
		#perturb_scale => 5
	},
	"rotor $rotor pid ki" => {
		values =>   11.953,
		round_each => 0.001,
		minmax => [0, 200],
		#perturb_scale => 5
	},
	"rotor $rotor pid kd" => {
		values => 0,
		round_each => 0.001,
		minmax => [-200, 200],
		#perturb_scale => 5
	},
	"rotor $rotor pid tau" => {
		values => 0.02,

		round_each => 0.00001,
		minmax => [0.0111, 10],
		#perturb_scale => 0.5
	},
	#"rotor $rotor pid int_min" => {
	#	values => 0.0625,
	#	round_each => 0.0001,
	#	minmax => [-5, 1],
	#	perturb_scale => 0.1
	#},
	#"rotor $rotor pid int_max" => {
	#	values => 2.5,
	#	round_each => 0.0001,
	#	minmax => [-1, 5],
	#	perturb_scale => 0.1
	#},
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
	#ssize => [ 10, 5, 2.5, 1 ],
	ssize => 1,
	max_iter => 20,
	tolerance => 0.01,
	log => sub {
			my ($vars, $state) = @_;
			our $lc++;

			print Dumper($vars, $state);
			print "^ LOG $lc ^\n";

			# Rest the motor for 10s:
			cmd("rotor $rotor target off");
			cmd("rotor $rotor target off");
			sleep 10;
		},
	f => \&run_test
	);

print Dumper $s->optimize;


exit 0;


# https://towardsdatascience.com/on-average-youre-using-the-wrong-average-geometric-harmonic-means-in-data-analysis-2a703e21ea0
sub harmonic_mean
{
	my @a = @_;

	my $s = 0;

	$s += 1/$_ foreach @a;

	return scalar(@a)/$s;
}

# Geometric mean, but: 
#
# If values can be negative, then take the most negative number and increase
# all numbers by the absoluate-value of that negative.  Then perform the
# geo-mean:
sub geometric_mean_adj
{
	my @a = @_;

	my $s = 1;

	my $min;
	foreach my $v (@a)
	{
		$min = $v if (!defined($min) || $v < $min)
	}

	my $adj = 0;
	$adj = (-$min)+1 if ($min <= 0);

	$s *= $_+$adj foreach @a;

	return $s**(1/scalar(@a)) - $adj;
}

###########################################################################
sub run_test
{
	my ($vars) = @_;

	our $run_count;
	$run_count++;

	my @scores;

	reset_pos($init_deg);
	push @scores, run_test_ang($next_deg, $vars);

	reset_pos($next_deg);
	push @scores, run_test_ang($init_deg, $vars);

	#reset_pos($init_deg/4);
	#push @scores, run_test_ang($next_deg/4, $vars);

	#reset_pos($next_deg/4);
	#push @scores, run_test_ang($init_deg/4, $vars);


	#my $ret = harmonic_mean(@scores);
	my $ret = geometric_mean_adj(@scores);

	print "RUN $run_count FINISHED: Average score: $ret\n";

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
	printf "========================== Kp=%.4f Ki=%.4f Kd=%.4f tau=%.5f target=%.2f\n",
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
		#$vars->{$var} = 0.1 if $vars->{$var} <= 0;
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
	my $good_count_max = 0;
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

		if ($stat->{position} > $position_hi_limit ||
			$stat->{position} < $position_lo_limit)
		{
			print "**** ABORT: out of range: pos=$stat->{position}\n";
			print Dumper($stat);
			$dist += 1e9;
			last;
		}

		if (@hist > 50)
		{
			my $first = $hist[$#hist-50]->{dist};
			my $last = $hist[$#hist]->{dist};

			if ($last_dist > 3 && $first - $last < 5)
			{
				print "*** ABORT? first=$first last=$last\n";
				#$dist = $dist**2;
				#last;
			}
		}

		# it is "good" if the distance between the target and current position
		# is less than 1 degree.  Break the loop after enough "good" counts:
		if ($last_dist < $target_accuracy_deg)
		{
			$good_count++;
			$good_count_max = $good_count if $good_count > $good_count_max;
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

		sleep($iteration_delay);
	}
	my $rms = 0;
	#$rms = rms_end();

	my $i;
	# oscillation
	my $osc_dist = 0;
	my $osc_dist_count = 0;
	my $osc_speed = 0;
	my $osc_speed_count = 0;
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
			$osc_dist += abs($prev_dist) + abs($cur_dist);
			$osc_dist_count++;
		}

		# Work to minimize the back-and-forth motion.  If we
		# need to alternate between a positive and negative speed
		# then hopefully those adjustments are very small:
		if ($prev->{speed} < 0 && $cur->{speed} > 0
			|| $prev->{speed} > 0 && $cur->{speed} < 0)
		{
			$osc_speed += abs($prev->{speed}) + abs($cur->{speed})*100;
			$osc_speed_count++;
		}


		#print "$i. d=$cur_dist s=$cur->{speed} osc-dist=$osc_dist osc-speed=$osc_speed\n";

	}

	my $last_hist = $hist[$#hist];
	my $last_dist = abs($last_hist->{target} - $last_hist->{position});
	my $last_speed = abs($last_hist->{speed});

	$rms *= 100;

	# Penalize timeouts
	$elapsed = $timeout*1.5 if ($elapsed > $timeout);


	my $range = $max - $min;
	my $range_err = abs($next_deg-$range);
		
	my $score = 0
		+ 10*$dist
		#+ $range_err**2
		+ $elapsed**2
		+ 10*2**$osc_dist_count		+ $osc_dist
		+ 10*2**$osc_speed_count 	#+ $osc_speed
		
		- min(1000, 10/$last_dist)
		- min(1000, 10/$last_speed)
		- 100*$good_count
		- 10*$good_count_max
		
		#+ $rms**2
		;

	printf "\nScore=%.2f: elapsed=%.2f osc-dist-count=%.2f osc-dist=%.2f osc-speed-count=%.2f osc-speed=%.2f dist=%.2f last_dist=%.3f last_speed=%.3f range=%.2f range_err=%.2f rms=%.2f, gc_max=%d\n\n",
		$score,
		$elapsed,
		$osc_dist_count,
		$osc_dist,
		$osc_speed_count,
		$osc_speed,
		$dist,
		$last_dist,
		$last_speed,
		$range,
		$range_err,
		$rms,
		$good_count_max
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

	# Re-run the commands every iteration in case we need to
	# hardware reset during the run:
	my $good = 0;
	do
	{
		foreach my $var (keys %var_init) 
		{
			my $val = $var_init{$var};
			$val = $val->{values} if (ref($val));
			cmd("$var $val");
		}

		my $stat;

		cmd("mv $rotor $ang") if defined $ang;
		cmd("rotor $rotor target on");

		$stat = rotor_stat($rotor);
		$good++ if (abs($stat->{target} - $stat->{position}) < $target_accuracy_deg);

		print "reset[$good]: $stat->{position} => $stat->{target}\n";
		sleep 0.5;
	}
	while ($good < 3);
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

sub max ($$) { $_[$_[0] < $_[1]] }
sub min ($$) { $_[$_[0] > $_[1]] }
