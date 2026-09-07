#include "QueryClassifier.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace tribunal {
namespace ui {

QueryClassifier::QueryClassifier() {
    initialize_keyword_sets();
}

void QueryClassifier::initialize_keyword_sets() {
    /* Security analysis keywords */
    keyword_sets_.push_back({
        core::QueryType::SecurityAudit,
        {"security", "vulnerability", "exploit", "attack", "breach", "unsafe",
         "injection", "xss", "csrf", "overflow", "buffer", "leak", "risk"},
        1.5f
    });

    /* Performance analysis keywords */
    keyword_sets_.push_back({
        core::QueryType::Performance,
        {"performance", "speed", "fast", "slow", "optimize", "efficient",
         "latency", "throughput", "memory", "cpu", "bottleneck", "profile"},
        1.5f
    });

    /* Correctness analysis keywords */
    keyword_sets_.push_back({
        core::QueryType::Correctness,
        {"correct", "logic", "bug", "error", "logic", "edge case", "spec",
         "specification", "invariant", "precondition", "postcondition"},
        1.5f
    });

    /* Style/maintainability keywords */
    keyword_sets_.push_back({
        core::QueryType::Style,
        {"style", "readable", "maintainable", "clean", "refactor",
         "naming", "convention", "pattern", "structure", "design"},
        1.5f
    });

    /* Default: general code review */
    keyword_sets_.push_back({
        core::QueryType::CodeReview,
        {"review", "analyze", "check", "assess", "evaluate", "quality"},
        1.0f
    });
}

QueryClassification QueryClassifier::classify(const std::string& query_text) {
    QueryClassification result;

    /* Score each type */
    float max_score = 0.0f;
    core::QueryType best_type = core::QueryType::CodeReview;

    for (const auto& kw_set : keyword_sets_) {
        float score = score_query_for_type(query_text, kw_set.type);
        if (score > max_score) {
            max_score = score;
            best_type = kw_set.type;
        }
    }

    result.primary_type = best_type;
    result.relevant_roles = get_roles_for_type(best_type);
    result.confidence = std::min(1.0f, max_score / 10.0f);

    /* Generate human-readable label */
    switch (best_type) {
        case core::QueryType::SecurityAudit:
            result.classified_as = "Security Audit";
            break;
        case core::QueryType::Performance:
            result.classified_as = "Performance Analysis";
            break;
        case core::QueryType::Correctness:
            result.classified_as = "Correctness Verification";
            break;
        case core::QueryType::Style:
            result.classified_as = "Style & Maintainability";
            break;
        default:
            result.classified_as = "Code Review";
            break;
    }

    /* Generate analysis explanation */
    std::ostringstream oss;
    oss << "Query classified as " << result.classified_as
        << " (confidence: " << (int)(result.confidence * 100) << "%). "
        << result.relevant_roles.size() << " analysts will focus on this type.";
    result.analysis = oss.str();

    return result;
}

float QueryClassifier::score_query_for_type(
    const std::string& query,
    core::QueryType type
) const {
    float score = 0.0f;

    for (const auto& kw_set : keyword_sets_) {
        if (kw_set.type != type) continue;

        for (const auto& keyword : kw_set.keywords) {
            if (contains_keyword(query, keyword)) {
                score += kw_set.weight;
            }
        }
    }

    return score;
}

bool QueryClassifier::contains_keyword(
    const std::string& text,
    const std::string& keyword
) const {
    /* Case-insensitive search */
    std::string lower_text = text;
    std::string lower_keyword = keyword;

    std::transform(lower_text.begin(), lower_text.end(),
                   lower_text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(lower_keyword.begin(), lower_keyword.end(),
                   lower_keyword.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lower_text.find(lower_keyword) != std::string::npos;
}

std::vector<core::AnalystRole> QueryClassifier::get_roles_for_type(core::QueryType type) const {
    std::vector<core::AnalystRole> roles;

    switch (type) {
        case core::QueryType::SecurityAudit:
            roles.push_back(core::AnalystRole::Security);
            roles.push_back(core::AnalystRole::Correctness);
            break;

        case core::QueryType::Performance:
            roles.push_back(core::AnalystRole::Performance);
            roles.push_back(core::AnalystRole::Correctness);
            break;

        case core::QueryType::Correctness:
            roles.push_back(core::AnalystRole::Correctness);
            roles.push_back(core::AnalystRole::Performance);
            break;

        case core::QueryType::Style:
            roles.push_back(core::AnalystRole::Style);
            roles.push_back(core::AnalystRole::Correctness);
            break;

        default:
            /* General code review: all roles */
            roles.push_back(core::AnalystRole::Security);
            roles.push_back(core::AnalystRole::Performance);
            roles.push_back(core::AnalystRole::Correctness);
            roles.push_back(core::AnalystRole::Style);
            break;
    }

    return roles;
}

std::string QueryClassifier::get_system_prompt_for_type(core::QueryType type) const {
    switch (type) {
        case core::QueryType::SecurityAudit:
            return "You are analyzing code for security vulnerabilities. Look for: "
                   "buffer overflows, SQL injection, XSS, authentication/authorization flaws, "
                   "unsafe memory operations, and other security risks.";

        case core::QueryType::Performance:
            return "You are analyzing code for performance and efficiency. Look for: "
                   "inefficient algorithms, unnecessary allocations, cache misses, "
                   "locking contention, and optimization opportunities.";

        case core::QueryType::Correctness:
            return "You are analyzing code for correctness. Look for: "
                   "logic errors, edge cases not handled, violations of specifications, "
                   "incorrect invariants, and flawed algorithms.";

        case core::QueryType::Style:
            return "You are analyzing code for style and maintainability. Look for: "
                   "naming consistency, code organization, clarity, adherence to patterns, "
                   "and opportunities to improve readability.";

        default:
            return "You are performing a general code review. Analyze the code for quality, "
                   "correctness, security, performance, and maintainability issues.";
    }
}

}  /* namespace ui */
}  /* namespace tribunal */
