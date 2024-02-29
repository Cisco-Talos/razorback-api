lib_LTLIBRARIES = librazorback_api.la

noinst_HEADERS = \
api_internal.h \
block_pool_private.h \
command_and_control.h \
connected_entity_private.h \
fantasia.h \
init.h \
judgment_private.h \
local_cache.h \
runtime_config.h \
submission_private.h

librazorback_api_la_SOURCES = \
alert_queue.c \
api.c \
binary_buffer.c \
block.c \
block_pool.c \
block_id.c \
command_and_control.c \
command_queue.c \
config_file.c \
connected_entity.c \
crypto.c \
daemon.c \
event.c \
fantasia.c \
hash.c \
init.c \
input_queue.c \
inspector_queue.c \
local_cache.c \
judgment.c \
judgment_queue.c \
log.c \
log_queue.c \
metadata.c \
messages.c \
ntvl.c \
queue.c \
queue_list.c \
request_queue.c \
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
 
