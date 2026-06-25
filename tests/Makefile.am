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
noinst_PROGRAMS= logTest daemonTest configTest smoke_source_next smoke_inspector_next

configTest_SOURCES = configTest.c
configTest_LDADD = -L../src -lrazorback_api @OPENTELEMETRY_CPP_LIBS@ @LIBS@

logTest_SOURCES = logTest.c
logTest_LDADD = -L../src -lrazorback_api @OPENTELEMETRY_CPP_LIBS@ @LIBS@

#mqTest_SOURCES = mqTest.c
#mqTest_LDADD = -L../src -lrazorback_api 

daemonTest_SOURCES = daemonTest.c
daemonTest_LDADD = -L../src -lrazorback_api @OPENTELEMETRY_CPP_LIBS@ @LIBS@

SMOKE_NEXT_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_SCHEMA_FIXTURE_ROOT=\"$(abs_top_srcdir)/schemas/razorback/fixtures\"

smoke_source_next_SOURCES = smoke_source_next.c ../src/runtime_next.c \
	../src/messages_next.c ../src/message_body.c
smoke_source_next_CPPFLAGS = $(SMOKE_NEXT_CPPFLAGS)
smoke_source_next_LDADD = @LIBS@

smoke_inspector_next_SOURCES = smoke_inspector_next.c ../src/runtime_next.c \
	../src/messages_next.c ../src/message_body.c
smoke_inspector_next_CPPFLAGS = $(SMOKE_NEXT_CPPFLAGS)
smoke_inspector_next_LDADD = @LIBS@

.PHONY: smoke-dispatcher-next

smoke-dispatcher-next: smoke_source_next smoke_inspector_next
	./smoke_source_next
	./smoke_inspector_next

if HAVE_CHECK
check_PROGRAMS = test_hash test_block_id test_list test_string_list \
	test_uuids test_config_file test_timer test_health test_message_body \
	test_messages_next test_config_next test_schema_conformance test_runtime_next
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

test_timer_SOURCES = test_timer.c test_runtime_support.c ../src/list.c ../src/lock.c ../src/thread.c ../src/timer.c
test_timer_CFLAGS = $(CHECK_CFLAGS)
test_timer_LDADD = $(CHECK_LIBS) @LIBS@

test_health_SOURCES = test_health.c test_runtime_support.c \
	../src/list.c ../src/lock.c ../src/thread.c ../src/socket.c ../src/health.c
test_health_CFLAGS = $(CHECK_CFLAGS)
test_health_LDADD = $(CHECK_LIBS) @LIBS@

MESSAGE_BODY_TEST_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_SCHEMA_FIXTURE_DIR=\"$(abs_top_srcdir)/schemas/razorback/fixtures/messages\"

test_message_body_SOURCES = test_message_body.c ../src/message_body.c
test_message_body_CFLAGS = $(CHECK_CFLAGS)
test_message_body_CPPFLAGS = $(MESSAGE_BODY_TEST_CPPFLAGS)
test_message_body_LDADD = $(CHECK_LIBS) @LIBS@

test_config_next_SOURCES = test_config_next.c ../src/config_next.c
test_config_next_CFLAGS = $(CHECK_CFLAGS)
test_config_next_LDADD = $(CHECK_LIBS) @LIBS@

SCHEMA_CONFORMANCE_TEST_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_REPO_ROOT=\"$(abs_top_srcdir)\"

test_schema_conformance_SOURCES = test_schema_conformance.c
test_schema_conformance_CFLAGS = $(CHECK_CFLAGS)
test_schema_conformance_CPPFLAGS = $(SCHEMA_CONFORMANCE_TEST_CPPFLAGS)
test_schema_conformance_LDADD = $(CHECK_LIBS) @LIBS@

MESSAGES_NEXT_TEST_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRAZORBACK_SCHEMA_FIXTURE_ROOT=\"$(abs_top_srcdir)/schemas/razorback/fixtures\"

test_messages_next_SOURCES = test_messages_next.c ../src/messages_next.c \
	../src/message_body.c
test_messages_next_CFLAGS = $(CHECK_CFLAGS)
test_messages_next_CPPFLAGS = $(MESSAGES_NEXT_TEST_CPPFLAGS)
test_messages_next_LDADD = $(CHECK_LIBS) @LIBS@

test_runtime_next_SOURCES = test_runtime_next.c ../src/runtime_next.c \
	../src/messages_next.c ../src/message_body.c
test_runtime_next_CFLAGS = $(CHECK_CFLAGS)
test_runtime_next_CPPFLAGS = $(MESSAGES_NEXT_TEST_CPPFLAGS)
test_runtime_next_LDADD = $(CHECK_LIBS) @LIBS@

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
	test_telemetry_stubs.c \
	../src/hash.c ../src/block_id.c ../src/list.c ../src/string_list.c ../src/json_buffer.c \
	../src/lock.c ../src/ntvl.c ../src/uuids.c ../src/messages/core.c \
	../src/messages/alert_child.c ../src/messages/alert_primary.c \
	../src/messages/block_submit.c ../src/messages/cache_req.c \
	../src/messages/cache_resp.c ../src/messages/inspection.c \
	../src/messages/judgment.c ../src/messages/output_event.c \
	../src/messages/output_inspection.c ../src/messages/cnc/bye.c \
	../src/messages/cnc/cache_clear.c ../src/messages/cnc/config_ack.c \
	../src/messages/cnc/config_update.c ../src/messages/cnc/error.c \
	../src/messages/cnc/go.c ../src/messages/cnc/hello.c \
	../src/messages/cnc/pause.c ../src/messages/cnc/paused.c \
	../src/messages/cnc/reg_req.c ../src/messages/cnc/reg_resp.c \
	../src/messages/cnc/rereg.c ../src/messages/cnc/running.c \
	../src/messages/cnc/term.c
test_messages_roundtrip_CFLAGS = $(CHECK_CFLAGS)
test_messages_roundtrip_CPPFLAGS = $(JSON_BUFFER_TEST_CPPFLAGS)
test_messages_roundtrip_LDADD = $(CHECK_LIBS) @LIBS@
endif
endif

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/src @extra_incl@
