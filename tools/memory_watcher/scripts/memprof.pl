#!/usr/bin/perl
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Given a memwatcher logfile, group memory allocations by callstack.
#
# Usage:
#
#   memprof.pl <logfile>
#
#      logfile -- The memwatcher.logXXXX file to summarize.
#
#
#
# Sample output:
#
# 54,061,617        100.00% AllocationStack::AllocationStack
# 41,975,368        77.64%  malloc
# 11,886,592        21.99%  VirtualAlloc
#  7,168,000        13.26%  v8::internal::OS::Allocate
#  7,168,000        13.26%  v8::internal::MemoryAllocator::AllocateRawMemory
#  5,976,184        11.05%  WebCore::V8Bridge::evaluate
#  5,767,168        10.67%  v8::internal::MemoryAllocator::AllocatePages
#  5,451,776        10.08%  WebCore::V8Proxy::initContextIfNeeded
#  ....
#
#
#
# ********
# Note: The output is currently sorted by decreasing size.
# ********
#

sub process_raw($$) {
  my $file = shift;
  my $filter = shift;

  my %leaks = ();
  my %stackframes = ();

  my $blamed = 0;
  my $bytes = 0;
  my $hits = 0;
  open (LOGFILE, "$file") or die("could not open $file");
  while(<LOGFILE>) {
    my $line = $_;
#print "$line";
    chomp($line);
    if ($line =~ m/([0-9]*) bytes, ([0-9]*) allocs/) {

      # If we didn't find any frames to account this to, log that.
      if ($blamed == 0) {
        $leaks{"UNACCOUNTED"} += $bytes;
      }

#print "START\n";
      #print("stackframe " . $1 . ", " . $2 . "\n");
      $hits = $2;
      $bytes = $1;
      %stackframes = ();   # we have a new frame, clear the list.
      $blamed = 0;         # we haven't blamed anyone yet
    }
    elsif ($line =~ m/Total Bytes:[ ]*([0-9]*)/) {
      $total_bytes += $1;
    }
    elsif ($line =~ m/=============/) {
      next;
    }
    elsif ($line =~ m/[ ]*([\-a-zA-Z_\\0-9\.]*) \(([0-9]*)\):[ ]*([<>_a-zA-Z_0-9:]*)/) {
#      print("junk: " . $line . "\n");
#    print("file: $1\n"); 
#    print("line: $2\n"); 
#    print("function: $3\n"); 
#   
      
      # blame the function
      my $pig = $3;
#      my $pig = $1;

      # only add the memory if this function is not yet on our callstack
      if (!exists $stackframes{$pig}) {  
        $leaks{$pig} += $bytes;
      }

      $stackframes{$pig}++;
      $blamed++;
    }
  }

  # now dump our hash table
  my $sum = 0;
  my @keys = sort { $leaks{$b} <=> $leaks{$a}  }keys %leaks;
  for ($i=0; $i<@keys; $i++) {
    my $key = @keys[$i];
    printf "%11s\t%3.2f%%\t%s\n", comma_print($leaks{$key}), (100* $leaks{$key} / $total_bytes), $key;
    $sum += $leaks{$key};
  }
  printf("TOTAL: %s\n", comma_print($sum));
}

# Insert commas into an integer after each three digits for printing.
sub comma_print {
    my $num = "$_[0]";
    $num =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1,/g;
    return $num;
}

# ----- Main ------------------------------------------------

# Get the command line argument
my $filename = shift;
my $filter = shift;

# Process the file.
process_raw($filename, $filter);
