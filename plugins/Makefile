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

clean:
		rm -f $(PLUGINS)

.PHONY: all clean
