#ifndef TRIBUNAL_POOL_H
#define TRIBUNAL_POOL_H

/**
 * core/Pool.h - Analyst Pool Manager
 *
 * Phase 3.2: Collection and lifecycle management for analyst group
 *
 * Manages:
 * - Static analysts (4 core roles: Security, Performance, Correctness, Style)
 * - Dynamic analysts (spawned at runtime for gap roles)
 * - Parallel execution of all analysts each round
 * - Query by role, model, or index
 */

#pragma once

#include "Analyst.h"
#include "types.h"
#include "../llm/LLMClient.h"
#include <vector>
#include <memory>

namespace tribunal {
namespace core {

/**
 * AnalystPool - Manages collection of analysts
 *
 * Represents the group of LLM experts analyzing the codebase.
 * Scales from 4 base analysts up to MAX_DYNAMIC_ROLES (4) spawned analysts.
 *
 * Thread-safe for parallel round execution.
 */
class AnalystPool {
public:
    /**
     * Constructor - Initialize pool with base analysts
     *
     * Creates 4 static analysts:
     * - Security analyst
     * - Performance analyst
     * - Correctness analyst
     * - Style analyst
     *
     * @param models  Vector of 4 model names for base analysts
     */
    explicit AnalystPool(const std::vector<std::string>& models);

    /**
     * add_analyst - Add dynamic analyst to pool
     *
     * Used when spawning new roles identified during debate.
     *
     * @param role           AnalystRole::Dynamic (for all spawned analysts)
     * @param model          LLM model to use
     * @param system_prompt  Role-specific system prompt
     * @return  true if added, false if at capacity
     */
    bool add_analyst(
        AnalystRole role,
        const std::string& model,
        const std::string& system_prompt
    );

    /**
     * remove_analyst - Remove analyst by index
     *
     * Used for pruning idle analysts.
     *
     * @param index  Index of analyst to remove
     * @return  true if removed, false if invalid index
     */
    bool remove_analyst(size_t index);

    /**
     * execute_round - Run all active analysts in parallel for one round
     *
     * Each analyst independently queries its LLM with the prompt.
     *
     * @param llm_client    LLM client
     * @param prompt        Debate prompt for this round
     * @return  Number of analysts that completed successfully
     */
    int execute_round(
        llm::LLMClient& llm_client,
        const std::string& prompt
    );

    /**
     * get_analyst - Get analyst by index
     *
     * @param index  Analyst index
     * @return  Pointer to analyst, or nullptr if invalid
     */
    Analyst* get_analyst(size_t index);
    const Analyst* get_analyst(size_t index) const;

    /**
     * get_analyst_by_role - Find first analyst with given role
     *
     * @param role  Role to search for
     * @return  Pointer to analyst, or nullptr if not found
     */
    Analyst* get_analyst_by_role(AnalystRole role);

    /**
     * active_count - Number of active (non-pruned) analysts
     *
     * @return  Count of active analysts
     */
    size_t active_count() const;

    /**
     * analyst_count - Total number of analysts (active + pruned)
     *
     * @return  Total count
     */
    size_t analyst_count() const { return analysts_.size(); }

    /**
     * get_stats - Get aggregated statistics for all analysts
     *
     * @return  Vector of AnalystStats (one per analyst, in order)
     */
    std::vector<AnalystStats> get_stats() const;

private:
    std::vector<std::unique_ptr<Analyst>> analysts_;
    size_t base_analyst_count_;  /* Static analysts (never pruned below) */
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_POOL_H */
