lib_LTLIBRARIES = librazorback_api.la

noinst_HEADERS = \
api_internal.h \
binary_buffer.h \
block_pool_private.h \
command_and_control.h \
connected_entity_private.h \
fantasia.h \
init.h \
judgment_private.h \
local_cache.h \
messages/core.h \
messages/cnc/core.h \
runtime_config.h \
submission_private.h

librazorback_api_la_SOURCES = \
api.c \
binary_buffer.c \
block.c \
block_pool.c \
block_id.c \
config_file.c \
command_and_control.c \
connected_entity.c \
crypto.c \
daemon.c \
event.c \
fantasia.c \
hash.c \
init.c \
inspector_queue.c \
list.c \
local_cache.c \
log.c \
judgment.c \
metadata.c \
messages/block_submit.c \
messages/cache_req.c \
messages/cache_resp.c \
messages/cnc/bye.c \
messages/cnc/cache_clear.c \
messages/cnc/config_ack.c \
messages/cnc/config_update.c \
messages/cnc/core.c \
messages/cnc/error.c \
messages/cnc/go.c \
messages/cnc/hello.c \
messages/cnc/pause.c \
messages/cnc/paused.c \
messages/cnc/reg_req.c \
messages/cnc/reg_resp.c \
messages/cnc/running.c \
messages/cnc/term.c \
messages/core.c \
messages/inspection.c \
messages/judgment.c \
messages/log.c \
ntvl.c \
queue.c \
queue_list.c \
response_queue.c \
runtime_config.c \
socket.c \
storage.c \
submission.c \
thread.c \
thread_pool.c \
uuids.c

librazorback_api_la_CFLAGS = -DBUILDING_SO
librazorback_api_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@

AM_CPPFLAGS = @API_INCLUDES@ @extra_incl@
 
