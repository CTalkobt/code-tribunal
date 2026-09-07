/**
 * src/main_cpp.cpp - Code-Tribunal C++ Entry Point
 *
 * Phase 6: Main binary entry point
 *
 * Orchestrates complete debate workflow:
 * 1. Parse arguments (query, code files)
 * 2. Classify query type
 * 3. Initialize analyst pool and LLM client
 * 4. Run debate rounds
 * 5. Display results
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>

#include "core/types.h"
#include "core/Council.h"
#include "llm/LLMClient.h"
#include "llm/OllamaClient.h"
#include "ui/QueryClassifier.h"
#include "util/Logging.h"

extern "C" {
    int start_http_server();
}

using namespace tribunal;

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] (<query> | --web)\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --web               Launch web dashboard (HTTP server on port 8080)\n";
    std::cerr << "  --ollama <url>      Ollama server URL (default: http://localhost:11434)\n";
    std::cerr << "  --rounds <n>        Number of debate rounds (default: 4)\n";
    std::cerr << "  --models <m1,m2...> Models to use (comma-separated)\n";
    std::cerr << "  --help              Show this help message\n\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << program_name << " --web\n";
    std::cerr << "  " << program_name << " \"Check for security issues in this code\"\n";
}

struct Options {
    std::string ollama_url = "http://localhost:11434";
    int rounds = 4;
    std::vector<std::string> models = {"llama3.2", "mistral", "neural-chat", "dolphin-mixtral"};
    std::string query;
    bool web_mode = false;
};

bool parse_arguments(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--web") == 0) {
            opts.web_mode = true;
        } else if (strcmp(argv[i], "--ollama") == 0 && i + 1 < argc) {
            opts.ollama_url = argv[++i];
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            opts.rounds = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--models") == 0 && i + 1 < argc) {
            /* Parse comma-separated models */
            std::string models_str = argv[++i];
            opts.models.clear();
            size_t start = 0;
            size_t end = models_str.find(',');
            while (end != std::string::npos) {
                opts.models.push_back(models_str.substr(start, end - start));
                start = end + 1;
                end = models_str.find(',', start);
            }
            opts.models.push_back(models_str.substr(start));
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (argv[i][0] != '-') {
            opts.query = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            return false;
        }
    }

    if (!opts.web_mode && opts.query.empty()) {
        std::cerr << "Error: Query required (or use --web for dashboard mode)\n";
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    Options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    /* Set up logging */
    util::Logger& logger = util::Logger::instance();
    logger.set_level(util::LogLevel::Info);

    if (opts.web_mode) {
        std::cout << "✓ Launching web dashboard (HTTP server)\n";
        std::cout << "✓ Web dashboard running on http://localhost:8080\n";
        std::cout << "Press Ctrl+C to exit\n";
        return start_http_server();
    }

    logger.log(util::LogLevel::Info, "Starting Code-Tribunal debate system");
    logger.log(util::LogLevel::Info, "Query: " + opts.query);
    logger.log(util::LogLevel::Info, "Ollama URL: " + opts.ollama_url);
    logger.log(util::LogLevel::Info, "Rounds: " + std::to_string(opts.rounds));

    /* Ensure we have at least 4 models */
    if (opts.models.size() < 4) {
        logger.log(util::LogLevel::Warning,
                   "Less than 4 models specified; using defaults for base analysts");
        opts.models = {"llama3.2", "mistral", "neural-chat", "dolphin-mixtral"};
    }

    /* Create configuration */
    core::Configuration config;
    config.rounds = opts.rounds;
    config.models = opts.models;
    config.api_type = "ollama";
    config.prune_enabled = 1;

    /* Classify query */
    ui::QueryClassifier classifier;
    auto classification = classifier.classify(opts.query);
    logger.log(util::LogLevel::Info, "Query classified: " + std::to_string(static_cast<int>(classification.primary_type)));

    /* Initialize LLM client */
    logger.log(util::LogLevel::Info, "Initializing LLM client...");
    auto llm_client = std::make_unique<llm::OllamaClient>(opts.ollama_url);

    if (!llm_client->is_available()) {
        logger.log(util::LogLevel::Error, "LLM service unavailable at " + opts.ollama_url);
        return 1;
    }

    logger.log(util::LogLevel::Info, "LLM client connected: " + llm_client->get_name());

    /* Create and initialize council */
    logger.log(util::LogLevel::Info, "Initializing council orchestrator...");
    core::CouncilOrchestrator council(config);

    if (!council.initialize_analysts(std::move(llm_client), config.models)) {
        logger.log(util::LogLevel::Error, "Failed to initialize analyst pool");
        return 1;
    }

    logger.log(util::LogLevel::Info, "Council initialized with " +
               std::to_string(config.models.size()) + " analysts");

    /* Run debate */
    logger.log(util::LogLevel::Info, "Starting debate...");

    try {
        core::DebateResult result = council.run_debate(opts.query, config.rounds);

        auto election_history = council.get_election_history();
        logger.log(util::LogLevel::Info,
                   "Debate complete: " + std::to_string(election_history.size()) +
                   " election rounds");
        logger.log(util::LogLevel::Info, "Winner: " + std::to_string(result.final_winner));

        return 0;

    } catch (const std::exception& e) {
        logger.log(util::LogLevel::Error, std::string("Debate error: ") + e.what());
        return 1;
    }
}
