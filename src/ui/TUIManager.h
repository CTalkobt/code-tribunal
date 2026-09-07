#ifndef TRIBUNAL_TUI_MANAGER_H
#define TRIBUNAL_TUI_MANAGER_H

/**
 * ui/TUIManager.h - Terminal User Interface Manager
 *
 * Phase 4.2: Text-based UI for debate execution and result presentation
 *
 * Handles:
 * - Welcome/setup prompts
 * - Debate progress display
 * - Round-by-round results
 * - Final verdict presentation
 * - Error reporting
 */

#pragma once

#include "../core/types.h"
#include "../core/Council.h"
#include "QueryClassifier.h"
#include <string>
#include <vector>
#include <memory>

namespace tribunal {
namespace ui {

/**
 * TUIManager - Terminal User Interface
 *
 * Manages all text output and user interaction for debate system.
 * Formats results for human-readable terminal display.
 *
 * Not thread-safe; must be called from single thread.
 */
class TUIManager {
public:
    /**
     * Constructor
     */
    TUIManager();

    /**
     * print_welcome - Display welcome banner
     */
    void print_welcome();

    /**
     * print_configuration - Show debate configuration
     *
     * @param config  Configuration being used
     */
    void print_configuration(const core::Configuration& config);

    /**
     * print_query_classification - Show classified query type
     *
     * @param classification  Result from QueryClassifier
     * @param original_query  User's original query text
     */
    void print_query_classification(
        const QueryClassification& classification,
        const std::string& original_query
    );

    /**
     * print_round_start - Display beginning of debate round
     *
     * @param round_number  Which round (0-based)
     * @param total_rounds  Total rounds to run
     * @param active_analysts  How many analysts active
     */
    void print_round_start(int round_number, int total_rounds, int active_analysts);

    /**
     * print_round_result - Show round outcome
     *
     * @param round_number  Which round
     * @param winner_id     Winning analyst
     * @param winner_role   Winner's role name
     * @param winner_score  Winner's score
     */
    void print_round_result(int round_number, int winner_id,
                           const std::string& winner_role, int winner_score);

    /**
     * print_pruning - Display pruned analysts
     *
     * @param pruned_ids    Analyst IDs that were removed
     * @param pruned_count  Total pruned this round
     */
    void print_pruning(const std::vector<int>& pruned_ids, int pruned_count);

    /**
     * print_final_result - Display debate winner and summary
     *
     * @param result  DebateResult from CouncilOrchestrator
     */
    void print_final_result(const core::DebateResult& result);

    /**
     * print_analyst_stats - Show detailed statistics
     *
     * @param stats         Vector of all analyst statistics
     * @param winner_id     ID of winning analyst (-1 if none)
     */
    void print_analyst_stats(const std::vector<core::AnalystStats>& stats,
                            int winner_id = -1);

    /**
     * print_error - Display error message
     *
     * @param error_msg  Description of error
     */
    void print_error(const std::string& error_msg);

    /**
     * print_status - Display status message
     *
     * @param status_msg  Informational message
     */
    void print_status(const std::string& status_msg);

private:
    /* Helper methods for formatting */
    std::string format_role(core::AnalystRole role) const;
    std::string format_query_type(core::QueryType type) const;
    void print_separator(char ch = '=', int width = 80);
};

}  /* namespace ui */
}  /* namespace tribunal */

#endif /* TRIBUNAL_TUI_MANAGER_H */
