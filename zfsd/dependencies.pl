#!/usr/bin/perl

use strict;

my @dep;
my $f;

if (-f "Makefile.dep")
{
  system ("make clean");
}
open (FO, ">Makefile.dep");

foreach $f (sort <*.h>)
{
  @dep = &headers ($f);
  my $var = uc $f;
  $var =~ s/[.-]/_/g;
  print FO $var, " = $f @dep\n";
}

print FO "\n";

foreach $f (sort <*.c>)
{
  @dep = &headers ($f);
  @dep = map { s/[.-]/_/g; uc "\$($_)"; } @dep;
  my $o = $f;
  $o =~ s/c$/o/;
  print FO "$o: $f @dep\n";
}

close (FO);

sub headers
{
  my ($f) = @_;

  my @d;

  open (FI, "<$f") || return @d;
  while (<FI>)
    {
      chomp;
      if (/^\s*#\s*include\s*"([^"]+)"/)
	{
	  push @d, $1;
	}
    }
  close (FI);

  return @d;
}
