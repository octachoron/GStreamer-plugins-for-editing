# Note: plugindir is set in configure

##############################################################################
# DONE: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libcontraststretch.la

##############################################################################
# DONE: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libcontraststretch_la_SOURCES = gstgimpcontraststretch.c gstgimpcontraststretch.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libcontraststretch_la_CFLAGS = $(GST_CFLAGS)
libcontraststretch_la_LIBADD = $(GST_LIBS)
libcontraststretch_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libcontraststretch_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstgimpcontraststretch.h
