/**
 * tests/test_analyst.cpp - Analyst State Machine Tests
 *
 * Tests for analyst lifecycle, response tracking, and scoring
 */

#include <iostream>
#include <cassert>
#include "../src/core/Analyst.h"
#include "../src/core/types.h"

using namespace tribunal;

int test_analyst_creation() {
    std::cout << "[TEST] Analyst Creation\n";
    
    core::Analyst analyst(
        core::AnalystRole::Security,
        "test-model",
        "You are a security expert"
    );
    
    assert(analyst.get_role() == core::AnalystRole::Security);
    assert(analyst.get_model() == "test-model");
    assert(analyst.is_active());
    assert(!analyst.is_pruned());
    assert(!analyst.is_complete());
    
    std::cout << "  ✓ Analyst creation works\n";
    return 0;
}

int test_analyst_scoring() {
    std::cout << "[TEST] Analyst Scoring\n";
    
    core::Analyst analyst(
        core::AnalystRole::Correctness,
        "model",
        "prompt"
    );
    
    auto stats = analyst.get_stats();
    assert(stats.total_score == 0);
    assert(stats.proposals_adopted == 0);
    assert(stats.challenges_won == 0);
    
    // Record proposal adopted
    analyst.record_proposal_adopted();
    stats = analyst.get_stats();
    assert(stats.total_score == core::config::SCORE_ADOPTED);
    assert(stats.proposals_adopted == 1);
    
    // Record challenge won
    analyst.record_challenge_won();
    stats = analyst.get_stats();
    assert(stats.total_score == core::config::SCORE_ADOPTED + core::config::SCORE_CHALLENGE_WON);
    assert(stats.challenges_won == 1);
    
    std::cout << "  ✓ Scoring works\n";
    return 0;
}

int test_analyst_response_tracking() {
    std::cout << "[TEST] Analyst Response Tracking\n";
    
    core::Analyst analyst(
        core::AnalystRole::Performance,
        "model",
        "prompt"
    );
    
    // Record activity
    analyst.record_round_activity(true);
    assert(analyst.get_idle_rounds() == 0);
    
    analyst.record_round_activity(false);
    assert(analyst.get_idle_rounds() == 1);
    
    analyst.record_round_activity(false);
    assert(analyst.get_idle_rounds() == 2);
    
    std::cout << "  ✓ Response tracking works\n";
    return 0;
}

int test_analyst_idle_tracking() {
    std::cout << "[TEST] Analyst Idle Tracking\n";
    
    core::Analyst analyst(
        core::AnalystRole::Style,
        "model",
        "prompt"
    );
    
    // Inactive rounds increment idle counter
    analyst.record_round_activity(false);
    analyst.record_round_activity(false);
    analyst.record_round_activity(false);
    
    assert(analyst.get_idle_rounds() == 3);
    
    // Active round resets it
    analyst.record_round_activity(true);
    assert(analyst.get_idle_rounds() == 0);
    
    std::cout << "  ✓ Idle tracking works\n";
    return 0;
}

int test_analyst_pruning() {
    std::cout << "[TEST] Analyst Pruning\n";
    
    core::Analyst analyst(
        core::AnalystRole::Security,
        "model",
        "prompt"
    );
    
    assert(!analyst.is_pruned());
    assert(analyst.is_active());
    
    analyst.mark_pruned();
    
    assert(analyst.is_pruned());
    assert(!analyst.is_active());
    
    std::cout << "  ✓ Pruning works\n";
    return 0;
}

int test_analyst_completion() {
    std::cout << "[TEST] Analyst Completion\n";
    
    core::Analyst analyst(
        core::AnalystRole::Correctness,
        "model",
        "prompt"
    );
    
    assert(!analyst.is_complete());
    assert(analyst.is_active());
    
    analyst.mark_complete();
    
    assert(analyst.is_complete());
    assert(!analyst.is_active());
    
    std::cout << "  ✓ Completion works\n";
    return 0;
}

int test_analyst_role_names() {
    std::cout << "[TEST] Analyst Role Names\n";
    
    core::Analyst security(core::AnalystRole::Security, "m", "p");
    core::Analyst perf(core::AnalystRole::Performance, "m", "p");
    core::Analyst correct(core::AnalystRole::Correctness, "m", "p");
    core::Analyst style(core::AnalystRole::Style, "m", "p");
    
    assert(!security.get_role_name().empty());
    assert(!perf.get_role_name().empty());
    assert(!correct.get_role_name().empty());
    assert(!style.get_role_name().empty());
    
    std::cout << "  ✓ Role names work\n";
    return 0;
}

int test_analyst_multiple_adopted() {
    std::cout << "[TEST] Multiple Proposals Adopted\n";
    
    core::Analyst analyst(
        core::AnalystRole::Performance,
        "model",
        "prompt"
    );
    
    for (int i = 0; i < 5; i++) {
        analyst.record_proposal_adopted();
    }
    
    auto stats = analyst.get_stats();
    assert(stats.proposals_adopted == 5);
    assert(stats.total_score == 5 * core::config::SCORE_ADOPTED);
    
    std::cout << "  ✓ Multiple proposal tracking works\n";
    return 0;
}

int main() {
    std::cout << "\n=== ANALYST TESTS ===\n\n";
    
    int failures = 0;
    failures += test_analyst_creation();
    failures += test_analyst_scoring();
    failures += test_analyst_response_tracking();
    failures += test_analyst_idle_tracking();
    failures += test_analyst_pruning();
    failures += test_analyst_completion();
    failures += test_analyst_role_names();
    failures += test_analyst_multiple_adopted();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All Analyst tests passed (8/8)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
