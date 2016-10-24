#!/usr/bin/perl
use Postemsg;

$msg = Postemsg->new("odin"); #-- You must set the server hostname
$msg->setClass("EVENT");  #--- You better set a valid class name and
$msg->setAdapter("NT");     #    a valid adapter name
$msg->setMessage("This is a new test of the postemsg Perl module");
$msg->setSlotValue("hostname","johnk");
$msg->setSlotValue("origin","146.84.113.17");
$msg->sendMessage();             #--- Sends the message to TEC
$msg->print();                #---prints all slot values

exit(0);


#================================================================
#  Postemsg Package
#
#  Description:
#
#  This package executes the tasks performed by Tivoli Systems
#  postemsg executable in a platform independent manner. The
#  various TEC message slots can be set with setter methods.
#  The actual message is sent to TEC by first: performing an
#  emulated RPC call to obtain the port where the TEC wants to
#  talk and the setting up a TCP/IP connection to that port
#  to transmit the data.  This nut was cracket by purusing
#  AEF C code and sniffing the wire while using
#  postemsg from the command line.
#
#  Written by: Jim Boone, Duke Energy Corp.
#  Date: May 18, 1998
#
#================================================================
package Postemsg;

use Exporter;
use Socket;

#--- Export package subroutines ---
@ISA = ('Exporter');
@EXPORT = qw(
  new
  setServer
  setAdapter
  setSeverity
  setClass
  setMessage
  setSlotValue
  deleteSlotValue
  deleteAllSlotValues
  sendMessage
  print
);

#--- Global Variables ---
$HDR_HEADER_LENGTH = 2000; # from AEF C code

