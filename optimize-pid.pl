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
$port->baudrate(115200);
$port->parity("none");
$port->databits(8);
$port->stopbits(1);        # POSIX does not support 1.5 stopbits

# phi tests:
my $rotor = 'theta';
my $init_deg = 230;
my $next_deg = 245.56;

my $track_date = '2023 08 02 04 11 20';
my $track_date_scale = 20;
my $track_sat = 'zarya';
#my $track_init_deg = 39.3; # phi use target at the moment of $track_date
my $track_init_deg = 245.56; # theta use target at the moment of $track_date
my $track_test_seconds = 20;
## Abort if it gets dangerous:
#my $position_lo_limit = -20;
#my $position_hi_limit = 80;
my $position_lo_limit = 100;
my $position_hi_limit = 300;

# theta tests:
#my $rotor = 'theta';
#my $init_deg = 400;
#my $next_deg = 430;

# Abort if it gets dangerous:
#my $position_lo_limit = 370;
#my $position_hi_limit = 450;


my $iteration_delay = 0.5;
my $timeout = 25;
my $req_good_count = 12;
my $target_accuracy_deg = 0.15;
my $reset_accuracy_deg = 0.5;

my %var_init = (
	
	# demo theta
	"rotor $rotor ramptime" => {
		values =>  0.5,
		round_each => 0.1,
		minmax => [0.0, 5],
		enabled => 0,
		#perturb_scale => 5
	},
	"rotor $rotor exp" => {
		values =>  1.79,
		round_each => 0.01,
		minmax => [1.0, 3],
		enabled => 1,
		#perturb_scale => 5
	},
	"rotor $rotor pid kp" => {
		values =>  0.0005,
		round_each => 1e-4,
		minmax => [1e-4, 1],
		#perturb_scale => 5
	},
	"rotor $rotor pid ki" => {
		values =>  0.0012,
		round_each => 1e-4,
		minmax => [1e-4, 1],
		#perturb_scale => 5
	},
	"rotor $rotor pid kvfb" => {
		values =>  0.0002,
		round_each => 1e-4,
		minmax => [1e-4, 1],
		#perturb_scale => 5
	},
	"rotor $rotor pid kvff" => {
		values =>  1e-4,
		round_each => 1e-4,
		minmax => [1e-4, 1],
		enabled => 1,
		#perturb_scale => 4
	},
	"rotor $rotor pid kaff" => {
		values => 2e-3,
		round_each => 1e-4,
		minmax => [1e-4, 1],
		enabled => 1,
		#perturb_scale => 4
	},

	"rotor $rotor pid k1" => {
		values =>  0.059,
		round_each => 1e-3,
		minmax => [1e-4, 3],
		#perturb_scale => 5
	},
	"rotor $rotor pid k2" => {
		values =>  3,
		round_each => 1e-1,
		minmax => [1e-1, 3],
		#perturb_scale => 5
	},
	"rotor $rotor pid k3" => {
		values =>  0.4498,
		round_each => 1e-4,
		minmax => [1e-4, 3],
		#perturb_scale => 1
	},
	"rotor $rotor pid k4" => {
		values =>  0.0001,
		round_each => 1e-4,
		minmax => [1e-4, 3],
		#perturb_scale => 4
	},

	# demo phi
#	"rotor $rotor ramptime" => {
#		values =>  0.5,
#		round_each => 0.1,
#		minmax => [0.0, 5],
#		enabled => 0,
#		#perturb_scale => 5
#	},
#	"rotor $rotor exp" => {
#		values =>  1.79,
#		round_each => 0.01,
#		minmax => [1.0, 3],
#		enabled => 1,
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kp" => {
#		values =>  0.0001,
#		round_each => 1e-4,
#		minmax => [1e-4, 1],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid ki" => {
#		values =>  0.0012,
#		round_each => 1e-4,
#		minmax => [1e-4, 1],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kvfb" => {
#		values =>  0.0002,
#		round_each => 1e-4,
#		minmax => [1e-4, 1],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kvff" => {
#		values =>  1e-4,
#		round_each => 1e-4,
#		minmax => [1e-4, 1],
#		enabled => 1,
#		#perturb_scale => 4
#	},
#	"rotor $rotor pid kaff" => {
#		values => 2e-3,
#		round_each => 1e-4,
#		minmax => [1e-4, 1],
#		enabled => 1,
#		#perturb_scale => 4
#	},
#
#	"rotor $rotor pid k1" => {
#		values =>  0.039,
#		round_each => 1e-3,
#		minmax => [1e-4, 3],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid k2" => {
#		values =>  3,
#		round_each => 1e-1,
#		minmax => [1e-1, 3],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid k3" => {
#		values =>  0.4498,
#		round_each => 1e-4,
#		minmax => [1e-4, 3],
#		#perturb_scale => 1
#	},
#	"rotor $rotor pid k4" => {
#		values =>  0.0001,
#		round_each => 1e-4,
#		minmax => [1e-4, 3],
#		#perturb_scale => 4
#	},


	# big theta 
#	"rotor $rotor pid kp" => {
#		values =>  52.401,
#		round_each => 0.001,
#		minmax => [0, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid ki" => {
#		values =>   12.286,
#		round_each => 0.001,
#		minmax => [0, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kd" => {
#		values => 0.185,
#		round_each => 0.001,
#		minmax => [-200, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid tau" => {
#		values => 0.09314,
#
#		round_each => 0.00001,
#		minmax => [0.0111, 10],
#		#perturb_scale => 0.5
#	},

#	# big phi
#	"rotor $rotor ramptime" => {
#		values =>  1.3,
#		round_each => 0.1,
#		minmax => [0.5, 5],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kp" => {
#		values =>  51.435,
#		round_each => 0.001,
#		minmax => [0, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid ki" => {
#		values =>   11.399,
#		round_each => 0.001,
#		minmax => [0, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid kd" => {
#		values => 1.959,
#		round_each => 0.001,
#		minmax => [-200, 200],
#		#perturb_scale => 5
#	},
#	"rotor $rotor pid tau" => {
#		values => 0.62029,
#
#		round_each => 0.00001,
#		minmax => [0.0111, 10],
#		#perturb_scale => 0.5
#	},
	);

