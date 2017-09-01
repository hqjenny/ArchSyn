###################################################################### 
# This is used for parsing Archsyn generated file with_cf_dup_hls.cpp
## Inputs:
## ARGV[0] - top function name 
## ARGV[1] - path to the Archsyn scripts folder
######################################################################


#!/usr/bin/perl
use warnings;
use strict;
use Cwd;
use Time::localtime;
use Time::gmtime;
use File::Compare;
use File::Path qw(rmtree make_path);
use File::Copy;

my $dir = getcwd;
my $fn = "with_cf_dup_hls.cpp";
my $line ="";
my $fn_write = "";
my $func ="";
my $path = "";

my $FUNCBEGIN  = 0;
my $FUNCTCLBEGIN = 0;
my $DIRECTIVEBEGIN = 0;
my $DRIVERBEGIN = 0;
my $FIFOBEGIN = 0;
open FILEW_D, ">$dir\/driver.cpp" or die $!;

if(!open FILE,$fn ){
    print $!;
}
else{
    while(<FILE>){
        $line = $_;
        chomp ($line);

        # Parse func
        if($line =~m /#FUNCBEGIN:(.*)/){
            $func = $1;
            $path = $dir."\/vivado_hls\/".$func;
            print $path;
            if (-e $path) 
            {
            #    rmtree $path;
            }
            make_path $path;
            #copy($dir."\/".$fn, $path."\/".$func.".cpp") or die "Copy failed: $!"; 
            
            open FILEW, ">$path\/$func.cpp" or die $!;
            $FUNCBEGIN = 1;
            print FILEW "\#include \"ap_int.h\"\n";
        }
        if($line =~m /#FUNCEND:(.*)/){
            close FILEW; 
            $FUNCBEGIN = 0;
        }elsif($FUNCBEGIN){
            print FILEW $line."\n";
        }

        # Parse tcl
        if($line =~m /#FUNCTCLEND:(.*)/){
            close FILEW; 
            $FUNCTCLBEGIN = 0;
        }
        if($FUNCTCLBEGIN){

            print FILEW $line."\n";
        }
        if($line =~m /#FUNCTCLBEGIN:(.*)/){
            open FILEW, ">$path\/run_hls.tcl" or die $!;
            if($func eq $ARGV[0]){
                print FILEW "set common_anc_dir $dir\n";
            }
            $FUNCTCLBEGIN = 1;
        }

        # Parse directives
        if($line =~m /#DIRECTIVEEND:(.*)/){
            close FILEW; 
            $DIRECTIVEBEGIN = 0;
            chdir $path;
            system("vivado_hls -f run_hls.tcl");
            chdir $dir;
        }
        if($DIRECTIVEBEGIN){

            print FILEW $line."\n";
        }
        if($line =~m /#DIRECTIVEBEGIN:(.*)/){
            open FILEW, ">$path\/directive.tcl" or die $!;
            $DIRECTIVEBEGIN = 1;
        }

        # Parse driver
        if($line =~m /#DRIVEREND:(.*)/){
            $DRIVERBEGIN = 0;
        }
        if($DRIVERBEGIN){

            print FILEW_D $line."\n";
        }
        if($line =~m /#DRIVERBEGIN:(.*)/){
            $DRIVERBEGIN = 1;
        }

        # Parse fifo
        if($line =~m /#FIFOEND:(.*)/){
            $FIFOBEGIN = 0;
            close FILEW;
        }
        if($FIFOBEGIN){
            $path = $dir."\/vivado_hls\/".$ARGV[0];
            open FILEW, ">>$path\/run_hls.tcl" or die $!;
            print FILEW $line."\n";
        }
        if($line =~m /#FIFOBEGIN:(.*)/){
            $FIFOBEGIN = 1;
        }
 

    }

}

system (" cp -r ".$ARGV[1]."/ip_depo ./vivado_hls");
system (" cp -r ".$ARGV[1]."/iplib ./vivado_hls/ip_depo");
close FILEW_D; 
