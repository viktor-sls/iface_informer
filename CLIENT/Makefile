CC=gcc
CFLAGS=-g -c -Wall -Werror
CPPFLAGS=-I/usr/local/include/libnl3
LDFLAGS=-lnl-3 -lnl-route-3 -livykis -lcjson
SOURCES=$(wildcard *.c)
MAIN=client.c
OBJECTS=$(patsubst %.c,%.o,$(filter-out $(MAIN),$(SOURCES)))
BINARY=client

all: $(BINARY)

$(BINARY): $(MAIN) $(OBJECTS)
	$(CC) $(CPPFLAGS) $^ -o $(BINARY) $(LDFLAGS)

$(OBJECTS): $(SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCES)

clean: 
	rm $(OBJECTS) $(BINARY)
