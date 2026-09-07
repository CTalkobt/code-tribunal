#include "Analyst.h"
#include <algorithm>

namespace tribunal {
namespace core {

Analyst::Analyst(
    AnalystRole role,
    const std::string& model,
    const std::string& system_prompt
)
    : role_(role), model_(model), system_prompt_(system_prompt) {
    role_name_ = role_to_string(role);
}

bool Analyst::execute_round(
    llm::LLMClient& llm_client,
    const std::string& prompt
) {
    try {
        llm::PromptRequest req{
            model_,
            system_prompt_,
            prompt,
            0.7f,           /* temperature */
            4096,           /* max_tokens */
            {}              /* stop_sequences */
        };

        llm::PromptResponse resp = llm_client.query(req);

        if (!resp.error.empty()) {
            last_error_ = resp.error;
            responses_.push_back("");
            return false;
        }

        responses_.push_back(resp.text);
        record_round_activity(!resp.text.empty());
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        responses_.push_back("");
        return false;
    }
}

std::string Analyst::get_response(int round) const {
    if (round < 0 || round >= static_cast<int>(responses_.size())) {
        return "";
    }
    return responses_[round];
}

void Analyst::record_proposal_adopted() {
    stats_.proposals_adopted++;
    stats_.total_score += config::SCORE_ADOPTED;
    stats_.rounds_since_contrib = 0;
}

void Analyst::record_challenge_won() {
    stats_.challenges_won++;
    stats_.total_score += config::SCORE_CHALLENGE_WON;
    stats_.rounds_since_contrib = 0;
}

void Analyst::record_round_activity(bool had_response) {
    if (had_response) {
        stats_.rounds_active++;
        stats_.rounds_since_contrib = 0;
    } else {
        stats_.rounds_since_contrib++;
    }
}

AnalystStats Analyst::get_stats() const {
    return stats_;
}

void Analyst::mark_pruned() {
    stats_.pruned = 1;
}

void Analyst::mark_complete() {
    complete_ = true;
}

bool Analyst::is_active() const {
    return stats_.pruned == 0 && !complete_;
}

}  /* namespace core */
}  /* namespace tribunal */
