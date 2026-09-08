#ifndef COUNCIL_TYPES_H
#define COUNCIL_TYPES_H

/**
 * core/types.h - Core C++ Type Definitions
 *
 * Phase 1.1: Defines all types, enums, constants used throughout the codebase.
 *
 * Design Principles:
 * - Use enum class for type safety (avoid C-style enum pollution)
 * - Use constexpr for compile-time constants
 * - Use std::string for dynamic strings (replace char[MAX] fixed arrays)
 * - Use std::vector for dynamic collections (replace fixed-size arrays)
 * - Keep struct layout compatible with C for Phase 0 (C version still used)
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <chrono>

namespace tribunal {
namespace core {

/* ====================================================================
 * Constants (formerly #define)
 * ==================================================================== */

namespace config {
    /* Model and analyst configuration */
    constexpr int MAX_MODELS = 16;           /* max concurrent analysts */
    constexpr int MAX_DYNAMIC_ROLES = 4;     /* cap on spawned roles */
    constexpr int BASE_ANALYST_COUNT = 4;    /* static analysts (never pruned) */

    /* File and code size limits */
    constexpr int MAX_FILES = 64;
    constexpr int MAX_PATH_LEN = 512;
    constexpr int MAX_FILE_BYTES = 32768;    /* skip files larger than this */

    /* Prompt and response buffers */
    constexpr int MAX_PROMPT_LEN = 16384;
    constexpr int MAX_RESPONSE_LEN = 65536;
    constexpr int MAX_CODE_LEN = 524288;     /* 512KB aggregate codebase */

    /* Debate and voting */
    constexpr int MAX_ROUNDS = 16;
    constexpr int MAX_ROLE_NAME = 48;

    /* String buffers for analyst data */
    constexpr int MAX_SYSTEM_PROMPT = 3072;
    constexpr int MAX_VOTE_REASON = 256;

    /* Configuration and filtering */
    constexpr int MAX_EXCLUDE_DIRS = 32;
    constexpr int MAX_GITIGNORE_PATTERNS = 64;
    constexpr int MAX_GITIGNORE_PAT_LEN = 128;

    /* Manifest and logging */
    constexpr int MAX_MANIFEST_LEN = 16384;
    constexpr int MAX_SKIPPED_LOG = 5120;

    /* Output markers */
    constexpr std::string_view FILE_MARKER = "/* COUNCIL_FILE: ";
    constexpr std::string_view FILE_MARKER_END = " */";

    /* Contribution scoring */
    constexpr int SCORE_ADOPTED = 2;         /* arbiter used this code */
    constexpr int SCORE_CHALLENGE_WON = 1;   /* peer conceded */
    constexpr int SCORE_ROUND_ACTIVE = 0;    /* non-zero response, no bonus */

    /* Pruning thresholds */
    constexpr int MAX_IDLE_ROUNDS = 3;       /* prune after N inactive rounds */
    constexpr int MIN_CONTRIBUTION_SCORE = 2; /* prune if total score < this */

    /* Database defaults */
    constexpr std::string_view DB_PATH = "lessons/council.db";
    constexpr std::string_view OLLAMA_URL = "http://localhost:11434/api/chat";
}

/* ====================================================================
 * Enums (Type-Safe)
 * ==================================================================== */

/**
 * AnalystRole - Role type for each analyst in the council
 *
 * Design note: Use enum class for type safety. First 4 roles are static
 * (always present), DYNAMIC is for spawned roles, JUDGE is for arbitration.
 */
enum class AnalystRole : int {
    Security = 0,      /* Finds security vulnerabilities, exploits */
    Performance = 1,   /* Optimizes speed, resource usage */
    Correctness = 2,   /* Ensures logic, spec compliance */
    Style = 3,         /* Code quality, readability, maintainability */
    Dynamic = 4,       /* Dynamically spawned specialist roles */
    Judge = 5,         /* Arbitrator (not in analyst pool) */
    Count = 6          /* Total role types (for sizing) */
};

/**
 * QueryType - Classification of user intent
 *
 * Used by QueryClassifier to understand what the user wants to analyze.
 * Enables LLM fallback for unknown query types.
 */
enum class QueryType : int {
    CodeReview = 0,        /* General code review */
    SecurityAudit = 1,     /* Security-focused review */
    Performance = 2,       /* Performance optimization */
    Correctness = 3,       /* Logic/spec verification */
    Style = 4,             /* Code quality improvement */
    Refactoring = 5,       /* Refactoring suggestions */
    Unknown = 6            /* Fallback: ask LLM to classify */
};

/**
 * ElectionOutcome - Result of voting
 */
enum class ElectionOutcome : int {
    Success = 0,           /* Someone elected */
    Tie = 1,               /* Tie, fallback to judge_model */
    Error = 2              /* Voting failed */
};

