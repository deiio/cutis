# Cutis Makefile
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com

DEBUG ?= -g
CFLAGS ?= -O2 -Wall -Werror -DSDS_ABORT_ON_OOM
CCOPT = $(CFLAGS)
INCLUDES ?= -I.

OBJ = data_struct/adlist.o  \
	  data_struct/dict.o 	\
	  data_struct/sds.o 	\
	  event/ae.o 			\
	  memory/zmalloc.o 		\
	  net/anet.o 			\
	  cutis.o

PRGNAME = cutis-server

all: cutis-server

data_struct/adlist.o: data_struct/adlist.c data_struct/adlist.h memory/zmalloc.h
data_struct/dict.o: data_struct/dict.c data_struct/dict.h memory/zmalloc.h
data_struct/sds.o: data_struct/sds.c data_struct/sds.h memory/zmalloc.h
event/ae.o: event/ae.c event/ae.h memory/zmalloc.h
memory/zmalloc.o: memory/zmalloc.c memory/zmalloc.h
net/anet.o: net/anet.c net/anet.h
cutis.o: cutis.c event/ae.h

cutis-server: $(OBJ)
	$(CC) -o $(PRGNAME) $(CCOPT) $(DEBUG) $(OBJ)

%.o: %.c
	$(CC) -c $(CCOPT) -o $@ $(DEBUG) $< $(INCLUDES)

32bit:
	@echo ""
	@echo "WARNING: if it fails under Linux you probably need to install libc6-dev-i386"
	@echo ""
	$(MAKE) CFLAGS="-m32" LDFLAGS="-m32"

clean:
	$(RM) $(PRGNAME) $(OBJ) *.o
