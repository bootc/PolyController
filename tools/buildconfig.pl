#!/usr/bin/perl
#
# This file is part of the PolyController firmware source code.
# Copyright (C) 2011 Chris Boot.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#
use strict;
use warnings;
use Cwd;
use File::Basename;

my $root_dir = dirname(dirname(Cwd::abs_path($0)));
my $prg = basename($0);

if (scalar(@ARGV) != 3) {
	print "$prg: Usage: $0 <BOARD> <IMAGE> <OBJDIR>\n";
	exit(1);
}

my $board = $ARGV[0];
my $image = $ARGV[1];
my $objdir = $ARGV[2];

my $board_dir = "$root_dir/board";
my $build_dir = "$root_dir/$objdir";

my $config_h = "$build_dir/config.h";
my $config_mk = "$build_dir/config.mk";
my $config_h_tmp = "$build_dir/config.h.tmp";
my $config_mk_tmp = "$build_dir/config.mk.tmp";

my @cfg_dirs = (
	$board,
	"$board/$image",
);

open(CONFIG_H, ">", $config_h_tmp)
	or die "$prg: cannot open $config_h_tmp: $!";
open(CONFIG_MK, ">", $config_mk_tmp)
	or die "$prg: cannot open $config_mk_tmp: $!";

print CONFIG_H <<EOF;
/*
 * This file is auto-generated. Do not hand-edit, your changes will be lost.
 * Instead, edit the following as required:
 *  board/<BOARD>/config.cfg
 *  board/<BOARD>/<IMAGE>/config.cfg
 *
 * Auto-generated for $board $image.
 */

#define CONFIG_BOARD "$board"
#define CONFIG_BOARD_$board 1
#define CONFIG_IMAGE "$image"
#define CONFIG_IMAGE_$image 1

EOF

print CONFIG_MK <<EOF;
#
# This file is auto-generated. Do not hand-edit, your changes will be lost.
# Instead, edit the following as required:
#  board/<BOARD>/config
#  board/<BOARD>/<IMAGE>/config
#
# Auto-generated for $board $image.
#

CONFIG_BOARD=$board
CONFIG_BOARD_$board=y
CONFIG_IMAGE=$image
CONFIG_IMAGE_$image=y

EOF

for my $cfg_dir (@cfg_dirs) {
	my $cfg_file = "$root_dir/board/$cfg_dir/config.cfg";

	open(CFG, "<", $cfg_file)
		or die "$prg: cannot read $cfg_file: $!";

	while (<CFG>) {
		chomp;

		# Strip out comments
		s/(#.*)$//;

		# Clean up spaces
		s/^\s+//;
		s/\s+$//;

		# Skip blank lines
		next if /^$/;

		if (/^(\w+)=([yn])$/) {
			my $var = $1;
			my $value = $2;
			my $value_h = ($2 eq "y") ? 1 : 0;

			print CONFIG_MK "CONFIG_$var=$value\n";
			print CONFIG_H  "#define CONFIG_$var $value_h\n";
		}
		elsif (/^(\w+)=(\w+)$/) {
			my $var = $1;
			my $value = $2;

			print CONFIG_MK "CONFIG_$var=$value\n";
			print CONFIG_H  "#define CONFIG_$var $value\n";
		}
		elsif (/^(\w+)="(\S+)"$/) {
			my $var = $1;
			my $value = $2;

			print CONFIG_MK "CONFIG_$var=\"$value\"\n";
			print CONFIG_H  "#define CONFIG_$var \"$value\"\n";
		}
		else {
			die "$prg: syntax error: $_\n";
		}
	}

	close(CFG);
}

close(CONFIG_H);
close(CONFIG_MK);

sub same_files {
	my $file1 = shift;
	my $file2 = shift;

	my $ret = system(
		qw(cmp -s), $file1, $file2);

	if ($ret == 0) {
		return 1;
	}

	return 0;
}

if (same_files($config_h, $config_h_tmp) &&
	same_files($config_mk, $config_mk_tmp))
{
	unlink($config_h_tmp);
	unlink($config_mk_tmp);
	exit(0);
}

rename($config_h_tmp, $config_h)
	or die "cannot rename: $!";
rename($config_mk_tmp, $config_mk)
	or die "cannot rename: $!";

print "\n";
print "***\n";
print "*** Configuration has been updated.\n";
print "*** You now need to re-run make to build the software.\n";
print "***\n";
print "\n";

exit(1);

1;
