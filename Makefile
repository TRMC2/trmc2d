#
# Makefile for the tempd temperature daemon.
#


########################################################################
# Commands and options.

# Comment-out if libreadline is not available.
WITH_READLINE = yes

CC=gcc
CFLAGS=-ggdb -Wall -Wextra
CFLAGS_SHARED=$(CFLAGS) -fPIC -shared -nostartfiles
LDFLAGS=

# This is the place to add -I or -L options if you need them to find
# libtrmc2. For example, if libtrmc2 is in /usr/local you would use:
#CFLAGS+=-I/usr/local/include
#LDFLAGS=-L/usr/local/lib


########################################################################

OBJS = tempd.o shell.o io.o interpreter.o parse.o constants.o plugin.o
LDLIBS = -ltrmc2 -ldl -lm

ifdef WITH_READLINE
    CFLAGS += -DUSE_READLINE
    LDLIBS += -lreadline -ltermcap
endif


########################################################################
# Rules.

all:			tempd interpolate-linear.so

# This is the main binary
tempd:			$(OBJS)
				$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Interpolation plugin based on GSL
interpolate.so:	LDLIBS=-lgsl -lgslcblas

tags:			*.[ch]
				ctags $^

%.o:			%.c
				$(CC) $(CFLAGS) -c $<

%.so:			%.c
				$(CC) $(CFLAGS_SHARED) $< $(LDLIBS) -o $@

clean:
				rm -f tempd tags *.o *.so core.*


########################################################################
# Dependencies.

constants.o:	constants.h parse.h
interpreter.o:	parse.h constants.h interpreter.h io.h
io.o:			io.h
parse.o:		parse.h
tempd.o:		parse.h interpreter.h io.h shell.h
shell.o:		constants.h parse.h interpreter.h io.h shell.h
plugin.o:		plugin.h
