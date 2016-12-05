#
# Makefile for the tempd temperature conversion plugins.
#

# The following variables are exported by the top-level make:
#WITH_GSL = yes
CC       ?= gcc
CFLAGS   ?= -O2 -ggdb -Wall -Wextra

# Global options.
CFLAGS  += -fPIC -shared -nostartfiles
LDLIBS  =

# Plugin-specific options.
interpolate.so: LDLIBS = -lgsl -lgslcblas -lm
expression.so:  LDLIBS = -lmatheval

# List of plugins to build.
PLUGINS = interpolate-linear.so
ifdef WITH_GSL
    PLUGINS += interpolate.so
endif
ifdef WITH_MATHEVAL
    PLUGINS += expression.so
endif

# Rules.

all:    $(PLUGINS)

%.so:   %.c
		$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
		rm -f $(PLUGINS)

.PHONY: all clean