#----------------------------------------------
#  This subroutine returns a reference to the hash that stores
#  all variables and slot values. Default values are set for base
#  slots.
#
sub new {
  my ($class,$server) = @_;
  bless {}, $class;
  $class->{base} = {
    'server'   => $server,
    'class'    => 'Logfile_Base',
    'adapter'  => 'Logfile',
    'severity' => 'WARNING',
    'msg' => "\'\'",
  };
  return $class;
}
#----------------------------------------------
sub setServer {
  my($this,$server) = @_;
  return $this->{base}->{server} = $server;
}
#----------------------------------------------
sub setAdapter {
  my($this,$var) = @_;
  return $this->{base}->{adapter} = "\'$var\'";
}
#----------------------------------------------
sub setSeverity {
  my($this,$var) = @_;
  return $this->{base}->{severity} = $var;
}
#----------------------------------------------
sub setClass {
  my($this,$var) = @_;
  return $this->{base}->{class} = $var;
}
#----------------------------------------------
sub setMessage {
  my($this,$var) = @_;
  return $this->{base}->{msg} = "\'$var\'";
}
#----------------------------------------------
sub setSlotValue{
  my ($this,$key,$value) = @_;
  $this->{$key} = $value;
}
#----------------------------------------------
sub deleteSlotValue{
  my ($this,$key) = @_;
  delete $this->{$key};
}
#----------------------------------------------
sub deleteAllSlotValues{
  my ($this) = @_;
  foreach $key (%$this){
    delete $this->{$key};
  }
}
#----------------------------------------------
sub print{
  my($this) = shift;
  foreach $k (sort keys %{ $this->{base} }){
     print "$k = $this->{base}->{$k}\n";
  }
  foreach $k1 (sort keys %$this){
    if($k1 ne base){
#    if($k1 ne $this->{base}){
     print "$k1 = $this->{$k1}\n";
    }
  }
}
#----------------------------------------------
#  This subroutine builds and sends the TEC messages via
#  a TCP stream socket.
#
sub sendMessage{
   my($this) = shift;
   my($mess,$TEC_addr,$port);

  $TEC_addr = inet_aton($this->{base}->{server});

  #--- If hostname slot not set, set as localhost with corresponding
  #    IP address origin
  if(!$this->{hostname}){
     #--- Get hostname of the system ---
    my($name,$aliases,$addrtype,$length,@addrs) = gethostbyname("localhost");
    $this->setSlotValue("hostname",$name);
     #--- Perform DNS lookup to grab IP address of the localhost
    ($name,$aliases,$addrtype,$length,@addrs) = gethostbyname($name);
    ($a,$b,$c,$d) = unpack('C4',$addrs[0]);
    $this->setSlotValue("origin","$a.$b.$c.$d");
  }
  $port = getPortNum($TEC_addr);

  #--- Build TEC message ---
  $mess = "$this->{base}->{class};\nsource=$this->{base}->{adapter};\n";
  $mess .= "severity=$this->{base}->{severity};\nmsg=$this->{base}->{msg};\n";

    #--- append all other slot values to the message ---
  foreach $key (keys %$this){
    next if $key eq 'base';
    $mess.="$key=$this->{$key};\n";
  }
    #--- append message terminator ---
  $mess .="END\n";

  $str_len = length($mess);
  $msg_len = $str_len + 1;

    #--- create socket and send data ---
  $sock_addr = sockaddr_in($port,$TEC_addr);
  socket(SOCKET,PF_INET,SOCK_STREAM,0) ||
    die "Can't create TEC stream socket: $!";
  connect(SOCKET,$sock_addr) || die
     "Can't connect to TEC server: $!";

  #--- Header variables from AEF C code with values implied from sniffer
  #    trace
  #--- Ensure transmision size does not exceed $HDR_HEADER_LENGTH
  #    (this is what the AEF code does!?)

  #--- Determine if message length is greater than 2k bytes. If so, split
  #    it into
  #    two packets.
  if($msg_len > $HDR_HEADER_LENGTH) {
    $sub1 = substr($mess,0,$HDR_HEADER_LENGTH);
    $pmess=pack("a8
N7a$HDR_HEADER_LENGTH","<START>>",0,0,0,0,0,$HDR_HEADER_LENGTH,$HDR_HEADER_LENGTH,$sub1);

    send(SOCKET,$pmess,0);
    $sub1 = substr($mess,$HDR_HEADER_LENGTH+1);
    $len = $msg_len - $HDR_HEADER_LENGTH;
    $pmess=pack("a$len c",$sub1,0x01);
  }
  else{
    $pmess=pack("a8 N7 a$str_len
c","<START>>",0,0,0,0,0,$msg_len,$msg_len,$mess,0x01);
    send(SOCKET,$pmess,0);
  }
  close SOCKET;
}

#----------------------------------------------
#  Sets up the UDP socket connection necessary to
#  query the TEC server for the port to contact it with.
#  Subroutine returns the port # on which to contact
#  TEC for a TCP stream connection.
#
sub getPortNum{
  my($TEC_addr) = shift;
  my($id,$mtype,$accept_status,$auth_type,$auth_len,$call_status,$port);
  my($mess,$reply,$sock_addr);

    #-- Create socket to host on RPC port 111 ---
  $sock_addr = sockaddr_in(111,$TEC_addr);
  socket(SOCKET,PF_INET,SOCK_DGRAM,0) ||
    die "Can't create RPC socket: $!";

  $mess=buildRPCDatagram();

    #-- send datagram and receive reply ---
  send(SOCKET,$mess,0,$sock_addr);
  recv(SOCKET,$reply,256,0);
  close SOCKET;

    #--- decode message ---
  ($id,$mtype,$accept_status,$auth_type,$auth_len,$call_status,$port) =
unpack(N7,$reply);

    #--- check for successfull status values ---
  if($accept_status!=0 && $call_status !=0) {
    print "Error in retrieving port address\n";
    exit 1;
  }
  return $port;
}

#----------------------------------------------
#  Builds the UDP packet necessary to query the TEC server
#  for the port to contact it with. This packet emulates
#  a specific RPC call as performed by the Tivoli
#  supplied postemsg code.
#
sub buildRPCDatagram{

  my($id,$type,$rpcvers,$prog,$vers,$proc,$message);
  my($prognum,$tec_ver,$proto);
    #--- Create RPC header ---
  $id = 12345;
  $type = 0; # RPC call
  $rpcvers = 2;
  $prog = 0x0186a0;
  $vers = 2;
  $proc = 3;
  $message = pack('N6',$id,$type,$rpcvers,$prog,$vers,$proc);
   #--- Use null authentication string ---
  $message.=pack(N4,0,0,0,0);

   #--- Add Procedure call data ---
  $prognum = 100033057;
  $tec_ver = 1;
  $proto = 6;

  return $message.=pack('N3 x2',$prognum,$tec_ver,$proto);
}



