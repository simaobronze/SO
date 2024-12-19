CC = gcc
CFLAGS = -Wall -Wextra -pthread
DEPS = feed.h manager.h utils.h
SRC_FEED = feed.c
SRC_MANAGER = manager.c
EXEC_FEED = feed
EXEC_MANAGER = manager

.PHONY: all feed broker clean clean_fifos clean_all

all: clean_fifos $(EXEC_FEED) $(EXEC_MANAGER)

feed: $(EXEC_FEED)

broker: $(EXEC_MANAGER)

$(EXEC_FEED): $(SRC_FEED) $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SRC_FEED)

$(EXEC_MANAGER): $(SRC_MANAGER) $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SRC_MANAGER)

clean:
	rm -f $(EXEC_FEED) $(EXEC_MANAGER)

clean_fifos:
	find . -type p -exec rm -f {} +

clean_all: clean clean_fifos
