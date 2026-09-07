#include "Election.h"
#include <algorithm>
#include <numeric>

namespace tribunal {
namespace core {

VotingSystem::VotingSystem(AnalystPool& pool)
    : pool_(pool) {
}

ElectionRound VotingSystem::run_election_round(int round_number) {
    ElectionRound round;
    round.round_number = round_number;

    /* Collect votes from all active analysts */
    round.votes = generate_votes_for_round(round_number);

    /* Tally results */
    auto score_map = tally_votes(round.votes);

    /* Select winner */
    round.winner_id = select_winner(score_map);

    /* Prune low-contributors */
    int pruned_count = prune_inactive_analysts();
    for (size_t i = 0; i < pool_.analyst_count(); i++) {
        auto* analyst = pool_.get_analyst(i);
        if (analyst && analyst->is_pruned()) {
            round.pruned.push_back(i);
        }
    }

    /* Generate outcome description */
    if (round.winner_id >= 0) {
        auto* winner = pool_.get_analyst(round.winner_id);
        if (winner) {
            round.outcome = "Round " + std::to_string(round_number) +
                          ": " + winner->get_role_name() +
                          " wins (" + std::to_string(pruned_count) + " pruned)";
        }
    }

    election_history_.push_back(round);
    return round;
}

int VotingSystem::prune_inactive_analysts() {
    int pruned_count = 0;

    for (size_t i = config::BASE_ANALYST_COUNT; i < pool_.analyst_count(); i++) {
        auto* analyst = pool_.get_analyst(i);
        if (!analyst || !analyst->is_active()) {
            continue;
        }

        bool should_prune = false;

        /* Prune if idle too long */
        if (analyst->get_idle_rounds() >= config::MAX_IDLE_ROUNDS) {
            should_prune = true;
        }

        /* Prune if low score */
        if (analyst->get_total_score() < config::MIN_CONTRIBUTION_SCORE) {
            should_prune = true;
        }

        if (should_prune) {
            analyst->mark_pruned();
            pruned_count++;
        }
    }

    return pruned_count;
}

std::map<int, int> VotingSystem::tally_votes(const std::vector<Vote>& votes) {
    std::map<int, int> scores;

    for (const auto& vote : votes) {
        scores[vote.votee_id] += vote.score;
    }

    return scores;
}

int VotingSystem::select_winner(const std::map<int, int>& score_map) {
    if (score_map.empty()) {
        return -1;
    }

    int winner_id = -1;
    int max_score = 0;

    for (const auto& [id, score] : score_map) {
        if (score > max_score) {
            max_score = score;
            winner_id = id;
        }
    }

    return winner_id;
}

std::vector<ElectionRound> VotingSystem::get_election_history() const {
    return election_history_;
}

ElectionRound VotingSystem::get_current_round() const {
    if (election_history_.empty()) {
        return ElectionRound();
    }
    return election_history_.back();
}

std::vector<Vote> VotingSystem::generate_votes_for_round(int round_number) {
    std::vector<Vote> votes;

    /* Simple voting: each active analyst votes for the one with best response */
    for (size_t i = 0; i < pool_.analyst_count(); i++) {
        auto* voter = pool_.get_analyst(i);
        if (!voter || !voter->is_active()) {
            continue;
        }

        /* Find best response this round */
        int best_votee = -1;
        size_t best_len = 0;

        for (size_t j = 0; j < pool_.analyst_count(); j++) {
            if (i == j) continue;  /* Don't vote for self */

            auto* votee = pool_.get_analyst(j);
            if (!votee || !votee->is_active()) {
                continue;
            }

            std::string response = votee->get_response(round_number);
            if (response.length() > best_len) {
                best_len = response.length();
                best_votee = j;
            }
        }

        if (best_votee >= 0) {
            Vote vote;
            vote.voter_id = i;
            vote.votee_id = best_votee;
            vote.score = 5;  /* Default vote weight */
            votes.push_back(vote);
        }
    }

    return votes;
}

}  /* namespace core */
}  /* namespace tribunal */
