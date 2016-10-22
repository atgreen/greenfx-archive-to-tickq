bin_PROGRAMS = archive-to-tickq

# We ship the git tree around because it includes version info, and is
# pretty small.
EXTRA_DIST = .git

archive_to_tickq_CPPFLAGS = $(AMQ_CFLAGS)

archive_to_tickq_LDADD = -lcurl $(AMQ_LIBS) -ljson-c
archive_to_tickq_SOURCES = main.cc
