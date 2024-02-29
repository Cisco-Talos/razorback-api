lib_LTLIBRARIES = librazorback_api.la

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
console.c \
daemon.c \
hash.c \
input_queue.c \
inspector_queue.c \
local_cache.c \
judgment_queue.c \
log.c \
messages.c \
ntvl.c \
queue.c \
queue_list.c \
request_queue.c \
response_queue.c \
runtime_config.c \
socket.c \
submission.c \
thread.c \
uuids.c

librazorback_api_la_CFLAGS = -DBUILDING_SO
librazorback_api_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@

AM_CPPFLAGS = @API_INCLUDES@ @extra_incl@
 
