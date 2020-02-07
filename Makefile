#  ____     ___ |    / _____ _____
# |  __    |    |___/    |     |
# |___| ___|    |    \ __|__   |     gsKit Open Source Project.
# ----------------------------------------------------------------------
# Copyright 2004 - Chris "Neovanglist" Gilbert <Neovanglist@LainOS.org>
# Licenced under Academic Free License version 2.0
# Review gsKit README & LICENSE files for further details.
#
# examples/font/Makefile - Makefile for "font" example.
#
LC_ALL=en_GB.UTF-8
LANG=en_GB.UTF-8
LANGUAGE=en
EE_BIN  = spcmechanic.elf
EE_OBJS = spcmechanic.o pad.o
EE_LIBS_EXTRA = -lpad -laudsrv -L../libxmp/libxmp-lite-4.5.0/lib  -lxmp-lite
EE_CFLAGS = -std=gnu99 -I../libxmp/libxmp-lite-4.5.0/include/libxmp-lite

all: $(EE_BIN)

run: $(EE_BIN)
	ps2client reset
	sleep 5
	ps2client execee host:$(EE_BIN)
frun: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

clean:
	rm -f $(EE_OBJS) $(EE_BIN) ${EE_BIN}

install: all

include Makefile.pref
include Makefile.global
