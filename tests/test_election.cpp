/**
 * tests/test_election.cpp - Election and Voting System Tests
 *
 * Tests for vote tallying, winner selection, and pruning logic
 */

#include <iostream>
#include <cassert>
#include <vector>
#include "../src/core/types.h"
#include "../src/core/Pool.h"
#include "../src/core/Election.h"

using namespace tribunal;

int test_vote_tallying() {
    std::cout << "[TEST] Vote Tallying\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    // Create test votes
    std::vector<core::Vote> votes;
    core::Vote v1;
    v1.voter_id = 0;
    v1.votee_id = 1;
    v1.score = 5;
    votes.push_back(v1);
    
    core::Vote v2;
    v2.voter_id = 1;
    v2.votee_id = 1;
    v2.score = 3;
    votes.push_back(v2);
    
    core::Vote v3;
    v3.voter_id = 2;
    v3.votee_id = 2;
    v3.score = 4;
    votes.push_back(v3);
    
    auto scores = voting.tally_votes(votes);
    
    assert(scores[1] == 8);  // analyst 1 got 5 + 3
    assert(scores[2] == 4);  // analyst 2 got 4
    
    std::cout << "  ✓ Vote tallying works\n";
    return 0;
}

int test_winner_selection() {
    std::cout << "[TEST] Winner Selection\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    // Create score map
    std::map<int, int> scores;
    scores[0] = 5;
    scores[1] = 10;   // winner
    scores[2] = 3;
    scores[3] = 7;
    
    int winner = voting.select_winner(scores);
    assert(winner == 1);
    
    std::cout << "  ✓ Winner selection works\n";
    return 0;
}

int test_winner_selection_tie() {
    std::cout << "[TEST] Winner Selection with Tie\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    // Create score map with tie
    std::map<int, int> scores;
    scores[0] = 5;
    scores[1] = 10;
    scores[2] = 10;   // tied with 1
    
    int winner = voting.select_winner(scores);
    assert(winner == 1 || winner == 2);  // either is acceptable
    
    std::cout << "  ✓ Tie-breaking works\n";
    return 0;
}

int test_empty_votes() {
    std::cout << "[TEST] Empty Vote Handling\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    std::vector<core::Vote> empty_votes;
    auto scores = voting.tally_votes(empty_votes);
    assert(scores.empty());
    
    int winner = voting.select_winner(scores);
    assert(winner == -1);  // no winner with no votes
    
    std::cout << "  ✓ Empty vote handling works\n";
    return 0;
}

int test_election_round_creation() {
    std::cout << "[TEST] Election Round Creation\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    auto election = voting.run_election_round(0);
    
    assert(election.round_number == 0);
    
    std::cout << "  ✓ Election round creation works\n";
    return 0;
}

int test_election_history() {
    std::cout << "[TEST] Election History Tracking\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    auto e1 = voting.run_election_round(0);
    auto e2 = voting.run_election_round(1);
    
    assert(voting.get_round_count() == 2);
    
    auto history = voting.get_election_history();
    assert(history.size() == 2);
    assert(history[0].round_number == 0);
    assert(history[1].round_number == 1);
    
    std::cout << "  ✓ Election history tracking works\n";
    return 0;
}

int test_vote_score_accumulation() {
    std::cout << "[TEST] Vote Score Accumulation\n";
    
    std::vector<std::string> models = {"m1", "m2", "m3", "m4"};
    core::AnalystPool pool(models);
    core::VotingSystem voting(pool);
    
    std::vector<core::Vote> votes;
    
    // Multiple votes for same analyst
    for (int i = 0; i < 5; i++) {
        core::Vote v;
        v.voter_id = i;
        v.votee_id = 0;
        v.score = 2;
        votes.push_back(v);
    }
    
    auto scores = voting.tally_votes(votes);
    assert(scores[0] == 10);  // 5 votes * 2 score each
    
    std::cout << "  ✓ Vote score accumulation works\n";
    return 0;
}

int main() {
    std::cout << "\n=== ELECTION SYSTEM TESTS ===\n\n";
    
    int failures = 0;
    failures += test_vote_tallying();
    failures += test_winner_selection();
    failures += test_winner_selection_tie();
    failures += test_empty_votes();
    failures += test_election_round_creation();
    failures += test_election_history();
    failures += test_vote_score_accumulation();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All election tests passed (7/7)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
