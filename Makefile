#
# Makefile for the tempd temperature daemon.
#


########################################################################
# Commands and options.
#
# This is the place to add -I or -L options if you need them to find
# libtrmc2.

CC=gcc
CFLAGS=-ggdb -W -Wall
LDFLAGS=

#CFLAGS+=-I/usr/local/include
#LDFLAGS=-L/usr/local/lib


########################################################################
# Rules.

# This is the main binary
tempd:			tempd.o io.o interpreter.o parse.o constants.o
				$(CC) $(LDFLAGS) $^ -ltrmc2 -o $@

# Stand-alone shell for the TRMC2
trmc2shell:		trmc2shell.o io.o interpreter.o parse.o constants.o
				$(CC) $(LDFLAGS) $^ -ltrmc2 -lreadline -ltermcap -o $@

tags:			*.[ch]
				ctags $^

%.o:			%.c
				$(CC) $(CFLAGS) -c $<

clean:
				rm -f tempd trmc2shell tags *.o core.*


########################################################################
# Dependencies.

constants.o:	constants.h parse.h
interpreter.o:	parse.h constants.h interpreter.h io.h
io.o:			io.h
parse.o:		parse.h
tempd.o:		parse.h interpreter.h io.h
trmc2shell.o:	constants.h parse.h interpreter.h io.h
