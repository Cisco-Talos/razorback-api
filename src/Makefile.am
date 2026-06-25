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

lib_LTLIBRARIES = librazorback_api.la
bin_PROGRAMS = rzb-worker rzb-dev

noinst_HEADERS = \
api_internal.h \
block_pool_private.h \
command_and_control.h \
connected_entity_private.h \
dev_mode.h \
fantasia.h \
health_internal.h \
init.h \
inspection.h \
judgment_private.h \
local_cache.h \
nugget_tool.h \
messages/core.h \
messages/cnc/core.h \
runtime_config.h \
submission_private.h \
telemetry.h \
transfer/core.h

librazorback_api_la_SOURCES = \
api.c \
block.c \
block_pool.c \
block_id.c \
command_and_control.c \
config_next.c \
config_file.c \
connected_entity.c \
crypto.c \
daemon.c \
dev_mode.c \
event.c \
fantasia.c \
hash.c \
health.c \
init.c \
inspection.c \
inspector_queue.c \
list.c \
local_cache.c \
lock.c \
log.c \
judgment.c \
json_buffer.c \
message_body.c \
messages_next.c \
metadata.c \
messages/alert_child.c \
messages/alert_primary.c \
messages/block_submit.c \
messages/cache_req.c \
messages/cache_resp.c \
messages/cnc/bye.c \
messages/cnc/cache_clear.c \
messages/cnc/config_ack.c \
messages/cnc/config_update.c \
messages/cnc/error.c \
messages/cnc/go.c \
messages/cnc/hello.c \
messages/cnc/pause.c \
messages/cnc/paused.c \
messages/cnc/reg_req.c \
messages/cnc/reg_resp.c \
messages/cnc/rereg.c \
messages/cnc/running.c \
messages/cnc/term.c \
messages/core.c \
messages/inspection.c \
messages/judgment.c \
messages/output_event.c \
messages/output_inspection.c \
ntvl.c \
nugget.c \
queue.c \
queue_list.c \
response_queue.c \
runtime_next.c \
runtime_config.c \
socket.c \
string_list.c \
submission.c \
thread.c \
thread_pool.c \
timer.c \
transfer/core.c \
transfer/file.c \
transfer/http.c \
transfer/ssh.c \
uuids.c \
telemetry_otel.cc

rzb_worker_SOURCES = \
rzb_worker.c \
nugget_tool.c

rzb_worker_LDADD = librazorback_api.la @OPENTELEMETRY_CPP_LIBS@

rzb_dev_SOURCES = \
rzb_dev.c \
nugget_tool.c

rzb_dev_LDADD = librazorback_api.la @OPENTELEMETRY_CPP_LIBS@

#librazorback_api_la_CFLAGS = -DBUILDING_SO -I$(top_srcdir)/libssh/include -I$(top_srcdir)/include
#librazorback_api_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@ ../libssh/src/libssh.la
librazorback_api_la_CFLAGS = -DBUILDING_SO -I$(top_srcdir)/include
librazorback_api_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@ -Wl,--disable-new-dtags -Wl,-rpath,'$$ORIGIN'
librazorback_api_la_LIBADD = @OPENTELEMETRY_CPP_LIBS@
librazorback_api_la_CPPFLAGS = $(AM_CPPFLAGS) @OPENTELEMETRY_CPP_CFLAGS@

#AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/libssh/include -I$(top_srcdir)/include @extra_incl@
AM_CPPFLAGS = -I$(srcdir)  -I$(top_srcdir)/include @extra_incl@
 
