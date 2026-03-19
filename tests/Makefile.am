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

if HAVE_CHECK
check_PROGRAMS = test_hash test_block_id test_list test_string_list \
	test_uuids test_config_file
TESTS = $(check_PROGRAMS)

TESTS_DIR_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_TESTS_DIR=\"$(abs_srcdir)\"

test_hash_SOURCES = test_hash.c test_support.c ../src/hash.c
test_hash_CFLAGS = $(CHECK_CFLAGS)
test_hash_LDADD = $(CHECK_LIBS) @LIBS@

test_block_id_SOURCES = test_block_id.c test_support.c ../src/hash.c ../src/block_id.c
test_block_id_CFLAGS = $(CHECK_CFLAGS)
test_block_id_LDADD = $(CHECK_LIBS) @LIBS@

test_list_SOURCES = test_list.c test_support.c ../src/list.c ../src/lock.c
test_list_CFLAGS = $(CHECK_CFLAGS)
test_list_LDADD = $(CHECK_LIBS) @LIBS@

test_string_list_SOURCES = test_string_list.c test_support.c ../src/list.c ../src/string_list.c ../src/lock.c
test_string_list_CFLAGS = $(CHECK_CFLAGS)
test_string_list_LDADD = $(CHECK_LIBS) @LIBS@

test_uuids_SOURCES = test_uuids.c test_runtime_support.c ../src/list.c ../src/lock.c ../src/uuids.c
test_uuids_CFLAGS = $(CHECK_CFLAGS)
test_uuids_LDADD = $(CHECK_LIBS) @LIBS@

test_config_file_SOURCES = test_config_file.c test_runtime_support.c ../src/config_file.c
test_config_file_CFLAGS = $(CHECK_CFLAGS)
test_config_file_CPPFLAGS = $(TESTS_DIR_CPPFLAGS)
test_config_file_LDADD = $(CHECK_LIBS) @LIBS@

if HAVE_JV
check_PROGRAMS += test_json_buffer_primitives test_json_buffer_structs \
	test_json_buffer_invalid test_messages_roundtrip
TESTS += test_json_buffer_primitives test_json_buffer_structs \
	test_json_buffer_invalid test_messages_roundtrip

JSON_BUFFER_TEST_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_SCHEMA_DIR=\"$(abs_top_srcdir)/share/razorback\" \
	-DRAZORBACK_JV=\"$(JV)\"

test_json_buffer_primitives_SOURCES = test_json_buffer_primitives.c test_json_buffer_support.c \
	../src/hash.c ../src/block_id.c ../src/list.c ../src/string_list.c ../src/json_buffer.c \
	../src/lock.c ../src/ntvl.c ../src/uuids.c
test_json_buffer_primitives_CFLAGS = $(CHECK_CFLAGS)
test_json_buffer_primitives_CPPFLAGS = $(JSON_BUFFER_TEST_CPPFLAGS)
test_json_buffer_primitives_LDADD = $(CHECK_LIBS) @LIBS@

test_json_buffer_structs_SOURCES = test_json_buffer_structs.c test_json_buffer_support.c \
	../src/hash.c ../src/block_id.c ../src/list.c ../src/string_list.c ../src/json_buffer.c \
	../src/lock.c ../src/ntvl.c ../src/uuids.c
test_json_buffer_structs_CFLAGS = $(CHECK_CFLAGS)
test_json_buffer_structs_CPPFLAGS = $(JSON_BUFFER_TEST_CPPFLAGS)
test_json_buffer_structs_LDADD = $(CHECK_LIBS) @LIBS@

test_json_buffer_invalid_SOURCES = test_json_buffer_invalid.c test_json_buffer_support.c \
	../src/hash.c ../src/block_id.c ../src/list.c ../src/string_list.c ../src/json_buffer.c \
	../src/lock.c ../src/ntvl.c ../src/uuids.c
test_json_buffer_invalid_CFLAGS = $(CHECK_CFLAGS)
test_json_buffer_invalid_CPPFLAGS = $(JSON_BUFFER_TEST_CPPFLAGS)
test_json_buffer_invalid_LDADD = $(CHECK_LIBS) @LIBS@

test_messages_roundtrip_SOURCES = test_messages_roundtrip.c test_json_buffer_support.c \
	../src/hash.c ../src/block_id.c ../src/list.c ../src/string_list.c ../src/json_buffer.c \
	../src/lock.c ../src/ntvl.c ../src/uuids.c ../src/messages/core.c \
	../src/messages/cache_req.c ../src/messages/cnc/config_ack.c
test_messages_roundtrip_CFLAGS = $(CHECK_CFLAGS)
test_messages_roundtrip_CPPFLAGS = $(JSON_BUFFER_TEST_CPPFLAGS)
test_messages_roundtrip_LDADD = $(CHECK_LIBS) @LIBS@
endif
endif

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src @extra_incl@
