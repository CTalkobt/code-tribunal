#ifndef TRIBUNAL_COUNCIL_H
#define TRIBUNAL_COUNCIL_H

/**
 * core/Council.h - Council Orchestrator Facade
 *
 * Phase 3.4: High-level orchestration of debate process
 *
 * Manages:
 * - Analyst pool initialization and lifecycle
 * - Round execution (debate → voting → pruning)
 * - Configuration and debate state
 * - Results collection and reporting
 */

#pragma once

#include "types.h"
#include "Pool.h"
#include "Election.h"
#include "../llm/LLMClient.h"
#include <vector>
#include <string>
#include <memory>

namespace tribunal {
namespace core {

/**
 * DebateResult - Outcome of complete debate
 *
 * Summarizes all rounds, winning analyst, pruning decisions.
 */
struct DebateResult {
    int final_winner = -1;              /* Analyst ID with highest score */
    std::vector<std::string> winner_responses;  /* Responses from winner */
    int rounds_completed = 0;           /* How many rounds ran */
    int analysts_pruned = 0;            /* Total pruned across all rounds */
    std::vector<AnalystStats> final_stats;  /* Stats for all analysts */
    std::string summary;                /* Human-readable result */
};

/**
 * CouncilOrchestrator - Main debate orchestrator
 *
 * Coordinates:
 * - Initialization (analysts, models, LLM client)
 * - Debate round loop
 * - Election and pruning
 * - Final result collection
 *
 * Usage:
 * ```cpp
 * CouncilOrchestrator council(config);
 * council.initialize_analysts(llm_client, models);
 * DebateResult result = council.run_debate(llm_client, prompt, num_rounds);
 * ```
 */
class CouncilOrchestrator {
public:
    /**
     * Constructor - Initialize orchestrator with configuration
     *
     * @param config  Configuration (rounds, models, thresholds)
     */
    explicit CouncilOrchestrator(const Configuration& config);

    /**
     * initialize_analysts - Set up analyst pool
     *
     * Creates base analysts and LLM client.
     *
     * @param llm_client  LLM client for all queries
     * @param models      Vector of model names (at least 4 for base analysts)
     * @return  true if initialized, false if error
     */
    bool initialize_analysts(
        std::unique_ptr<llm::LLMClient> llm_client,
        const std::vector<std::string>& models
    );

    /**
     * run_debate - Execute complete debate process
     *
     * Loops through configured rounds:
     * 1. Run debate (all analysts query LLM with prompt)
     * 2. Run election (vote and select winners)
     * 3. Prune low-contributors
     * 4. Record results
     *
     * @param prompt       What to analyze (user query)
     * @param num_rounds   How many debate rounds
     * @return  DebateResult with final outcome
     */
    DebateResult run_debate(
        const std::string& prompt,
        int num_rounds
    );

    /**
     * get_analyst_pool - Access underlying pool
     *
     * @return  Pointer to AnalystPool
     */
    AnalystPool* get_pool() { return pool_.get(); }
    const AnalystPool* get_pool() const { return pool_.get(); }

    /**
     * get_election_history - Get all voting results
     *
     * @return  Vector of ElectionRound results
     */
    std::vector<ElectionRound> get_election_history() const;

    /**
     * get_config - Get running configuration
     *
     * @return  Configuration struct
     */
    Configuration get_config() const { return config_; }

    /**
     * is_initialized - Check if ready to run debate
     *
     * @return  true if pool and LLM are ready
     */
    bool is_initialized() const;

private:
    Configuration config_;
    std::unique_ptr<AnalystPool> pool_;
    std::unique_ptr<llm::LLMClient> llm_client_;
    std::unique_ptr<VotingSystem> voting_system_;

    /* Helper methods */
    bool run_round(const std::string& prompt, int round_number);
    DebateResult finalize_results();
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_COUNCIL_H */
