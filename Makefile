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

ifdef WITH_READLINE
    trmc2shell.o: CFLAGS += -DUSE_READLINE
endif


########################################################################
# Files common to tempd and trmc2shell.

COMMON_OBJ=io.o interpreter.o parse.o constants.o plugin.o
COMMON_LIBS=-ltrmc2 -ldl -lm


########################################################################
# Rules.

all:			tempd trmc2shell interpolate-linear.so

# This is the main binary
tempd:			tempd.o $(COMMON_OBJ)
				$(CC) $^ $(COMMON_LIBS) -o $@

# Stand-alone shell for the TRMC2
trmc2shell:		trmc2shell.o $(COMMON_OBJ)
				$(CC) $^ $(COMMON_LIBS) -lreadline -ltermcap -o $@

# Interpolation plugin based on GSL
interpolate.so:	LDLIBS=-lgsl -lgslcblas

tags:			*.[ch]
				ctags $^

%.o:			%.c
				$(CC) $(CFLAGS) -c $<

%.so:			%.c
				$(CC) $(CFLAGS_SHARED) $< $(LDLIBS) -o $@

clean:
				rm -f tempd trmc2shell tags *.o *.so core.*


########################################################################
# Dependencies.

constants.o:	constants.h parse.h
interpreter.o:	parse.h constants.h interpreter.h io.h
io.o:			io.h
parse.o:		parse.h
tempd.o:		parse.h interpreter.h io.h plugin.h
trmc2shell.o:	constants.h parse.h interpreter.h io.h plugin.h
plugin.o:		plugin.h
