/**
 * tests/integration_test_council.cpp - C++ Tribunal Integration Test
 *
 * Phase 5: Validates complete end-to-end debate system
 *
 * Tests:
 * - Analyst pool creation and lifecycle
 * - Election rounds with voting
 * - Query classification
 * - Council orchestration
 * - TUI output formatting
 *
 * This test validates that all Phase 1-4 components integrate correctly.
 */

#include <iostream>
#include <cassert>
#include <memory>

#include "../src/core/types.h"
#include "../src/core/Analyst.h"
#include "../src/core/Pool.h"
#include "../src/core/Election.h"
#include "../src/core/Council.h"
#include "../src/llm/LLMClient.h"
#include "../src/ui/QueryClassifier.h"
#include "../src/ui/TUIManager.h"

using namespace tribunal;

class MockLLMClient : public llm::LLMClient {
public:
    llm::PromptResponse query(const llm::PromptRequest& req) override {
        llm::PromptResponse resp;
        resp.text = "Mock response for: " + req.user_message;
        resp.tokens_used = 100;
        resp.truncated = false;
        resp.error = "";
        return resp;
    }

    llm::PromptResponse query_streaming(
        const llm::PromptRequest& req,
        const llm::StreamCallback& cb
    ) override {
        return query(req);
    }

    std::vector<std::string> available_models() override {
        return {"mock-model-1", "mock-model-2"};
    }

    std::optional<std::string> get_model_info(const std::string& model) override {
        return model + "-info";
    }

    bool is_available() override {
        return true;
    }

    std::string get_name() const override {
        return "MockLLMClient";
    }

    std::string get_provider_type() const override {
        return "mock";
    }

    bool supports_streaming() const override {
        return false;
    }

    bool supports_batch() const override {
        return false;
    }

    std::string get_last_error() const override {
        return "";
    }
};

int test_analyst_creation() {
    std::cout << "[TEST] Analyst Creation\n";

    core::Analyst analyst(
        core::AnalystRole::Security,
        "test-model",
        "You are a security expert"
    );

    assert(analyst.get_role() == core::AnalystRole::Security);
    assert(analyst.get_model() == "test-model");
    assert(analyst.is_active() == true);
    assert(analyst.is_pruned() == false);

    std::cout << "  ✓ Analyst creation works\n";
    return 0;
}

int test_analyst_pool() {
    std::cout << "[TEST] Analyst Pool\n";

    std::vector<std::string> models = {"model1", "model2", "model3", "model4"};
    core::AnalystPool pool(models);

    assert(pool.analyst_count() == 4);
    assert(pool.active_count() == 4);

    auto analyst = pool.get_analyst(0);
    assert(analyst != nullptr);
    assert(analyst->get_role() == core::AnalystRole::Security);

    std::cout << "  ✓ Analyst pool creation works\n";
    return 0;
}

int test_query_classifier() {
    std::cout << "[TEST] Query Classifier\n";

    ui::QueryClassifier classifier;

    /* Test security query */
    auto result = classifier.classify("Check for security vulnerabilities");
    assert(result.primary_type == core::QueryType::SecurityAudit);
    assert(result.confidence > 0.0f);

    /* Test performance query */
    result = classifier.classify("Optimize for performance and efficiency");
    assert(result.primary_type == core::QueryType::Performance);

    /* Test correctness query */
    result = classifier.classify("Verify logic errors and correctness");
    assert(result.primary_type == core::QueryType::Correctness);

    std::cout << "  ✓ Query classification works\n";
    return 0;
}

int test_voting_system() {
    std::cout << "[TEST] Voting System\n";

    std::vector<std::string> models = {"model1", "model2", "model3", "model4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);

    assert(voting.get_round_count() == 0);

    auto election = voting.run_election_round(0);
    assert(election.round_number == 0);
    assert(voting.get_round_count() == 1);

    std::cout << "  ✓ Voting system works\n";
    return 0;
}

int test_council_orchestrator() {
    std::cout << "[TEST] Council Orchestrator\n";

    core::Configuration config;
    config.rounds = 2;
    config.models = {"model1", "model2", "model3", "model4"};

    core::CouncilOrchestrator council(config);
    assert(council.is_initialized() == false);

    auto llm_client = std::make_unique<MockLLMClient>();
    bool init_ok = council.initialize_analysts(
        std::move(llm_client),
        config.models
    );
    assert(init_ok == true);
    assert(council.is_initialized() == true);

    std::cout << "  ✓ Council orchestrator works\n";
    return 0;
}

int test_tui_manager() {
    std::cout << "[TEST] TUI Manager\n";

    ui::TUIManager tui;
    tui.print_welcome();
    tui.print_status("Test status message");

    core::Configuration config;
    tui.print_configuration(config);

    std::cout << "  ✓ TUI manager works\n";
    return 0;
}

int main() {
    std::cout << "\n=== CODE-TRIBUNAL C++17 INTEGRATION TESTS ===\n\n";

    int failures = 0;

    failures += test_analyst_creation();
    failures += test_analyst_pool();
    failures += test_query_classifier();
    failures += test_voting_system();
    failures += test_council_orchestrator();
    failures += test_tui_manager();

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All integration tests passed (6/6)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
