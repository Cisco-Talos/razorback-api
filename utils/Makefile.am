lib_LTLIBRARIES = librazorback_utils.la

include_HEADERS = \
rzb_conf.h \
rzb_network.h \
rzb_thread.h \
rzb_utils.h \
rzb_utils_types.h \
rzb_visibility.h

librazorback_utils_la_SOURCES = \
rzb_conf.c \
rzb_network.c \
rzb_thread.c \
rzb_utils.c

librazorback_utils_la_CFLAGS = -DBUILDING_SO
librazorback_utils_la_LDFLAGS = -version-info @APIVERSION@ @XCCFLAGS@

AM_CPPFLAGS = @UTILS_INCLUDES@ @extra_incl@

