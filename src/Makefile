# Cutis Makefile
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com

DEBUG ?= -g
CFLAGS ?= -O2 -Wall -Werror -DSDS_ABORT_ON_OOM
CCOPT = $(CFLAGS)
INCLUDES ?= -I.

OBJ = commands/command.o    \
	  commands/object.o     \
      data_struct/adlist.o  \
      data_struct/dict.o    \
      data_struct/sds.o     \
      event/ae.o            \
      memory/zmalloc.o      \
      net/anet.o            \
      server/server.o       \
      server/client.o       \
      utils/log.o           \
      utils/string_util.o   \
      cutis.o

PRGNAME = cutis-server

all: cutis-server

commands/command.o: commands/command.c commands/command.h \
                    server/client.h                       \
                    memory/zmalloc.h                      \
                    utils/log.h

commands/object.o: commands/object.c commands/object.h \
                   data_struct/sds.h                   \
                   memory/zmalloc.h					   \
                   server/server.h					   \
                   utils/log.h

data_struct/adlist.o: data_struct/adlist.c data_struct/adlist.h \
                      memory/zmalloc.h

data_struct/dict.o: data_struct/dict.c data_struct/dict.h \
                    memory/zmalloc.h

data_struct/sds.o: data_struct/sds.c data_struct/sds.h \
                   memory/zmalloc.h

event/ae.o: event/ae.c event/ae.h \
            memory/zmalloc.h

memory/zmalloc.o: memory/zmalloc.c memory/zmalloc.h

net/anet.o: net/anet.c net/anet.h

server/client.o: server/client.c server/client.h \
                 data_struct/adlist.h            \
                 data_struct/sds.h               \
                 event/ae.h                      \
                 memory/zmalloc.h                \
                 net/anet.h                      \
                 server/server.h                 \
                 utils/log.h

server/server.o: server/server.c server/server.h \
                 data_struct/adlist.h            \
                 event/ae.h                      \
                 memory/zmalloc.h                \
                 net/anet.h                      \
                 server/client.h                 \
                 utils/log.h

utils/log.o: utils/log.c utils/log.h \
             server/server.h

utils/string_util.o: utils/string_util.c utils/string_util.h

cutis.o: cutis.c         \
         server/server.h \
         utils/log.h     \
         version.h

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
