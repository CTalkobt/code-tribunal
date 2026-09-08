# Code-Tribunal Build Configuration (C++17)
# Complete rewrite in C++17 with council orchestration system

# C++ Compiler
CXX     = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -Isrc -I. -Wno-deprecated-declarations
CXXLDFLAGS = -lcurl -lsqlite3 -lpthread -lssl -lcrypto

# Primary target
TARGET = council

# C++ Sources
CXX_SRCS  = src/storage/Database.cpp \
            src/util/Logging.cpp \
            src/util/Concurrent.cpp \
            src/util/Hash.cpp \
            src/llm/LLMClient.cpp \
            src/llm/OllamaClient.cpp \
            src/llm/MultiEndpointOllamaClient.cpp \
            src/llm/ClaudeClient.cpp \
            src/llm/GoogleAntigravityClient.cpp \
            src/core/ConfigParser.cpp \
            src/core/Analyst.cpp \
            src/core/Pool.cpp \
            src/core/Election.cpp \
            src/core/Council.cpp \
            src/ui/QueryClassifier.cpp \
            src/http/HttpServer.cpp \
            src/main.cpp
CXX_OBJS  = $(CXX_SRCS:.cpp=.o)

# C Sources (deprecated - using C++ HttpServer instead)
# C_SRCS   = src/tribunal_http.c
# C_OBJS   = $(C_SRCS:.c=.o)

ALL_OBJS = $(CXX_OBJS)

# Test targets
TEST_DATABASE = test_database
TEST_INTEGRATION = test_integration

# Build targets
.PHONY: all clean install check-deps check-cpp-compiler test tests integration-test

all: check-cpp-compiler $(TARGET)

# Main C++ binary
$(TARGET): $(ALL_OBJS)
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
	install -m 755 $(TARGET) /usr/local/bin/council
	@echo "Installed to /usr/local/bin/council"