/**
 * VoteType - Reason for voting (spawn, prune, election)
 */
enum class VoteType : int {
    ArbiterElection = 0,   /* Vote for new arbiter */
    RoleSpawn = 1,         /* Vote to add new specialist */
    AnalystPrune = 2       /* Vote to remove idle analyst */
};

/* ====================================================================
 * Core Data Structures
 * ==================================================================== */

/**
 * AnalystStats - Per-analyst contribution tracking
 *
 * Used to track whether an analyst is contributing meaningfully.
 * Forms basis for pruning decisions (remove if idle too long).
 */
struct AnalystStats {
    int proposals_adopted = 0;      /* times arbiter used this analyst's code */
    int challenges_won = 0;         /* times a peer conceded to this analyst */
    int rounds_active = 0;          /* total rounds with non-empty response */
    int rounds_since_contrib = 0;   /* consecutive rounds with no contribution */
    int total_score = 0;            /* running composite score */
    int removal_immune = 0;         /* rounds remaining in grace period */
    int pruned = 0;                 /* 1 = removed from active pool */

    bool is_active() const {
        return pruned == 0 && removal_immune >= 0;
    }
};

/**
 * RoundResponse - Response from one analyst in one round
 *
 * Stores both the text response and any error that occurred.
 * Used for debate transcript and decision-tree building.
 */
struct RoundResponse {
    std::string text;               /* response from analyst (up to MAX_RESPONSE_LEN) */
    int error = 0;                  /* error code if query failed */
};

/**
 * FileEntry - Metadata for one source file in the codebase
 *
 * Phase 1 stores path and summary. Phase 2+ will add caching.
 */
struct FileEntry {
    std::string path;               /* full or relative path */
    std::string summary;            /* brief description of file purpose */
};

/**
 * ElectionRecord - Results of one voting round
 *
 * Recorded for replay and decision-tree building.
 * Tracks who voted for whom and why.
 */
struct ElectionRecord {
    int arbiter_idx = -1;           /* index of elected analyst */
    std::string arbiter_role;       /* role of elected analyst */
    std::string arbiter_model;      /* model of elected analyst */
    std::vector<int> votes;         /* votes[i] = whom analyst i voted for (-1 = abstain) */
    std::vector<std::string> reasons;  /* one-sentence justification per voter */
    int held_after_round = 0;       /* which round this election occurred */
};

/**
 * PoolEvent - Record of analyst pool changes
 *
 * Tracks when analysts are spawned or pruned, and why.
 * Used for decision transparency and debugging.
 */
struct PoolEvent {
    int round = 0;                  /* round number */
    VoteType vote_type = VoteType::ArbiterElection;  /* spawn, prune, or election */
    bool passed = false;            /* vote passed? */
    std::string role_name;          /* name of role (spawned or pruned) */
    std::string model;              /* model used for this role */
    std::string rationale;          /* why spawned/pruned/elected */
};

/**
 * AnalystData - State and responses for one member of the council
 *
 * Phase 1: Basic structure. Phase 3: Replaced by Analyst C++ class with methods.
 * Stores all responses across rounds, stats, and current state.
 */
struct AnalystData {
    /* Identity */
    std::string model;              /* LLM model name (e.g. "llama3.2") */
    AnalystRole role = AnalystRole::Security;
    std::string role_name;          /* "security", "performance", or custom */

    /* System prompt (what this analyst should look for) */
    std::string system_prompt;      /* up to MAX_SYSTEM_PROMPT bytes */

    /* Responses across all rounds */
    std::vector<RoundResponse> rounds;  /* one per round (size = round_count) */

    /* Contribution tracking */
    AnalystStats stats;

    /* Lifecycle */
    int is_dynamic = 0;             /* 1 = spawned at runtime, 0 = static */
    int spawn_round = 0;            /* which round this analyst was added (0-based) */
    int done = 0;                   /* 1 = finished all rounds */
    int error = 0;                  /* error code if failed */
};

/**
 * Council - Main state container for one debate
 *
 * Phase 1: Holds all data. Phase 3 will become a C++ class.
 * This is the "world state" that debate loop manipulates.
 */
struct Council {
    /* Task specification */
    std::string task;               /* what user asked us to analyze */

    /* Codebase being analyzed */
    std::vector<FileEntry> files;   /* one entry per source file */

    /* Analysts (now Analyst class, but keeping vector for compatibility) */
    /* std::vector<AnalystData> analysts; */  /* replaced by AnalystPool in Phase 3.2 */

    /* Analysts participating in debate */
    std::vector<AnalystData> analysts;  /* active and completed analysts (replaced by AnalystPool in Phase 3) */
    int base_analyst_count = 4;     /* static analysts (never prune below) */

    /* Debate progress */
    int round_count = 0;            /* number of rounds to run */
    int current_round = 0;          /* which round we're on */

