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

# subdir,<subdir>
# Called at the end of each makefile in a subdirectory in order to add files
# and further subdirectories to the build list.
define subdir
	$(call target-dirs,$(addprefix $(1)/,$(filter %/,$($(1)-y))))
	$(call target-files,$(addprefix $(1)/,$(filter-out %/,$($(1)-y))))
endef


# target-files,<file-list>
# Called by subdir to add files to the build list.
define target-files
	$(call addfiles,SRC,%.c,$(1))
	$(call addfiles,CPPSRC,%.cpp,$(1))
	$(call addfiles,ASRC,%.S,$(1))
	$(call addfiles,BINSRC,%.bin,$(1))
endef


# target-dirs,<dir-list>
# Called by subdir and the main makefile to add subdirectories to the build
# list.
define target-dirs
	$(foreach dir,$(1),
		$(call target-dir,$(patsubst %/,%,$(dir)))
	)
endef


# addfiles,<variable>,<pattern>,<file-list>
# Called by target-files to add the files that match a filter to a variable.
define addfiles
	$(foreach file,$(filter $(2),$(3)),
		$(1) += $(file)
	)
endef


# target-dir,<dir>
# Called by target-dirs to include a particular directory in the build list.
define target-dir
	subdirs += $(1)
	curdir := $(1)
	include $(1)/Makefile
endef


