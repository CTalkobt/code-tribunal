#include "TUIManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace tribunal {
namespace ui {

TUIManager::TUIManager() {
}

void TUIManager::print_welcome() {
    print_separator('=', 80);
    std::cout << "\n  CODE TRIBUNAL - Multi-LLM Code Review\n\n";
    std::cout << "  A council of LLM experts analyzes your code through iterative debate\n";
    std::cout << "  and voting to produce comprehensive, consensus-based review.\n\n";
    print_separator('=', 80);
    std::cout << "\n";
}

void TUIManager::print_configuration(const core::Configuration& config) {
    std::cout << "Configuration:\n";
    std::cout << "  Rounds: " << config.rounds << "\n";
    std::cout << "  Models: ";
    for (size_t i = 0; i < config.models.size() && i < 4; i++) {
        if (i > 0) std::cout << ", ";
        std::cout << config.models[i];
    }
    std::cout << "\n";
    std::cout << "  API: " << config.api_type << "\n";
    std::cout << "  Pruning: " << (config.prune_enabled ? "enabled" : "disabled") << "\n";
    std::cout << "\n";
}

void TUIManager::print_query_classification(
    const QueryClassification& classification,
    const std::string& original_query
) {
    std::cout << "Query Analysis:\n";
    std::cout << "  Type: " << classification.classified_as << "\n";
    std::cout << "  Confidence: " << (int)(classification.confidence * 100) << "%\n";
    std::cout << "  Analysis: " << classification.analysis << "\n";
    std::cout << "\n";
}

void TUIManager::print_round_start(int round_number, int total_rounds,
                                   int active_analysts) {
    print_separator('-', 80);
    std::cout << " ROUND " << (round_number + 1) << " of " << total_rounds
              << " (" << active_analysts << " analysts)\n";
    print_separator('-', 80);
}

void TUIManager::print_round_result(int round_number, int winner_id,
                                    const std::string& winner_role,
                                    int winner_score) {
    std::cout << "  Winner: " << winner_role
              << " (analyst #" << winner_id << ", score: " << winner_score << ")\n";
}

void TUIManager::print_pruning(const std::vector<int>& pruned_ids,
                               int pruned_count) {
    if (pruned_count > 0) {
        std::cout << "  Pruned: " << pruned_count << " analyst(s) [";
        for (size_t i = 0; i < pruned_ids.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << "#" << pruned_ids[i];
        }
        std::cout << "]\n";
    }
}

void TUIManager::print_final_result(const core::DebateResult& result) {
    print_separator('=', 80);
    std::cout << "\n  DEBATE COMPLETE\n\n";

    if (result.final_winner >= 0) {
        std::cout << "  Winner: Analyst #" << result.final_winner
                  << " (score: " << result.final_stats[result.final_winner].total_score
                  << ")\n\n";

        std::cout << "  Final Statistics:\n";
        std::cout << "    Rounds completed: " << result.rounds_completed << "\n";
        std::cout << "    Analysts pruned: " << result.analysts_pruned << "\n";
        std::cout << "    Winner proposals adopted: "
                  << result.final_stats[result.final_winner].proposals_adopted << "\n";
        std::cout << "    Winner challenges won: "
                  << result.final_stats[result.final_winner].challenges_won << "\n";
    } else {
        std::cout << "  No clear winner.\n\n";
    }

    std::cout << "\n  Summary: " << result.summary << "\n\n";
    print_separator('=', 80);
    std::cout << "\n";
}

void TUIManager::print_analyst_stats(const std::vector<core::AnalystStats>& stats,
                                     int winner_id) {
    print_separator('-', 80);
    std::cout << " ANALYST STATISTICS\n";
    print_separator('-', 80);

    std::cout << std::left << std::setw(8) << "ID"
              << std::setw(15) << "Role"
              << std::setw(10) << "Score"
              << std::setw(10) << "Adopted"
              << std::setw(10) << "Challenges"
              << std::setw(10) << "Active"
              << "Idle\n";

    print_separator('-', 80);

    for (size_t i = 0; i < stats.size(); i++) {
        const auto& stat = stats[i];
        std::string marker = (static_cast<int>(i) == winner_id) ? "* " : "  ";

        std::cout << marker << std::left
                  << std::setw(6) << i
                  << std::setw(15) << (stat.pruned ? "(pruned)" : "active")
                  << std::setw(10) << stat.total_score
                  << std::setw(10) << stat.proposals_adopted
                  << std::setw(10) << stat.challenges_won
                  << std::setw(10) << stat.rounds_active
                  << stat.rounds_since_contrib << "\n";
    }

    print_separator('-', 80);
    std::cout << "\n";
}

void TUIManager::print_error(const std::string& error_msg) {
    std::cerr << "\n[ERROR] " << error_msg << "\n\n";
}

void TUIManager::print_status(const std::string& status_msg) {
    std::cout << "[*] " << status_msg << "\n";
}

std::string TUIManager::format_role(core::AnalystRole role) const {
    return core::role_to_string(role);
}

std::string TUIManager::format_query_type(core::QueryType type) const {
    switch (type) {
        case core::QueryType::SecurityAudit:
            return "Security Audit";
        case core::QueryType::Performance:
            return "Performance Analysis";
        case core::QueryType::Correctness:
            return "Correctness Verification";
        case core::QueryType::Style:
            return "Style & Maintainability";
        default:
            return "Code Review";
    }
}

void TUIManager::print_separator(char ch, int width) {
    for (int i = 0; i < width; i++) {
        std::cout << ch;
    }
    std::cout << "\n";
}

}  /* namespace ui */
}  /* namespace tribunal */
