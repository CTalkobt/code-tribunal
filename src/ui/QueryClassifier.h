#ifndef TRIBUNAL_QUERY_CLASSIFIER_H
#define TRIBUNAL_QUERY_CLASSIFIER_H

/**
 * ui/QueryClassifier.h - Query Classification and Routing
 *
 * Phase 4.1: Classify user queries and route to appropriate debate topics
 *
 * Classifies:
 * - Code review (general quality assessment)
 * - Security audit (vulnerability detection)
 * - Performance analysis (efficiency and optimization)
 * - Correctness verification (logic and specification)
 * - Style/maintainability (code quality patterns)
 */

#pragma once

#include "../core/types.h"
#include <string>
#include <vector>

namespace tribunal {
namespace ui {

/**
 * QueryClassification - Result of query analysis
 *
 * Determines which analysts should focus on the query.
 */
struct QueryClassification {
    core::QueryType primary_type = core::QueryType::CodeReview;  /* main focus */
    std::vector<core::AnalystRole> relevant_roles;               /* analysts to prioritize */
    std::string classified_as;                                   /* human-readable label */
    float confidence = 0.0f;                                     /* 0.0-1.0 confidence score */
    std::string analysis;                                        /* explanation of classification */
};

/**
 * QueryClassifier - Classify incoming user queries
 *
 * Analyzes user query text to determine:
 * - Primary query type (code review, security, perf, etc.)
 * - Which analysts should focus on the query
 * - Confidence in classification
 *
 * Thread-safe for concurrent classification.
 */
class QueryClassifier {
public:
    /**
     * Constructor
     */
    QueryClassifier();

    /**
     * classify - Analyze user query text
     *
     * Uses keyword matching and heuristics to determine query intent.
     * Falls back to general CodeReview if unclear.
     *
     * @param query_text  User's question or request
     * @return  QueryClassification with type, roles, and confidence
     */
    QueryClassification classify(const std::string& query_text);

    /**
     * get_roles_for_type - Get relevant analysts for query type
     *
     * @param type  QueryType to analyze
     * @return  Vector of AnalystRole for this type
     */
    std::vector<core::AnalystRole> get_roles_for_type(core::QueryType type) const;

    /**
     * get_system_prompt_for_type - Get debate prompt for query type
     *
     * @param type  QueryType to create prompt for
     * @return  System prompt directing analysts on this type of analysis
     */
    std::string get_system_prompt_for_type(core::QueryType type) const;

private:
    /* Keyword detection for classification */
    struct KeywordSet {
        core::QueryType type;
        std::vector<std::string> keywords;
        float weight = 1.0f;
    };

    std::vector<KeywordSet> keyword_sets_;

    /* Helper methods */
    void initialize_keyword_sets();
    float score_query_for_type(const std::string& query, core::QueryType type) const;
    bool contains_keyword(const std::string& text, const std::string& keyword) const;
};

}  /* namespace ui */
}  /* namespace tribunal */

#endif /* TRIBUNAL_QUERY_CLASSIFIER_H */
