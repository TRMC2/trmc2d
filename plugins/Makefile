# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makefile for the trmc2d temperature conversion plugins.
#

# The following variables are exported by the top-level make:
#WITH_GSL = yes
PLUGINDIR ?= /usr/local/lib/trmc2d
CC        ?= gcc
CFLAGS    ?= -O2 -ggdb -Wall -Wextra

# Global options.
%.so: CFLAGS  += -fPIC -shared -nostartfiles
%.so: LDLIBS  =
test-plugin: LDLIBS = -ldl
COMPILE = $(CC) $(CFLAGS) $< $(LDLIBS) -o $@

# Plugin-specific options.
ifdef WITH_GSL
    interpolate.so: LDLIBS = -lgsl -lgslcblas -lm
endif
expression.so:  LDLIBS = -lmatheval

# List of plugins to build.
PLUGINS = interpolate.so
ifdef WITH_MATHEVAL
    PLUGINS += expression.so
endif

# Rules.

all:    $(PLUGINS)

ifndef WITH_GSL
    # Compile the version that does not require the GSL.
    interpolate.so: interpolate-linear.c
		$(COMPILE)
endif

%.so:   %.c
		$(COMPILE)

test-plugin: test-plugin.c
		$(COMPILE)

install: $(PLUGINS)
		mkdir -p $(PLUGINDIR)
		install $(PLUGINS) $(PLUGINDIR)

uninstall:
		cd $(PLUGINDIR) && rm $(PLUGINS)
		rmdir --ignore-fail-on-non-empty $(PLUGINDIR)

clean:
		rm -f $(PLUGINS) test-plugin

.PHONY: all clean
