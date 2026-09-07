/**
 * tests/test_pool.cpp - Analyst Pool Tests
 *
 * Tests for collection management, parallel execution, and queries
 */

#include <iostream>
#include <cassert>
#include "../src/core/Pool.h"

using namespace tribunal;

int test_pool_creation() {
    std::cout << "[TEST] Pool Creation\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    assert(pool.analyst_count() == 4);
    assert(pool.active_count() == 4);
    
    std::cout << "  ✓ Pool creation works\n";
    return 0;
}

int test_pool_get_analyst() {
    std::cout << "[TEST] Pool Get Analyst\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    auto analyst = pool.get_analyst(0);
    assert(analyst != nullptr);
    assert(analyst->get_role() == core::AnalystRole::Security);
    
    auto analyst2 = pool.get_analyst(1);
    assert(analyst2 != nullptr);
    assert(analyst2->get_role() == core::AnalystRole::Performance);
    
    std::cout << "  ✓ Get analyst works\n";
    return 0;
}

int test_pool_get_invalid_analyst() {
    std::cout << "[TEST] Pool Invalid Analyst Access\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    auto analyst = pool.get_analyst(99);
    assert(analyst == nullptr);
    
    std::cout << "  ✓ Invalid access handling works\n";
    return 0;
}

int test_pool_get_by_role() {
    std::cout << "[TEST] Pool Get by Role\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    auto security = pool.get_analyst_by_role(core::AnalystRole::Security);
    assert(security != nullptr);
    assert(security->get_role() == core::AnalystRole::Security);
    
    auto performance = pool.get_analyst_by_role(core::AnalystRole::Performance);
    assert(performance != nullptr);
    assert(performance->get_role() == core::AnalystRole::Performance);
    
    std::cout << "  ✓ Get by role works\n";
    return 0;
}

int test_pool_active_count() {
    std::cout << "[TEST] Pool Active Count\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    assert(pool.active_count() == 4);
    
    // Prune one
    auto analyst = pool.get_analyst(0);
    analyst->mark_pruned();
    
    assert(pool.active_count() == 3);
    
    // Prune another
    analyst = pool.get_analyst(1);
    analyst->mark_pruned();
    
    assert(pool.active_count() == 2);
    
    std::cout << "  ✓ Active count tracking works\n";
    return 0;
}

int test_pool_add_analyst() {
    std::cout << "[TEST] Pool Add Dynamic Analyst\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    assert(pool.analyst_count() == 4);
    
    bool added = pool.add_analyst(
        core::AnalystRole::Security,
        "new-model",
        "New analyst prompt"
    );
    
    assert(added);
    assert(pool.analyst_count() == 5);
    
    std::cout << "  ✓ Add analyst works\n";
    return 0;
}

int test_pool_remove_analyst() {
    std::cout << "[TEST] Pool Remove Analyst\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    assert(pool.analyst_count() == 4);
    
    bool removed = pool.remove_analyst(0);
    assert(removed);
    assert(pool.analyst_count() == 3);
    
    bool removed_invalid = pool.remove_analyst(99);
    assert(!removed_invalid);
    
    std::cout << "  ✓ Remove analyst works\n";
    return 0;
}

int test_pool_get_stats() {
    std::cout << "[TEST] Pool Get Statistics\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    // Record some activity
    auto analyst = pool.get_analyst(0);
    analyst->record_proposal_adopted();
    analyst->record_challenge_won();
    
    auto stats = pool.get_stats();
    assert(stats.size() == 4);
    assert(stats[0].total_score > 0);
    
    std::cout << "  ✓ Get statistics works\n";
    return 0;
}

int test_pool_capacity_limit() {
    std::cout << "[TEST] Pool Capacity Limit\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    
    // Add up to capacity
    int added_count = 0;
    for (int i = 0; i < core::config::MAX_MODELS; i++) {
        bool added = pool.add_analyst(
            core::AnalystRole::Security,
            "model-" + std::to_string(i),
            "prompt"
        );
        if (added) added_count++;
        else break;
    }
    
    // Should have capped at MAX_MODELS
    assert(pool.analyst_count() <= core::config::MAX_MODELS);
    
    std::cout << "  ✓ Capacity limiting works\n";
    return 0;
}

int main() {
    std::cout << "\n=== ANALYST POOL TESTS ===\n\n";
    
    int failures = 0;
    failures += test_pool_creation();
    failures += test_pool_get_analyst();
    failures += test_pool_get_invalid_analyst();
    failures += test_pool_get_by_role();
    failures += test_pool_active_count();
    failures += test_pool_add_analyst();
    failures += test_pool_remove_analyst();
    failures += test_pool_get_stats();
    failures += test_pool_capacity_limit();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All Pool tests passed (9/9)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
