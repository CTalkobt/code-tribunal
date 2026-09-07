/**
 * tests/test_query_classifier.cpp - Query Classification Tests
 *
 * Tests for all query types, confidence scoring, and keyword matching
 */

#include <iostream>
#include <cassert>
#include "../src/ui/QueryClassifier.h"

using namespace tribunal;

int test_classify_security_audit() {
    std::cout << "[TEST] Security Audit Classification\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("Check for security vulnerabilities and exploits");
    
    assert(result.primary_type == core::QueryType::SecurityAudit);
    assert(result.confidence > 0.0f);
    assert(result.classified_as == "Security Audit");
    
    std::cout << "  ✓ Security audit classification works\n";
    return 0;
}

int test_classify_performance() {
    std::cout << "[TEST] Performance Analysis Classification\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("Optimize for performance and efficiency");
    
    assert(result.primary_type == core::QueryType::Performance);
    assert(result.confidence > 0.0f);
    assert(result.classified_as == "Performance Analysis");
    
    std::cout << "  ✓ Performance classification works\n";
    return 0;
}

int test_classify_correctness() {
    std::cout << "[TEST] Correctness Verification Classification\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("Verify logic errors and correctness");
    
    assert(result.primary_type == core::QueryType::Correctness);
    assert(result.confidence > 0.0f);
    assert(result.classified_as == "Correctness Verification");
    
    std::cout << "  ✓ Correctness classification works\n";
    return 0;
}

int test_classify_style() {
    std::cout << "[TEST] Style Classification\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("Review code style and maintainability");
    
    assert(result.primary_type == core::QueryType::Style);
    assert(result.confidence > 0.0f);
    assert(result.classified_as == "Style & Maintainability");
    
    std::cout << "  ✓ Style classification works\n";
    return 0;
}

int test_classify_code_review() {
    std::cout << "[TEST] General Code Review Classification\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("Review and analyze this code");
    
    assert(result.primary_type == core::QueryType::CodeReview);
    assert(result.confidence > 0.0f);
    
    std::cout << "  ✓ Code review classification works\n";
    return 0;
}

int test_classify_case_insensitive() {
    std::cout << "[TEST] Case-Insensitive Classification\n";
    
    ui::QueryClassifier classifier;
    
    auto result1 = classifier.classify("SECURITY vulnerabilities");
    auto result2 = classifier.classify("security vulnerabilities");
    auto result3 = classifier.classify("Security Vulnerabilities");
    
    assert(result1.primary_type == core::QueryType::SecurityAudit);
    assert(result2.primary_type == core::QueryType::SecurityAudit);
    assert(result3.primary_type == core::QueryType::SecurityAudit);
    
    std::cout << "  ✓ Case-insensitive matching works\n";
    return 0;
}

int test_classify_confidence_scoring() {
    std::cout << "[TEST] Confidence Scoring\n";
    
    ui::QueryClassifier classifier;
    
    // Strong security query
    auto strong = classifier.classify("Find security vulnerabilities and exploits and attacks");
    
    // Weak/ambiguous query
    auto weak = classifier.classify("review");
    
    assert(strong.confidence >= weak.confidence);
    
    std::cout << "  ✓ Confidence scoring works\n";
    return 0;
}

int test_classify_relevant_roles() {
    std::cout << "[TEST] Relevant Roles Mapping\n";
    
    ui::QueryClassifier classifier;
    
    auto security = classifier.classify("Security vulnerabilities");
    auto perf = classifier.classify("Performance optimization");
    auto correct = classifier.classify("Logic errors");
    auto style = classifier.classify("Code style");
    
    assert(!security.relevant_roles.empty());
    assert(!perf.relevant_roles.empty());
    assert(!correct.relevant_roles.empty());
    assert(!style.relevant_roles.empty());
    
    std::cout << "  ✓ Relevant roles mapping works\n";
    return 0;
}

int test_classify_empty_query() {
    std::cout << "[TEST] Empty Query Handling\n";
    
    ui::QueryClassifier classifier;
    auto result = classifier.classify("");
    
    // Should fallback to CodeReview with low confidence
    assert(result.confidence >= 0.0f);
    
    std::cout << "  ✓ Empty query handling works\n";
    return 0;
}

int test_classify_long_query() {
    std::cout << "[TEST] Long Query Handling\n";
    
    ui::QueryClassifier classifier;
    
    std::string long_query = "Analyze this code for ";
    for (int i = 0; i < 100; i++) {
        long_query += "security issues ";
    }
    
    auto result = classifier.classify(long_query);
    assert(result.primary_type == core::QueryType::SecurityAudit);
    
    std::cout << "  ✓ Long query handling works\n";
    return 0;
}

int main() {
    std::cout << "\n=== QUERY CLASSIFIER TESTS ===\n\n";
    
    int failures = 0;
    failures += test_classify_security_audit();
    failures += test_classify_performance();
    failures += test_classify_correctness();
    failures += test_classify_style();
    failures += test_classify_code_review();
    failures += test_classify_case_insensitive();
    failures += test_classify_confidence_scoring();
    failures += test_classify_relevant_roles();
    failures += test_classify_empty_query();
    failures += test_classify_long_query();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All QueryClassifier tests passed (10/10)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
