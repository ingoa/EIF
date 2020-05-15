#!/usr/bin/perl
#
# postemesg.pl 
# Perl script that emulates the Tivoli postemsg command
#
# Requires that Postemsg.pm be in the perl include path
#
# I.V. Blankenship Gulf Breeze Software
# March 31, 2009
#
# Note: Does not implement the -f postemesg option and offers
#       the -p option to specify a port
#
use Postemsg;
use Getopt::Std;

sub Usage
{
  warn "$0 -S EIF_HOST [-p EIF_PORT] [-m MESSAGE] [-r SEVERITY] [attribute=value...] CLASS SOURCE\n" .
  "\t-S EIF_HOST Hostname or IP address where event will be sent.\n" .
  "\t-p EIF_PORT Port EIF_HOST is listening on, defualt is to use RPC.\n" .
  "\t-m MESSAGE Specifies the text of the event.\n" .
  "\t-r Specifies a severity (FATAL,CRITICAL,MINOR,WARNING,UNKNOWN,HARMLESS)\n" .
  "\tattribute=value Assigns a value to any valid attribute.\n" . 
  "\t                The attribute should be one defined for the event class.\n" . 
  "\t                Separate multiple attribute=value expressions with spaces and \n" .
  "\t                enclose values containing spaces within quotes.\n" .
  "\tCLASS Specifies the class of the event.\n" .
  "\tSOURCE Specifies the source of the event.\n";
  exit(1);
}

my $g_server = 'TECHOST';
my $g_port = 5529;
my $g_class;
my $g_severity;
my $g_source;
my $g_msg;
my %slots = ();

my $opts = ();
Usage() unless getopts('S:p:m:r:',\%opts); 
Usage() unless $opts{S};

my %severities = (FATAL => 1,CRITICAL => 1, MINOR => 1, WARNING => 1, UNKNOWN => 1, HARMLESS => 1);

if($#ARGV >= 1)
{
  $g_source = $ARGV[$#ARGV];
  $g_class = $ARGV[$#ARGV - 1];
  for(my $i=0;$i<$#ARGV - 1;$i++)
  {
    $ARGV[$i] =~ /(\S+)=(.+)/ and do
    {
      $slots{$1}=$2;
      next;
    };
    die "Invalid attribute value specification $ARGV[$i]\n";
  }
}
else { Usage(); }

if($opts{p})
{
  $g_port = $opts{p};
  if($g_port =~ /\d+/) { $g_port=int($g_port); }
  else 
  {
    die "$g_port is not a valid port number\n";
  }
}
else
{
  $g_port = 0;
}

if($opts{r})
{
  $g_severity = uc($opts{r});
  warn "$g_severity is not a standard severity.\n" unless defined $severities{$g_severity};
}

$g_msg = $opts{m} if $opts{m};

SendEvent(\%slots);

exit(0);

sub SendEvent
{
  my ($slots) = @_;
  $msg = Postemsg->new($g_server,$g_port);  
  $msg->setClass($g_class); 
  $msg->setAdapter($g_source); 
  $msg->setMessage($g_msg) if $g_msg;
  $msg->setSeverity($g_severity) if $g_severity;
  foreach my $slot (keys %$slots)
  {
    $msg->setSlotValue($slot,$slots->{$slot}); 
  }
  #$msg->sendMessage();  
  $msg->print();         
}

