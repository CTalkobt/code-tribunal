# Code-Tribunal Build Configuration
# Supports parallel C and C++ compilation for migration phase

# C Compiler (current/stable)
CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -Isrc -I.
LDFLAGS = -lcurl -lsqlite3 -lpthread -lssl -lcrypto

# C++ Compiler (C++17, for migration)
CXX     = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -Isrc -I. -Wno-deprecated-declarations
CXXLDFLAGS = -lcurl -lsqlite3 -lpthread -lssl -lcrypto

# Targets
TARGET      = council      # C version (current default)
TARGET_CXX  = council_cpp  # C++ version (beta)

# C Sources (core logic)
C_SRCS    = src/main.c src/council.c src/ollama.c src/db.c
C_OBJS    = $(C_SRCS:.c=.o)

# C++ Sources (placeholder for Phase 1+; empty for now)
CXX_SRCS  =
CXX_OBJS  = $(CXX_SRCS:.cpp=.o)

# Build targets
.PHONY: all clean install check-deps check-cpp-compiler test

all: check-deps $(TARGET)

# C version (default, stable)
$(TARGET): $(C_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built C version: $@"

# C++ version (beta, for testing during migration)
$(TARGET_CXX): check-cpp-compiler $(CXX_OBJS) $(C_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Built C++ version: $@"

# C object files
%.o: %.c src/council.h
	$(CC) $(CFLAGS) -c -o $@ $<

# C++ object files (when needed in Phase 1+)
%.o: %.cpp src/council.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Dependency checks
check-deps:
	@command -v gcc >/dev/null 2>&1 || \
	  (echo "ERROR: gcc not found. Install build-essential" && exit 1)
	@command -v curl-config >/dev/null 2>&1 || \
	  (echo "ERROR: libcurl-dev not found. Install: apt install libcurl4-openssl-dev" && exit 1)
	@pkg-config --exists sqlite3 2>/dev/null || \
	  (echo "ERROR: libsqlite3-dev not found. Install: apt install libsqlite3-dev" && exit 1)

check-cpp-compiler:
	@command -v g++ >/dev/null 2>&1 || \
	  (echo "ERROR: g++ not found. Install build-essential" && exit 1)
	@$(CXX) -std=c++17 -E - < /dev/null >/dev/null 2>&1 || \
	  (echo "ERROR: C++17 not supported by g++. Requires GCC 7+ or Clang 5+" && exit 1)

# Build both versions (for testing during migration)
all-versions: check-deps check-cpp-compiler $(TARGET) $(TARGET_CXX)
	@echo "Built both C and C++ versions"

# Installation
install: check-deps $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/council
	@echo "Installed C version to /usr/local/bin/council"

install-cpp: check-cpp-compiler $(TARGET_CXX)
	install -m 755 $(TARGET_CXX) /usr/local/bin/council_cpp
	@echo "Installed C++ version to /usr/local/bin/council_cpp"

# Cleanup
clean:
	rm -f $(C_OBJS) $(CXX_OBJS) $(TARGET) $(TARGET_CXX)
	@echo "Cleaned build artifacts"

# Show build configuration
show-config:
	@echo "C Compiler: $(CC) $(CFLAGS)"
	@echo "C++ Compiler: $(CXX) $(CXXFLAGS)"
	@echo "C Target: $(TARGET)"
	@echo "C++ Target: $(TARGET_CXX)"
	@echo "C Sources: $(C_SRCS)"
	@echo "C++ Sources: $(CXX_SRCS)"