my %var_reset = %var_init;

#print cmd("rotor $rotor detail");
print Dumper rotor_stat($rotor);
#exit;

#reset_pos($init_deg);

my $s = PDL::Opt::Simplex::Simple->new(
	vars => {
		%var_init
	},
	#ssize => [ 10, 5, 2.5, 1 ],
	#ssize => [1, 0.5 ],
	ssize => [ 0.1, 0.05, 0.01 ],
	max_iter => 200,
	tolerance => 0.00001,
	log => sub {
			my ($vars, $state) = @_;
			our $lc++;

			#%var_reset = %{ $state->{best_vars} };


			print "========================================= LOG ===================================\n";
			print Dumper($vars, $state);
			print "^ LOG $lc ^\n";

			# Rest the motor for 10s:
			cmd("rotor $rotor target off");
			cmd("rotor $rotor target off");
			sleep 1;
		},
	f => \&run_test
	);

my $result = $s->optimize;

# Apply best result:
foreach my $var (keys %$result) 
{
	cmd("$var $result->{$var}->{values}");
	cmd("$var $result->{$var}->{values}");
}


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
	my $ret = 0;

	reset_pos($init_deg);
	push @scores, run_test_ang($next_deg, $vars);

	#reset_pos($next_deg);
	#push @scores, run_test_ang($init_deg, $vars);

	#$ret = harmonic_mean(@scores);
	#$ret = geometric_mean_adj(@scores);


	# Skip tracking tests if the score is really bad to 
	# revent uncontrolled oscillation:
	$track_date_scale = 1;
	$ret += 100 * run_test_track($vars) if ($ret < 1e9);

	$track_date_scale = 10;
	$ret += run_test_track($vars) if ($ret < 1e9);

	reset_pos(152.76);
	push @scores, run_test_ang($init_deg, $vars);

	$ret = max($scores[0] // 0, $scores[1] // -1e9);

	print "RUN $run_count FINISHED: Score: $ret\n";

	return $ret;
}

