# Copyright (c) 2011-2026 Cisco Systems, Inc.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

#noinst_PROGRAMS= configTest logTest daemonTest
noinst_PROGRAMS= logTest daemonTest configTest

configTest_SOURCES = configTest.c
configTest_LDADD = -L../src -lrazorback_api @LIBS@

logTest_SOURCES = logTest.c
logTest_LDADD = -L../src -lrazorback_api @LIBS@

#mqTest_SOURCES = mqTest.c
#mqTest_LDADD = -L../src -lrazorback_api 

daemonTest_SOURCES = daemonTest.c
daemonTest_LDADD = -L../src -lrazorback_api @LIBS@

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/include @extra_incl@

