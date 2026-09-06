#ifndef TRIBUNAL_ANALYST_H
#define TRIBUNAL_ANALYST_H

/**
 * core/Analyst.h - Analyst State Machine
 *
 * Phase 3.1: Individual analyst lifecycle and state management
 *
 * Each Analyst represents one LLM-based expert in the debate council.
 * Manages:
 * - Role and model identity
 * - Responses across all debate rounds
 * - Contribution tracking (for scoring/pruning)
 * - Lifecycle (active, idle, pruned)
 */

#pragma once

#include "types.h"
#include "../llm/LLMClient.h"
#include <vector>
#include <string>
#include <memory>

namespace tribunal {
namespace core {

/**
 * Analyst - Individual expert in the debate council
 *
 * Represents one LLM-based analyst with a specific role.
 * Executes queries each round and tracks contributions.
 *
 * State transitions:
 *   CREATED → ACTIVE → IDLE → PRUNED
 *   or
 *   CREATED → ACTIVE → COMPLETE
 */
class Analyst {
public:
    /**
     * Constructor - Create analyst with given role and model
     *
     * @param role          Which analyst type (Security, Performance, etc.)
     * @param model         LLM model name (e.g., "llama3.2")
     * @param system_prompt System prompt (defines analyst's focus)
     */
    Analyst(
        AnalystRole role,
        const std::string& model,
        const std::string& system_prompt
    );

    /**
     * execute_round - Run this analyst for one debate round
     *
     * Sends a prompt to the LLM and stores the response.
     *
     * @param llm_client    LLM client to query
     * @param prompt        User prompt for this round
     * @return  true if successful, false if error
     */
    bool execute_round(
        llm::LLMClient& llm_client,
        const std::string& prompt
    );

    /**
     * get_response - Get this analyst's response from a specific round
     *
     * @param round  Round number (0-based)
     * @return  Response text (empty if no response for that round)
     */
    std::string get_response(int round) const;

    /**
     * record_proposal_adopted - Mark that arbiter used this analyst's code
     *
     * Increases score and resets idle counter.
     */
    void record_proposal_adopted();

    /**
     * record_challenge_won - Mark that a peer conceded to this analyst
     *
     * Increases score and resets idle counter.
     */
    void record_challenge_won();

    /**
     * record_round_activity - Record that analyst participated this round
     *
     * Resets idle counter if analyst had a response.
     *
     * @param had_response  true if analyst provided a response this round
     */
    void record_round_activity(bool had_response);

    /**
     * get_stats - Get contribution statistics
     *
     * @return  AnalystStats struct
     */
    AnalystStats get_stats() const;

    /**
     * mark_pruned - Remove this analyst from active debate
     *
     * Called when contribution score is too low.
     */
    void mark_pruned();

    /**
     * mark_complete - Mark debate as finished for this analyst
     *
     * Called after all rounds are complete.
     */
    void mark_complete();

    /* Accessors */
    AnalystRole get_role() const { return role_; }
    std::string get_role_name() const { return role_name_; }
    std::string get_model() const { return model_; }
    std::string get_system_prompt() const { return system_prompt_; }
    bool is_active() const;
    bool is_complete() const { return complete_; }
    bool is_pruned() const { return stats_.pruned; }
    int get_idle_rounds() const { return stats_.rounds_since_contrib; }
    int get_total_score() const { return stats_.total_score; }

private:
    /* Identity */
    AnalystRole role_;
    std::string role_name_;
    std::string model_;
    std::string system_prompt_;

    /* Debate history */
    std::vector<std::string> responses_;  /* One per round */
    int round_count_ = 0;

    /* State */
    AnalystStats stats_;
    bool complete_ = false;
    std::string last_error_;
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_ANALYST_H */