sub run_test_track
{
	my ($vars) = @_;

	our $run_count;
	$run_count++;

	reset_pos($track_init_deg);
	cmd("rotor $rotor target off");
	cmd("sat track $track_sat");
	cmd("sat track $track_sat");
	foreach my $var (sort keys %$vars) 
	{
		my $val = $vars->{$var};
		$val = $val->{values} if (ref($val));
		cmd("$var $val");
	}
	cmd("rotor $rotor pid reset\n");
	cmd("date scale $track_date_scale");
	cmd("date set $track_date temp");
	cmd("rotor $rotor target on");

	my $count = $track_test_seconds * $track_date_scale;

	my %stat_sum;
	my %stat_sum_abs;
	my $ret = 0;

	print_var_line($vars);
	my $line = cmd("rotor $rotor stat $count");

	# parse lines, aggregate, store them in stat:
	my @stats;
	my @lines = split(/\n/, $line);
	foreach my $line (@lines)
	{
		my %stat;
		while ($line =~ /(\w+)[:=]\s*([0-9-.]+)/g)
		{
			$stat{$1} = $2;
			$stat_sum{$1} += $2;
			$stat_sum_abs{$1} += abs($2);
		}

		push @stats, \%stat if (scalar(keys(%stat)) > 0);
	}

	my $osc_speed = 0;
	my $osc_speed_count = 0;
	my $prev_speed = 0;

	foreach my $stat (@stats)
	{
		#print Dumper $stat, { ps => $prev_speed };
		if ($prev_speed < 0 && $stat->{speed} > 0 || $prev_speed > 0 && $stat->{speed} < 0)
		{
			$osc_speed += abs($prev_speed - $stat->{speed});
			$osc_speed_count++;
		}

		$prev_speed = $stat->{speed};
	}

	#print Dumper (
	#	{ 
	#		sum => \%stat_sum,
	#		sum_abs => \%stat_sum_abs,
	#		osc_speed => { count => $osc_speed_count, speed => $osc_speed },
	#	}
	#	);

	if (!defined($stat_sum{max}) || !defined($stat_sum{avg}) || !defined($stat_sum_abs{err}))
	{
		print "Undefined vars?\n";
		$ret = 1e9;
	}
	else
	{
		$ret =
			#+ 1000 * $stat_sum{max}
			+ 1000 * $stat_sum{avg}
			+ 1000 * $stat_sum_abs{err}
			+ 100 *$osc_speed_count * $osc_speed;
			;
	}

	print "max=$stat_sum{max} avg=$stat_sum{avg} err=$stat_sum_abs{err} osc_count=$osc_speed_count osc_speed=$osc_speed\n";
	cmd("sat reset");

	print "TRACK RUN $run_count FINISHED: Average score: $ret\n";

	return $ret;

}

sub run_test_ang
{
	my ($ang, $vars) = @_;

#print Dumper $vars;

	cmd("reset");
	cmd("reset");
	cmd("rotor $rotor target off");
	cmd("rotor $rotor target off");
	cmd("rotor $rotor pid reset");
	cmd("rotor $rotor pid reset");
	foreach my $var (sort keys %$vars) 
	{
		#$vars->{$var} = 0.1 if $vars->{$var} <= 0;
		cmd("$var $vars->{$var}");
		cmd("$var $vars->{$var}");
	}

	cmd(sprintf("mv $rotor %f", $ang));
	cmd(sprintf("mv $rotor %f", $ang));
	cmd("rotor $rotor target on");
	cmd("rotor $rotor target on");

	print_var_line($vars, $ang);
	#rms_start();

	my $dist = 0;
	my $count = 0;
	my $good_count = 0;
	my $good_count_max = 0;
	my $good_dist_max = 0;
	my $good_resets = 0;
	my $start = time();
	my $elapsed = 0;
	my $max_overshoot = 0;
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
			$good_dist_max = $last_dist if $good_count_max >= 2 && $last_dist > $good_dist_max;
		}
		else
		{
			$good_resets++ if $good_count > 0;
			$good_count = 0;
		}

		my $overshoot = 0;
		if ($good_count_max >= 1)
		{
			if ($stat->{target} == $init_deg && $stat->{position} < $init_deg)
			{
				$overshoot = abs($init_deg - $stat->{position})
			}
			elsif ($stat->{target} == $next_deg && $stat->{position} > $next_deg)
			{
				$overshoot = abs($next_deg - $stat->{position})
			}
		}

		$max_overshoot = $overshoot if $overshoot > $max_overshoot;


		$elapsed = (time()-$start);
		printf "%2d [%3.2f]. dist=%3.2f last_dist=%3.2f good=%2d %+3.2f deg -> %+3.2f: P=%+3.2f I=%+3.2f D=%+3.2f FF=%+3.2f S=%+3.2f speed=%+3.2f%% overshoot=%.2f\n",
			$count,
			$elapsed,
			$dist, $last_dist,
			$good_count,
			$stat->{position},
			$stat->{target},
			$stat->{proportional}*100,
			$stat->{integrator}*100,
			$stat->{damping}*100,
			$stat->{'feed-forward'}*100,
			$stat->{smc}*100,
			$stat->{speed}*100,
			$overshoot
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
	my $last_speed = abs($last_hist->{speed}) * 100;

	$rms *= 100;

	# Penalize timeouts
	$elapsed = $timeout*1.5 if ($elapsed > $timeout);

	$good_dist_max = 10 if !$good_dist_max;


	my $range = $max - $min;
	my $range_err = abs($next_deg-$range);
		
	my $score = 0
		+ 1000*$dist
		#+ $range_err**2
		+ 10000 * $elapsed
		+ 1000*$osc_dist_count *$osc_dist
		+ 1000*$osc_speed_count 	+ $osc_speed # prevents Kd from going crazy
		#+ (400*$good_dist_max/($good_count_max || 1))**2
		+ 10 * $max_overshoot
		+ 10 * $good_resets
		
		#- min(100, 10/max(0.01,$last_dist))
		#- min(100, 10/max(0.01,$last_speed))
		#- 100*$good_count
		- 100000*$good_count_max
		- 4000 * $vars->{"rotor $rotor ramptime"}
		#+ $rms**2
		;

	printf "\nScore=%.2f: elapsed=%.2f osc-dist-count=%.2f osc-dist=%.2f osc-speed-count=%.2f osc-speed=%.2f dist=%.2f last_dist=%.3f last_speed=%.3f range=%.2f range_err=%.2f rms=%.2f gc_max=%d gd_max=%.2f gr=%.2f os=%.2f\n\n",
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
		$good_count_max,
		$good_dist_max,
		$good_resets,
		$max_overshoot,
		;

	return $score
}

