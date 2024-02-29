#noinst_PROGRAMS= configTest logTest daemonTest
noinst_PROGRAMS= logTest daemonTest configTest

configTest_SOURCES = configTest.c
configTest_LDADD = -L../src -lrazorback_api 

logTest_SOURCES = logTest.c
logTest_LDADD = -L../src -lrazorback_api 

#mqTest_SOURCES = mqTest.c
#mqTest_LDADD = -L../src -lrazorback_api 

daemonTest_SOURCES = daemonTest.c
daemonTest_LDADD = -L../src -lrazorback_api

AM_CPPFLAGS = @API_INCLUDES@ @extra_incl@

