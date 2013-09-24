#!/usr/bin/perl
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Read a memtrace logfile from stdin and group memory allocations by logical
# code component. The code component is guessed from the callstack, and
# is something like {v8, sqlite, disk cache, skia, etc..}
#
# Usage:
#
#   summary.pl
#
#      [STDIN] -- The memwatcher.logXXXX file to summarize.
#

sub process_stdin() {
  my %leaks = ();
  my $total_bytes = 0;

  while(<STDIN>) {
    my $line = $_;
    chomp($line);
    my $bytes, $loc;
    ($bytes, $loc) = ($line =~ m/[ \t]*([0-9]*)[ \t]*[0-9\.%]*[ \t]*(.*)/);
    chomp($loc);
    while(<STDIN>) {
	my $cont = $_;
        chomp($cont);
        last if $cont =~ m/=====/;
	$loc .= "\n" . $cont;
    }
    my $location_blame = "";

#    print "Found: $bytes, $loc\n";
    
    if ($loc =~ m/v8::internal::Snapshot::Deserialize/) {
      $location_blame = "v8 Snapshot Deserialize";
    } elsif ($loc =~ m/RenderStyle::create/) {
      $location_blame = "RenderStyle::create";
    } elsif ($loc =~ m/v8::internal::OldSpace::SlowAllocateRaw/) {
      $location_blame = "v8 OldSpace";
    } elsif ($loc =~ m/sqlite/) {
      $location_blame = "sqlite";
    } elsif ($loc =~ m/ TransportDIB::Map/) {
      $location_blame = "Shared Memory Backing Store";
    } elsif ($loc =~ m/imagedecoder/) {
      $location_blame = "img decoder";
    } elsif ($loc =~ m/SkBitmap/) {
      $location_blame = "skia";
    } elsif ($loc =~ m/disk_cache/) {
      $location_blame = "disk cache";
    } elsif ($loc =~ m/skia/) {
      $location_blame = "skia";
    } elsif ($loc =~ m/:WSA/) {
      $location_blame = "net";
    } elsif ($loc =~ m/dns/) {
      $location_blame = "net";
    } elsif ($loc =~ m/trunk\\net/) {
      $location_blame = "net";
    } elsif ($loc =~ m/WinHttp/) {
      $location_blame = "WinHttp";
    } elsif ($loc =~ m/:I_Crypt/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/CryptGetTls/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/WinVerifyTrust/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/Cert/) {
      $location_blame = "WinHttpSSL";
    } elsif ($loc =~ m/plugin/) {
      $location_blame = "plugin";
    } elsif ($loc =~ m/NP_/) {
      $location_blame = "plugin";
    } elsif ($loc =~ m/hunspell/) {
      $location_blame = "hunspell";
    } elsif ($loc =~ m/TextCodec/) {
      $location_blame = "fonts";
    } elsif ($loc =~ m/glyph/) {
      $location_blame = "fonts";
    } elsif ($loc =~ m/cssparser/) {
      $location_blame = "webkit css";
    } elsif ($loc =~ m/::CSS/) {
      $location_blame = "webkit css";
    } elsif ($loc =~ m/Arena/) {
      $location_blame = "webkit arenas";
    } elsif ($loc =~ m/WebCore::.*ResourceLoader::addData/) {
      $location_blame = "WebCore *ResourceLoader addData";
    } elsif ($loc =~ m/OnUpdateVisitedLinks/) {
      $location_blame = "OnUpdateVisitedLinks";
    } elsif ($loc =~ m/IPC/) {
      $location_blame = "ipc";
    } elsif ($loc =~ m/trunk\\chrome\\browser/) {
      $location_blame = "browser";
    } elsif ($loc =~ m/trunk\\chrome\\renderer/) {
      $location_blame = "renderer";
    } elsif ($loc =~ m/webcore\\html/) {
      $location_blame = "webkit webcore html";
    } elsif ($loc =~ m/webkit.*string/) {
      $location_blame = "webkit strings";
    } elsif ($loc =~ m/htmltokenizer/) {
      $location_blame = "webkit HTMLTokenizer";
    } elsif ($loc =~ m/javascriptcore/) {
      $location_blame = "webkit javascriptcore";
    } elsif ($loc =~ m/webkit/) {
      $location_blame = "webkit other";
    } elsif ($loc =~ m/safe_browsing/) {
      $location_blame = "safe_browsing";
    } elsif ($loc =~ m/VisitedLinkMaster/) {
      $location_blame = "VisitedLinkMaster";
    } elsif ($loc =~ m/NewDOMUI/) {
      $location_blame = "NewDOMUI";
    } elsif ($loc =~ m/RegistryControlledDomainService/) {
      $location_blame = "RegistryControlledDomainService";
    } elsif ($loc =~ m/URLRequestChromeJob::DataAvailable/) {
      $location_blame = "URLRequestChromeJob DataAvailable";
    } elsif ($loc =~ m/history_publisher/) {
      $location_blame = "history publisher";
    } else {
      $location_blame = "unknown";
    }

    # Surface large outliers in an "interesting" group.
    my $interesting_group = "unknown";
    my $interesting_size = 10000000;  # Make this smaller as needed.
    # TODO(jar): Add this as a pair of shell arguments.
    if ($bytes > $interesting_size && $location_blame eq $interesting_group) {
      # Create a special group for the exact stack that contributed so much.
      $location_blame = $loc;
    }

    $total_bytes += $bytes;
    $leaks{$location_blame} += $bytes;
  }

  # now dump our hash table
  my $sum = 0;
  my @keys = sort { $leaks{$b} <=> $leaks{$a}  }keys %leaks;
  for ($i=0; $i<@keys; $i++) {
    my $key = @keys[$i];
    printf "%11s\t(%3.2f%%)\t%s\n", comma_print($leaks{$key}), (100* $leaks{$key} / $total_bytes), $key;
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

process_stdin();
