#
# Makefile for the tempd temperature daemon.
#


########################################################################
# Commands and options.

CC=gcc
CFLAGS=-ggdb -W -Wall
LDFLAGS=

# This is the place to add -I or -L options if you need them to find
# libtrmc2. For example, if libtrmc2 is in /usr/local you would use:
#CFLAGS+=-I/usr/local/include
#LDFLAGS=-L/usr/local/lib


########################################################################
# Files common to tempd and trmc2shell.

COMMON_OBJ=io.o interpreter.o parse.o constants.o plugin.o
COMMON_LIBS=-ltrmc2 -ldl -lm


########################################################################
# Rules.

all:			tempd trmc2shell interpolate-linear.so

# This is the main binary
tempd:			tempd.o $(COMMON_OBJ)
				$(CC) $(COMMON_LIBS) $^ $(LDLIBS) -o $@

# Stand-alone shell for the TRMC2
trmc2shell:		trmc2shell.o $(COMMON_OBJ)
				$(CC) $(COMMON_LIBS) $^ $(LDLIBS) -lreadline -ltermcap -o $@

# Interpolation plugin based on GSL
interpolate.so:	interpolate.c
				$(CC) $(CFLAGS) -shared -nostartfiles -lgsl -lgslcblas $< -o $@

tags:			*.[ch]
				ctags $^

%.o:			%.c
				$(CC) $(CFLAGS) -c $<

%.so:			%.c
				$(CC) $(CFLAGS) -shared -nostartfiles $< -o $@

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