    /* Election schedule and state */
    int election_start = 1;         /* first vote after this round */
    int election_backoff = 2;       /* multiply interval by this after each vote */
    int election_max = 8;           /* cap election interval at this */
    int next_election_round = 0;    /* when to hold next election */
    int election_interval = 1;      /* current interval (doubles each election) */

    std::vector<ElectionRecord> elections;  /* one per election held */

    /* Dynamic role spawning */
    int spawn_enabled = 1;          /* 0 = disabled */
    int spawn_check_round = 1;      /* first round to consider spawning */
    int dynamic_count = 0;          /* how many dynamic analysts added */

    /* Pruning (remove idle analysts) */
    int prune_enabled = 1;          /* 0 = disabled */
    int prune_interval = 3;         /* rounds_since_contrib threshold */
    int prune_grace = 2;            /* immunity rounds after spawn */
    int prune_respawn = 1;          /* trigger spawn vote after prune? */

    /* Pool event log (for transparency) */
    std::vector<PoolEvent> pool_events;

    /* Models */
    std::string judge_model;        /* model for arbitration and voting */
    std::string fast_model;         /* optional fast model for voting */
    int fast_mode = 0;              /* use fast_model for elections? */

    /* Filtering */
    int max_file_bytes = config::MAX_FILE_BYTES;  /* skip files larger than this */
    std::vector<std::string> exclude_dirs;        /* directories to skip */
    std::vector<std::string> gitignore_patterns;  /* patterns to skip */

    /* Output and persistence */
    std::string consensus;          /* arbiter's final answer */
    std::string manifest;           /* log of files examined */
    std::string skipped_log;        /* log of files skipped */
    std::string db_path;            /* path to lesson database */

    /* Configuration */
    int auto_apply = 0;             /* auto-apply changes? */
};

/**
 * Configuration - Parsed config file
 *
 * Phase 1.3 will implement Config parser.
 * Stores all settings from council.conf.
 */
struct Configuration {
    /* Debate */
    int rounds = 4;

    /* Election */
    int election_start = 1;
    int election_backoff = 2;
    int election_max = 8;

    /* Spawning and pruning */
    int spawn_enabled = 1;
    int spawn_check_round = 1;
    int prune_enabled = 1;
    int prune_interval = 3;
    int prune_grace = 2;
    int min_analysts = 2;
    int prune_respawn = 1;

    /* Models */
    std::vector<std::string> models;       /* analyst models */
    std::string judge_model = "llama3.2";
    std::string fast_model = "";

    /* Ollama configuration */
    std::vector<std::string> ollama_urls;  /* Multi-endpoint support */
    int ollama_timeout = 120;              /* Request timeout in seconds */

    /* Filtering */
    int max_file_bytes = config::MAX_FILE_BYTES;
    std::vector<std::string> exclude_dirs;
    std::vector<std::string> gitignore_patterns;

    /* API backend selection */
    std::string api_type = "ollama";       /* "ollama", "claude", "google-agy" */

    /* Ollama configuration */
    std::string ollama_api_url = "http://localhost:11434/api/chat";

    /* Claude API configuration */
    std::string claude_api_key = "";       /* from env CLAUDE_API_KEY or config */
    std::string claude_model = "claude-3-5-sonnet-20241022";

    /* Google Antigravity API configuration */
    std::string google_agy_api_key = "";   /* from env GOOGLE_AGY_API_KEY or config */
    std::string google_agy_model = "";     /* from env GOOGLE_AGY_MODEL or config */
    std::string google_agy_endpoint = ""; /* custom endpoint if needed */
};

/* ====================================================================
 * Utilities
 * ==================================================================== */

/**
 * Convert AnalystRole to string (for logging, display)
 */
inline std::string role_to_string(AnalystRole role) {
    switch (role) {
        case AnalystRole::Security:     return "security";
        case AnalystRole::Performance:  return "performance";
        case AnalystRole::Correctness:  return "correctness";
        case AnalystRole::Style:        return "style";
        case AnalystRole::Dynamic:      return "dynamic";
        case AnalystRole::Judge:        return "judge";
        default:                         return "unknown";
    }
}

/**
 * Convert string to AnalystRole (for parsing config)
 */
inline std::optional<AnalystRole> string_to_role(const std::string& str) {
    if (str == "security")      return AnalystRole::Security;
    if (str == "performance")   return AnalystRole::Performance;
    if (str == "correctness")   return AnalystRole::Correctness;
    if (str == "style")         return AnalystRole::Style;
    if (str == "dynamic")       return AnalystRole::Dynamic;
    if (str == "judge")         return AnalystRole::Judge;
    return std::nullopt;
}

}  /* namespace core */
}  /* namespace tribunal */

#endif /* COUNCIL_TYPES_H */
