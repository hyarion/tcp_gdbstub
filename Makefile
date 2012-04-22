CC ?= clang
CFLAGS += -Wall -pedantic --std=c99 -include src/foobar_target.h -g

all: passive_stub
passive_stub: src/passive_comm.c src/foobar_target.c src/comm.c src/main.c
	$(CC) $(CFLAGS) -o passive_stub src/passive_comm.c src/foobar_target.c src/comm.c src/main.c
clean:
	rm -f passive_stub
.PHONY: all clean
