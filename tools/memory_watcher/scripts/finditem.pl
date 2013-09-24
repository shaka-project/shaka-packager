#!/usr/bin/perl
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sub process_raw($$) {
  my $file = shift;
  my $search = shift;

  my %leaks = ();

  my $save = 0;
  my $print = 0;
  my $bytes = 0;
  my $calls = 0;
  my $sum_bytes = 0;
  my $sum_calls = 0;

  open (LOGFILE, "$file") or die("could not open $file");
  while(<LOGFILE>) {
    my $line = $_;
    if ($line =~ m/([0-9]*) bytes, ([0-9]*) allocs/) {
      $save = "";
      $print = 0;
      $bytes = $1;
      $calls = $2;
    }
    elsif ($line =~ m/$search/) {
      $print = 1;
    }
    elsif ($line =~ m/=============/) {
      $save .= $line;
      if ($print) {
        print "$bytes bytes ($calls calls)\n";
        print $save;
        $sum_bytes += $bytes;
        $sum_calls += $calls;
        $save = "";
        $print = 0;
        $calls = 0;
      }
    }
    $save .= $line;
  }
  print("TOTAL: $sum_bytes bytes ($sum_calls calls)\n");
}


# ----- Main ------------------------------------------------

# Get the command line argument
my $filename = shift;
my $search = shift;

# Process the file.
process_raw($filename, $search);
