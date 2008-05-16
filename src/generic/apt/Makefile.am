MAINTAINERCLEANFILES = Makefile.in

localedir = $(datadir)/locale
INCLUDES = -Wall @WERROR@ -I$(top_builddir) -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
LDADD = @LIBINTL@

EXTRA_DIST = README.hier

noinst_LIBRARIES = libgeneric-apt.a

libgeneric_apt_a_SOURCES = \
        acqprogress.cc      \
        acqprogress.h       \
        aptcache.cc         \
        aptcache.h          \
        apt.cc              \
        apt.h               \
        aptitudepolicy.cc   \
        aptitudepolicy.h    \
        apt_undo_group.cc   \
        apt_undo_group.h    \
        config_signal.cc    \
        config_signal.h     \
	download_manager.cc \
	download_manager.h  \
	download_install_manager.cc \
	download_install_manager.h  \
	download_update_manager.cc  \
	download_update_manager.h   \
	download_signal_log.cc \
	download_signal_log.h  \
        infer_reason.cc     \
        infer_reason.h      \
	log.cc		    \
	log.h		    \
        matchers.cc         \
        matchers.h          \
        pkg_acqfile.cc      \
        pkg_acqfile.h       \
        rev_dep_iterator.h
