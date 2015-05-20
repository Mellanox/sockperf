if DOC
DOC_SUBDIR = doc
endif

if TEST
TEST_SUBDIR = tests
endif

if TOOL
TOOL_SUBDIR = tools
endif


dist_doc_DATA = \
	readme \
	authors \
	news \
	version \
	copying
	

SUBDIRS = src ${DOC_SUBDIR} ${TEST_SUBDIR} ${TOOL_SUBDIR}

AM_LDFLAGS = -rdynamic


