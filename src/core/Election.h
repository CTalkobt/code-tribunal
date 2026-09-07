#ifndef TRIBUNAL_ELECTION_H
#define TRIBUNAL_ELECTION_H

/**
 * core/Election.h - Election and Voting System
 *
 * Phase 3.3: Election voting, pruning decisions, and winner determination
 *
 * Manages:
 * - Vote collection and tallying each round
 * - Pruning low-contribution analysts
 * - Winner selection via aggregate scoring
 * - Election round state (proposal phase, voting phase, results)
 */

#pragma once

#include "types.h"
#include "Analyst.h"
#include "Pool.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace tribunal {
namespace core {

/**
 * Vote - Single vote cast by an analyst
 *
 * Records preference for one analyst's solution over another.
 */
struct Vote {
    int voter_id = -1;              /* Analyst voting */
    int votee_id = -1;              /* Target analyst being voted on */
    int score = 0;                  /* Vote weight (1-10) */
};

/**
 * ElectionRound - Result of one voting round
 *
 * Tracks all votes, decisions, and outcomes.
 */
struct ElectionRound {
    int round_number = 0;           /* Which debate round */
    std::vector<Vote> votes;        /* All votes cast this round */
    std::vector<int> pruned;        /* Analyst IDs removed this round */
    int winner_id = -1;             /* Winning analyst for this round */
    std::string outcome;            /* Human-readable result */
};

/**
 * VotingSystem - Manages election rounds and results
 *
 * Coordinates:
 * - Vote collection from analysts
 * - Tally and decision-making
 * - Pruning of low-contributors
 * - Winner selection via max scoring
 *
 * Thread-safe for concurrent voting.
 */
class VotingSystem {
public:
    /**
     * Constructor - Initialize voting system with analyst pool
     *
     * @param pool  Reference to AnalystPool for voting access
     */
    explicit VotingSystem(AnalystPool& pool);

    /**
     * run_election_round - Execute one election round
     *
     * - Collects votes from all active analysts
     * - Tallies results
     * - Identifies and removes low-contributors (pruning)
     * - Selects winner
     *
     * @param round_number  Which debate round this election is for
     * @return  ElectionRound with results
     */
    ElectionRound run_election_round(int round_number);

    /**
     * prune_inactive_analysts - Remove analysts below contribution threshold
     *
     * An analyst is pruned if:
     * - idle_rounds >= MAX_IDLE_ROUNDS
     * - total_score < MIN_CONTRIBUTION_SCORE
     * - AND analyst_count > base_analyst_count
     *
     * @return  Number of analysts pruned
     */
    int prune_inactive_analysts();

    /**
     * tally_votes - Count and aggregate votes
     *
     * Combines analyst feedback into score map.
     *
     * @param votes  Vector of votes from this round
     * @return  Map of analyst_id → aggregated_score
     */
    std::map<int, int> tally_votes(const std::vector<Vote>& votes);

    /**
     * select_winner - Choose best performer
     *
     * Winner is analyst with highest aggregated score.
     * Ties break to earliest analyst.
     *
     * @param score_map  Results from tally_votes()
     * @return  Analyst ID of winner (-1 if tie or no votes)
     */
    int select_winner(const std::map<int, int>& score_map);

    /**
     * get_election_history - Get all past rounds
     *
     * @return  Vector of completed ElectionRound results
     */
    std::vector<ElectionRound> get_election_history() const;

    /**
     * get_current_round - Get most recent election
     *
     * @return  Last ElectionRound, or empty if no elections yet
     */
    ElectionRound get_current_round() const;

    /**
     * get_round_count - Total completed rounds
     *
     * @return  Number of elections held
     */
    size_t get_round_count() const { return election_history_.size(); }

private:
    AnalystPool& pool_;
    std::vector<ElectionRound> election_history_;

    /* Helper for generating votes (to be filled by subclass or caller) */
    std::vector<Vote> generate_votes_for_round(int round_number);
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_ELECTION_H */
