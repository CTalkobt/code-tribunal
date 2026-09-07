#include "Council.h"
#include <algorithm>
#include <sstream>

namespace tribunal {
namespace core {

CouncilOrchestrator::CouncilOrchestrator(const Configuration& config)
    : config_(config) {
}

bool CouncilOrchestrator::initialize_analysts(
    std::unique_ptr<llm::LLMClient> llm_client,
    const std::vector<std::string>& models
) {
    if (!llm_client || models.empty()) {
        return false;
    }

    llm_client_ = std::move(llm_client);
    pool_ = std::make_unique<AnalystPool>(models);
    voting_system_ = std::make_unique<VotingSystem>(*pool_);

    return true;
}

DebateResult CouncilOrchestrator::run_debate(
    const std::string& prompt,
    int num_rounds
) {
    if (!is_initialized()) {
        DebateResult result;
        result.summary = "Error: Council not initialized";
        return result;
    }

    DebateResult result;
    result.rounds_completed = 0;

    /* Run debate rounds */
    for (int round = 0; round < num_rounds; round++) {
        if (!run_round(prompt, round)) {
            break;
        }
        result.rounds_completed++;
    }

    /* Collect final results */
    return finalize_results();
}

bool CouncilOrchestrator::run_round(
    const std::string& prompt,
    int round_number
) {
    if (!pool_ || !llm_client_ || !voting_system_) {
        return false;
    }

    /* Run all analysts */
    int success_count = pool_->execute_round(*llm_client_, prompt);
    if (success_count <= 0) {
        return false;
    }

    /* Run election */
    ElectionRound election = voting_system_->run_election_round(round_number);

    return true;
}

DebateResult CouncilOrchestrator::finalize_results() {
    DebateResult result;

    if (!pool_) {
        result.summary = "Error: Pool not initialized";
        return result;
    }

    /* Find analyst with highest total score */
    result.final_winner = -1;
    int max_score = -1;

    auto stats = pool_->get_stats();
    for (size_t i = 0; i < stats.size(); i++) {
        if (stats[i].total_score > max_score) {
            max_score = stats[i].total_score;
            result.final_winner = i;
        }
    }

    /* Collect winner's responses */
    if (result.final_winner >= 0) {
        auto* winner = pool_->get_analyst(result.final_winner);
        if (winner) {
            for (int r = 0; r < config_.rounds; r++) {
                std::string response = winner->get_response(r);
                if (!response.empty()) {
                    result.winner_responses.push_back(response);
                }
            }
        }
    }

    /* Count final statistics */
    result.final_stats = pool_->get_stats();
    for (const auto& stat : result.final_stats) {
        if (stat.pruned) {
            result.analysts_pruned++;
        }
    }

    /* Generate summary */
    std::ostringstream oss;
    if (result.final_winner >= 0) {
        auto* winner = pool_->get_analyst(result.final_winner);
        if (winner) {
            oss << "Debate complete. Winner: " << winner->get_role_name()
                << " (score: " << result.final_stats[result.final_winner].total_score
                << ", pruned: " << result.analysts_pruned << ")";
        }
    } else {
        oss << "No clear winner";
    }
    result.summary = oss.str();

    return result;
}

std::vector<ElectionRound> CouncilOrchestrator::get_election_history() const {
    if (voting_system_) {
        return voting_system_->get_election_history();
    }
    return {};
}

bool CouncilOrchestrator::is_initialized() const {
    return pool_ != nullptr && llm_client_ != nullptr && voting_system_ != nullptr;
}

}  /* namespace core */
}  /* namespace tribunal */
