# Note: plugindir is set in configure

##############################################################################
# DONE: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstmaskedunsharp.la

##############################################################################
# DONE: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstmaskedunsharp_la_SOURCES = framearith.c framearith.h gstmaskedunsharp.c gstmaskedunsharp.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstmaskedunsharp_la_CFLAGS = $(GST_CFLAGS)
libgstmaskedunsharp_la_LIBADD = $(GST_LIBS)
libgstmaskedunsharp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstmaskedunsharp_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstmaskedunsharp.h
