# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makefile for the trmc2d temperature daemon.
#


########################################################################
# User-accessible settings.

# Install locations for the program and its plugins.
INSTALLDIR = /usr/local
BINDIR     = $(INSTALLDIR)/bin
PLUGINDIR  = $(INSTALLDIR)/lib/trmc2d

# Comment-out if libreadline is not available.
# Doing so will disable line editing facilities in shell mode.
WITH_READLINE = yes

# Comment-out if libgsl is not available.
# Doing so will disable all methods but `linear' from interpolate.so.
WITH_GSL = yes

# Comment-out if libmatheval is not available.
# Doing so will disable building expression.so.
WITH_MATHEVAL = yes

# Global options.
CC       = gcc
CPPFLAGS =
CFLAGS   = -std=gnu11 -O2 -ggdb -Wall -Wextra
LDFLAGS  =

# This is only useful if you modified INSTALLDIR or PLUGINDIR:
#CPPFLAGS += -DDEFAULT_PLUGIN_DIR=$(PLUGINDIR)

# This is the place to add -I or -L options if you need them to find
# libtrmc2. For example, if libtrmc2 is in /usr/local and the compiler
# does not find it, you would use:
#CPPFLAGS += -I/usr/local/include
#LDFLAGS  += -L/usr/local/lib

# End of user-accessible settings.
########################################################################

OBJS = trmc2d.o shell.o io.o interpreter.o parse.o constants.o plugin.o
LDLIBS = -ltrmc2 -ldl -lm

ifdef WITH_READLINE
    shell.o: override CPPFLAGS += -DUSE_READLINE
    trmc2d:  LDLIBS += -lreadline -ltermcap
endif

# Get version information.
interpreter.o: override CPPFLAGS += -DVERSION='"$(shell ./get-version.sh)"'

# Export to the `plugins' sub-make.
export CC CPPFLAGS CFLAGS WITH_GSL WITH_MATHEVAL PLUGINDIR


########################################################################
# Rules.

all:    trmc2d plugins

trmc2d: $(OBJS)
		$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

plugins:
		$(MAKE) -C plugins

tags:   *.[ch]
		ctags $^

%.o:    %.c
		$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

install: trmc2d
		mkdir -p $(BINDIR)
		install -s trmc2d $(BINDIR)
		$(MAKE) -C plugins install
		-[ $$(id -u) = 0 ] && \
			install -m 644 trmc2d.service /etc/systemd/system && \
			systemctl enable trmc2d
		@echo "\n*** trmc2d successfully installed. ***"
		@[ $$(id -u) = 0 ] && \
		echo "To start trmc2d now, type: sudo systemctl start trmc2d" || true

uninstall:
		-[ $$(id -u) = 0 ] && systemctl stop trmc2d
		-[ $$(id -u) = 0 ] && systemctl disable trmc2d
		-[ $$(id -u) = 0 ] && rm -f /etc/systemd/system/trmc2d.service
		rm -f $(BINDIR)/trmc2d
		$(MAKE) -C plugins uninstall

clean:
		rm -f trmc2d tags $(OBJS) core.*
		$(MAKE) -C plugins clean

.PHONY: all plugins clean


########################################################################
# Dependencies.

constants.o:    constants.h parse.h
interpreter.o:  parse.h constants.h interpreter.h io.h plugin.h
io.o:           io.h
parse.o:        parse.h
trmc2d.o:       parse.h interpreter.h io.h shell.h
shell.o:        constants.h parse.h interpreter.h io.h shell.h
plugin.o:       plugin.h