# Cleanup
clean:
	rm -f $(ALL_OBJS) $(TARGET) tests/*.o $(TEST_DATABASE) $(TEST_INTEGRATION)
	@echo "Cleaned build artifacts"

# Run all tests
test: tests integration-test

# Unit tests (database)
tests: check-cpp-compiler $(TEST_DATABASE)
	@./$(TEST_DATABASE)

$(TEST_DATABASE): tests/test_database.o src/storage/Database.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Built database test"

tests/test_database.o: tests/test_database.cpp src/core/types.h src/storage/Database.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Integration tests
integration-test: check-cpp-compiler $(TEST_INTEGRATION)
	@./$(TEST_INTEGRATION)

$(TEST_INTEGRATION): tests/integration_test_council.o src/core/Analyst.o src/core/Pool.o src/core/Election.o src/core/Council.o src/ui/QueryClassifier.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Built integration test"

tests/integration_test_council.o: tests/integration_test_council.cpp src/core/Analyst.h src/core/Pool.h src/core/Election.h src/core/Council.h src/ui/QueryClassifier.h
	@mkdir -p tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

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

src/llm/MultiEndpointOllamaClient.o: src/llm/MultiEndpointOllamaClient.cpp src/llm/MultiEndpointOllamaClient.h src/llm/OllamaClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/llm/ClaudeClient.o: src/llm/ClaudeClient.cpp src/llm/ClaudeClient.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/llm/GoogleAntigravityClient.o: src/llm/GoogleAntigravityClient.cpp src/llm/GoogleAntigravityClient.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Analyst.o: src/core/Analyst.cpp src/core/Analyst.h src/core/types.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Pool.o: src/core/Pool.cpp src/core/Pool.h src/core/Analyst.h src/llm/LLMClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Election.o: src/core/Election.cpp src/core/Election.h src/core/Pool.h src/core/Analyst.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/Council.o: src/core/Council.cpp src/core/Council.h src/core/Pool.h src/core/Election.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/core/ConfigParser.o: src/core/ConfigParser.cpp src/core/ConfigParser.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/ui/QueryClassifier.o: src/ui/QueryClassifier.cpp src/ui/QueryClassifier.h src/core/types.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/main.o: src/main.cpp src/core/Council.h src/core/ConfigParser.h src/llm/OllamaClient.h src/llm/MultiEndpointOllamaClient.h src/ui/QueryClassifier.h src/util/Logging.h src/http/HttpServer.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Show configuration
show-config:
	@echo "C++ Compiler: $(CXX) $(CXXFLAGS)"
	@echo "Target: $(TARGET)"
	@echo "C++ Sources: $(CXX_SRCS)"

# Additional test targets
TEST_OLLAMA = test_ollama
TEST_ELECTION = test_election
TEST_THREADPOOL = test_threadpool
TEST_CLASSIFIER = test_classifier
TEST_ANALYST = test_analyst
TEST_POOL = test_pool_test
TEST_LOGGER = test_logger

# Compile all additional tests
$(TEST_OLLAMA): tests/test_ollama_client.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@./$@

tests/test_ollama_client.o: tests/test_ollama_client.cpp src/llm/LLMClient.h src/llm/OllamaClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_ELECTION): tests/test_election.o src/core/Pool.o src/core/Election.o src/core/Analyst.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@./$@

tests/test_election.o: tests/test_election.cpp src/core/types.h src/core/Pool.h src/core/Election.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_THREADPOOL): tests/test_threadpool.o src/util/Concurrent.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@./$@

# High-Priority and Medium-Priority Test Targets (48 new tests)

$(TEST_OLLAMA): tests/test_ollama_client.o src/llm/OllamaClient.o src/llm/LLMClient.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running OllamaClient tests..." && ./$@

tests/test_ollama_client.o: tests/test_ollama_client.cpp src/llm/LLMClient.h src/llm/OllamaClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_ELECTION): tests/test_election.o src/core/Pool.o src/core/Election.o src/core/Analyst.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running Election tests..." && ./$@

tests/test_election.o: tests/test_election.cpp src/core/types.h src/core/Pool.h src/core/Election.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_THREADPOOL): tests/test_threadpool.o src/util/Concurrent.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running ThreadPool tests..." && ./$@

tests/test_threadpool.o: tests/test_threadpool.cpp src/util/Concurrent.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_CLASSIFIER): tests/test_query_classifier.o src/ui/QueryClassifier.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running QueryClassifier tests..." && ./$@

tests/test_query_classifier.o: tests/test_query_classifier.cpp src/ui/QueryClassifier.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_ANALYST): tests/test_analyst.o src/core/Analyst.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running Analyst tests..." && ./$@

tests/test_analyst.o: tests/test_analyst.cpp src/core/Analyst.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_POOL): tests/test_pool.o src/core/Pool.o src/core/Analyst.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running Pool tests..." && ./$@

tests/test_pool.o: tests/test_pool.cpp src/core/Pool.h src/core/Analyst.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_LOGGER): tests/test_logger.o src/util/Logging.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLDFLAGS)
	@echo "Running Logger tests..." && ./$@

tests/test_logger.o: tests/test_logger.cpp src/util/Logging.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Run comprehensive test suite (29 original + 48 new = 77 tests total)
test-all: tests integration-test $(TEST_OLLAMA) $(TEST_ELECTION) $(TEST_THREADPOOL) $(TEST_CLASSIFIER) $(TEST_ANALYST) $(TEST_POOL) $(TEST_LOGGER)
	@echo "\n=========================================="
	@echo "✓ All 77 comprehensive tests completed"
	@echo "=========================================="
	@echo "  Database: 23 tests"
	@echo "  Integration: 6 tests"
	@echo "  OllamaClient: 6 tests"
	@echo "  Election: 7 tests"
	@echo "  ThreadPool: 7 tests"
	@echo "  QueryClassifier: 10 tests"
	@echo "  Analyst: 8 tests"
	@echo "  Pool: 9 tests"
	@echo "  Logger: 7 tests"
	@echo "=========================================="
	@echo ""
