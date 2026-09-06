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

# C++ Sources (Phase 1+)
CXX_SRCS  = src/storage/Database.cpp \
            src/util/Logging.cpp \
            src/util/Concurrent.cpp \
            src/util/Hash.cpp \
            src/llm/LLMClient.cpp \
            src/llm/OllamaClient.cpp \
            src/core/Analyst.cpp \
            src/core/Pool.cpp \
            src/core/Election.cpp \
            src/core/Council.cpp \
            src/ui/QueryClassifier.cpp \
            src/ui/TUIManager.cpp
CXX_OBJS  = $(CXX_SRCS:.cpp=.o)

# Test sources
TEST_SRCS = tests/test_database.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGET = test_database

# Build targets
.PHONY: all clean install check-deps check-cpp-compiler test tests

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

# Unit tests
tests: check-cpp-compiler $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) src/storage/Database.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running unit tests..."
	@./$@

tests/test_database.o: tests/test_database.cpp src/core/types.h src/storage/Database.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/storage/Database.o: src/storage/Database.cpp src/storage/Database.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/util/Logging.o: src/util/Logging.cpp src/util/Logging.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/util/Concurrent.o: src/util/Concurrent.cpp src/util/Concurrent.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/util/Hash.o: src/util/Hash.cpp src/util/Hash.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/llm/LLMClient.o: src/llm/LLMClient.cpp src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/llm/OllamaClient.o: src/llm/OllamaClient.cpp src/llm/OllamaClient.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Analyst.o: src/core/Analyst.cpp src/core/Analyst.h src/core/types.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Pool.o: src/core/Pool.cpp src/core/Pool.h src/core/Analyst.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Election.o: src/core/Election.cpp src/core/Election.h src/core/Pool.h src/core/Analyst.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Council.o: src/core/Council.cpp src/core/Council.h src/core/Pool.h src/core/Election.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/ui/QueryClassifier.o: src/ui/QueryClassifier.cpp src/ui/QueryClassifier.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/ui/TUIManager.o: src/ui/TUIManager.cpp src/ui/TUIManager.h src/core/Council.h src/ui/QueryClassifier.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Show build configuration
show-config:
	@echo "C Compiler: $(CC) $(CFLAGS)"
	@echo "C++ Compiler: $(CXX) $(CXXFLAGS)"
	@echo "C Target: $(TARGET)"
	@echo "C++ Target: $(TARGET_CXX)"
	@echo "C Sources: $(C_SRCS)"
	@echo "C++ Sources: $(CXX_SRCS)"
	@echo "Test Target: $(TEST_TARGET)"
