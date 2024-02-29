
lib_LTLIBRARIES = librazorback_api.la

include_HEADERS = \
rzb_alert_api.h \
rzb_alert_util.h \
rzb_api.h \
rzb_api_types.h \
rzb_cache.h \
rzb_client.h \
rzb_collection_api.h \
rzb_detection_api.h \
rzb_global.h \
rzb_intel_api.h \
rzb_intel_global.h \
rzb_workstation_api.h \
rzb_workstation_global.h

librazorback_api_la_SOURCES = \
rzb_api.c \
rzb_client.c \
rzb_alert_api.c \
rzb_alert_util.c \
rzb_cache.c \
rzb_intel_api.c \
rzb_workstation_api.c

librazorback_api_la_CFLAGS = -DBUILDING_SO
librazorback_api_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@
librazorback_api_la_LIBADD = @UTILS_LIBRARIES@

AM_CPPFLAGS = @API_INCLUDES@ @extra_incl@

