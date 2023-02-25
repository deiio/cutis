# Cutis Makefile
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com

DEBUG ?= -g
CFLAGS ?= -O2 -Wall -Werror -DSDS_ABORT_ON_OOM
CCOPT = $(CFLAGS)

OBJ = cutis.o adlist.o ae.o anet.o dict.o sds.o zmalloc.o

PRGNAME = cutis-server

all: cutis-server

adlist.o: adlist.c adlist.h zmalloc.h
ae.o: ae.c ae.h zmalloc.h
anet.o: anet.c anet.h
cutis.o: cutis.c ae.h
dict.o: dict.c dict.h zmalloc.h
sds.o: sds.c sds.h zmalloc.h
zmalloc.o: zmalloc.c zmalloc.h

cutis-server: $(OBJ)
	$(CC) -o $(PRGNAME) $(CCOPT) $(DEBUG) $(OBJ)

%.o: %.c
	$(CC) -c $(CCOPT) $(DEBUG) $<

clean:
	$(RM) -rf $(PRGNAME) *.o
