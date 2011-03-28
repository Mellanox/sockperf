if DOC
DOC_SUBDIR = doc
endif

if TEST
TEST_SUBDIR = tests
endif

if TOOL
TOOL_SUBDIR = tools
endif


EXTRA_DIST = \
	readme \
	authors \
	news \
	version \
	copying
	

SUBDIRS = src ${DOC_SUBDIR} ${TEST_SUBDIR} ${TOOL_SUBDIR}

ACLOCAL_FLAGS = -I config/m4


