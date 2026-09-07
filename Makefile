# Code-Tribunal Build Configuration (C++17)
# Complete rewrite in C++17 with council orchestration system
# C source files removed - see git tag 'c-legacy/final' for historical reference

# C++ Compiler
CXX     = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -Isrc -I. -Wno-deprecated-declarations
CXXLDFLAGS = -lcurl -lsqlite3 -lpthread -lssl -lcrypto

# Primary target
TARGET = council_cpp

# C++ Sources
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
            src/ui/TUIManager.cpp \
            src/main_cpp.cpp
CXX_OBJS  = $(CXX_SRCS:.cpp=.o)

# Test sources
TEST_SRCS = tests/test_database.cpp tests/integration_test_council.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGET = test_database
INTEGRATION_TEST = integration_test_council

# Build targets
.PHONY: all clean install check-deps check-cpp-compiler test tests integration-test

all: check-cpp-compiler $(TARGET)

# Main C++ binary
$(TARGET): $(CXX_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Built: $@"

# Dependency checks
check-deps:
	@command -v curl-config >/dev/null 2>&1 || \
	  (echo "ERROR: libcurl-dev not found. Install: apt install libcurl4-openssl-dev" && exit 1)
	@pkg-config --exists sqlite3 2>/dev/null || \
	  (echo "ERROR: libsqlite3-dev not found. Install: apt install libsqlite3-dev" && exit 1)

check-cpp-compiler:
	@command -v g++ >/dev/null 2>&1 || \
	  (echo "ERROR: g++ not found. Install build-essential" && exit 1)
	@$(CXX) -std=c++17 -E - < /dev/null >/dev/null 2>&1 || \
	  (echo "ERROR: C++17 not supported by g++. Requires GCC 7+ or Clang 5+" && exit 1)

# Installation
install: check-cpp-compiler $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/council_cpp
	@echo "Installed C++ version to /usr/local/bin/council_cpp"

# Cleanup
clean:
	rm -f $(CXX_OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET) $(INTEGRATION_TEST)
	@echo "Cleaned build artifacts"

# Unit tests
tests: check-cpp-compiler $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) src/storage/Database.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running database tests..."
	@./$@

tests/test_database.o: tests/test_database.cpp src/core/types.h src/storage/Database.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Integration tests
integration-test: check-cpp-compiler $(INTEGRATION_TEST)
	@echo "Running integration tests..."
	@./$(INTEGRATION_TEST)

tests/integration_test_council.o: tests/integration_test_council.cpp src/core/Analyst.h src/core/Pool.h src/core/Election.h src/core/Council.h src/ui/QueryClassifier.h src/ui/TUIManager.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(INTEGRATION_TEST): tests/integration_test_council.o src/core/Analyst.o src/core/Pool.o src/core/Election.o src/core/Council.o src/ui/QueryClassifier.o src/ui/TUIManager.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Built integration test: $@"

# Object file compilation rules
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

src/main_cpp.o: src/main_cpp.cpp src/core/Council.h src/llm/OllamaClient.h src/ui/QueryClassifier.h src/ui/TUIManager.h src/util/Logging.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Show configuration
show-config:
	@echo "C++ Compiler: $(CXX) $(CXXFLAGS)"
	@echo "Target: $(TARGET)"
	@echo "C++ Sources: $(CXX_SRCS)"
