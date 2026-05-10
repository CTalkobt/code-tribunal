CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -Isrc
LDFLAGS = -lcurl -lsqlite3 -lpthread

TARGET  = council
SRCS    = src/main.c src/council.c src/ollama.c src/db.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean install check-deps

all: check-deps $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c src/council.h
	$(CC) $(CFLAGS) -c -o $@ $<

check-deps:
	@command -v curl-config  >/dev/null 2>&1 || \
	  (echo "ERROR: libcurl-dev not found. Install: apt install libcurl4-openssl-dev" && exit 1)
	@pkg-config --exists sqlite3 2>/dev/null || \
	  (echo "ERROR: libsqlite3-dev not found. Install: apt install libsqlite3-dev" && exit 1)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/council
	@echo "Installed to /usr/local/bin/council"

clean:
	rm -f $(OBJS) $(TARGET)
