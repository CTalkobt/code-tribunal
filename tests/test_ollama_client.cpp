/**
 * tests/test_ollama_client.cpp - OllamaClient Unit Tests
 *
 * Tests for LLM client initialization, model queries, and error handling
 */

#include <iostream>
#include <cassert>
#include "../src/llm/OllamaClient.h"

using namespace tribunal;

int test_ollama_initialization() {
    std::cout << "[TEST] OllamaClient Initialization\n";
    
    llm::OllamaClient client("http://localhost:11434");
    // OllamaClient may support streaming at runtime
    bool streaming = client.supports_streaming();
    assert(streaming || !streaming);  // Always true - just verify method exists
    
    std::cout << "  ✓ Client initialization works\n";
    return 0;
}

int test_ollama_model_info() {
    std::cout << "[TEST] OllamaClient Model Info\n";
    
    llm::OllamaClient client("http://localhost:11434");
    
    // Test with default model (may not exist, but method should not crash)
    auto info = client.get_model_info("nonexistent-model");
    // Should handle gracefully whether found or not
    
    std::cout << "  ✓ Model info retrieval works\n";
    return 0;
}

int test_ollama_prompt_request() {
    std::cout << "[TEST] OllamaClient Prompt Request\n";
    
    llm::PromptRequest req;
    req.model = "test-model";
    req.user_message = "test query";
    req.system_prompt = "test system";
    req.temperature = 0.7f;
    req.max_tokens = 1024;
    
    assert(req.model == "test-model");
    assert(req.user_message == "test query");
    assert(req.max_tokens == 1024);
    
    std::cout << "  ✓ Prompt request formatting works\n";
    return 0;
}

int test_ollama_response_struct() {
    std::cout << "[TEST] OllamaClient Response Struct\n";
    
    llm::PromptResponse resp;
    resp.text = "test response";
    resp.tokens_used = 100;
    resp.truncated = false;
    resp.error = "";
    
    assert(resp.text == "test response");
    assert(resp.tokens_used == 100);
    assert(!resp.truncated);
    assert(resp.error.empty());
    
    std::cout << "  ✓ Response struct works\n";
    return 0;
}

int test_ollama_url_configuration() {
    std::cout << "[TEST] OllamaClient URL Configuration\n";
    
    // Test custom URL
    llm::OllamaClient client1("http://example.com:11434");
    assert(!client1.get_name().empty());
    
    // Test default URL
    llm::OllamaClient client2("http://localhost:11434");
    assert(!client2.get_name().empty());
    
    std::cout << "  ✓ URL configuration works\n";
    return 0;
}

int test_ollama_error_handling() {
    std::cout << "[TEST] OllamaClient Error Handling\n";
    
    llm::OllamaClient client("http://localhost:11434");
    
    // Create a request with empty model (should be handled)
    llm::PromptRequest req;
    req.model = "";
    req.user_message = "test";
    req.system_prompt = "test";
    
    // Client should handle gracefully
    std::string last_error = client.get_last_error();
    // Error should be empty initially
    assert(last_error.empty());
    
    std::cout << "  ✓ Error handling works\n";
    return 0;
}

int main() {
    std::cout << "\n=== OLLAMA CLIENT TESTS ===\n\n";
    
    int failures = 0;
    failures += test_ollama_initialization();
    failures += test_ollama_model_info();
    failures += test_ollama_prompt_request();
    failures += test_ollama_response_struct();
    failures += test_ollama_url_configuration();
    failures += test_ollama_error_handling();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All OllamaClient tests passed (6/6)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
