# Note: plugindir is set in configure

##############################################################################
# DONE: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstaddalpha.la

##############################################################################
# DONE: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstaddalpha_la_SOURCES = gstaddalpha.c gstaddalpha.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstaddalpha_la_CFLAGS = $(GST_CFLAGS)
libgstaddalpha_la_LIBADD = $(GST_LIBS)
libgstaddalpha_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaddalpha_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstaddalpha.h