sub rotor_stat
{
	my ($rotor) = @_;
	my %vars;

	do {
		my $stat = cmd("rotor $rotor detail"); 
		while ($stat =~ /([0-9a-zA-Z-]+):\s*(\S+)/gs)
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
	foreach my $var (keys %var_init) 
	{
		my $val = $var_reset{$var};
		$val = $val->{values} if (ref($val));
		cmd("$var $val");
	}

	my $count = 0;
	do
	{

		my $stat;

		cmd("mv $rotor $ang") if defined $ang;
		cmd("rotor $rotor target on");

		$stat = rotor_stat($rotor);
		$good++ if (abs($stat->{target} - $stat->{position}) < $reset_accuracy_deg);

		print "== reset[$good]: $stat->{position} => $stat->{target}\n";
		sleep 0.5;

		$count++;
		if (!($count % 30))
		{
			system("cd build; make r");
		}

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
	$port->write("$cmd\n");
	sleep(0.1);
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

			if ($line =~ /Unknown|usage|expected/i)
			{
				print "\nInvalid line:\n  $cmd\n  $line\n";
				$success = 0;
			}

			print "<< $line" if $cmd !~ /rotor.*detail|rotor.*pid|reset/;

		}

		# Exit when the prompt is presented:
		last if $in =~ /\]# $/;
	}
	#print "$in\n";
	
	$ret = undef if !$success;

	return $ret;
}

sub print_var_line
{
	my ($vars, $ang) = @_;

	$ang //= -1;
	printf "\n\n=== Kp=%.7f Ki=%.7f Kvfb=%.7f Kvff=%.7f Kaff=%.7f K1=%.7f K2=%.7f K3=%.7f K4=%.7f exp=%.2f ramp=%.2f target=%.2f\n",
		$vars->{"rotor $rotor pid kp"},
		$vars->{"rotor $rotor pid ki"},
		$vars->{"rotor $rotor pid kvfb"},
		$vars->{"rotor $rotor pid kvff"},
		$vars->{"rotor $rotor pid kaff"},
		$vars->{"rotor $rotor pid k1"},
		$vars->{"rotor $rotor pid k2"},
		$vars->{"rotor $rotor pid k3"},
		$vars->{"rotor $rotor pid k3"},
		$vars->{"rotor $rotor exp"},
		$vars->{"rotor $rotor ramptime"},
		$ang;
}
sub max ($$) { $_[$_[0] < $_[1]] }
sub min ($$) { $_[$_[0] > $_[1]] }

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
