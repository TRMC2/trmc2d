#
# Makefile for the tempd temperature daemon.
#


########################################################################
# User-accessible settings.

# Comment-out if libreadline is not available.
WITH_READLINE = yes

# Comment-out if libgsl is not available.
WITH_GSL = yes

# Global options.
CC      = gcc
CFLAGS  = -O2 -ggdb -Wall -Wextra
LDFLAGS =

# This is the place to add -I or -L options if you need them to find
# libtrmc2. For example, if libtrmc2 is in /usr/local and the compiler
# does not find it, you would use:
#CFLAGS  += -I/usr/local/include
#LDFLAGS += -L/usr/local/lib

# End of user-accessible settings.
########################################################################

OBJS = tempd.o shell.o io.o interpreter.o parse.o constants.o plugin.o
LDLIBS = -ltrmc2 -ldl -lm
PLUGINS = interpolate-linear.so

# Options for plugins.
%.so:           CFLAGS += -fPIC -shared -nostartfiles
%.so:           LDLIBS =
interpolate.so: LDLIBS = -lgsl -lgslcblas -lm

ifdef WITH_READLINE
    shell.o: CFLAGS += -DUSE_READLINE
    tempd:   LDLIBS += -lreadline -ltermcap
endif

ifdef WITH_GSL
    PLUGINS += interpolate.so
endif


########################################################################
# Rules.

all:    tempd $(PLUGINS)

tempd:  $(OBJS)
		$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

tags:   *.[ch]
		ctags $^

%.o:    %.c
		$(CC) $(CFLAGS) -c $<

%.so:   %.c
		$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
		rm -f tempd tags $(OBJS) $(PLUGINS) core.*

.PHONY: all clean


########################################################################
# Dependencies.

constants.o:    constants.h parse.h
interpreter.o:  parse.h constants.h interpreter.h io.h plugin.h
io.o:           io.h
parse.o:        parse.h
tempd.o:        parse.h interpreter.h io.h shell.h
shell.o:        constants.h parse.h interpreter.h io.h shell.h
plugin.o:       plugin.h
