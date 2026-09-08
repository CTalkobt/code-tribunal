/**
 * src/main.cpp - Code-Tribunal C++ Entry Point
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
#include "core/ConfigParser.h"
#include "llm/LLMClient.h"
#include "llm/OllamaClient.h"
#include "llm/MultiEndpointOllamaClient.h"
#include "ui/QueryClassifier.h"
#include "util/Logging.h"
#include "http/HttpServer.h"

using namespace tribunal;

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] (<query> | --web)\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --web               Launch web dashboard (HTTP server on port 8080)\n";
    std::cerr << "  --ollama <url>      Ollama server URL (can be specified multiple times)\n";
    std::cerr << "                      Default: http://localhost:11434\n";
    std::cerr << "                      Multi-endpoint enables parallelism (2+ endpoints)\n";
    std::cerr << "  --rounds <n>        Number of debate rounds (default: 4)\n";
    std::cerr << "  --models <m1,m2...> Models to use (comma-separated)\n";
    std::cerr << "  --help              Show this help message\n\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << program_name << " --web\n";
    std::cerr << "  " << program_name << " \"Check for security issues in this code\"\n";
    std::cerr << "  " << program_name << " --ollama http://gpu1:11434 --ollama http://gpu2:11434 \"query\"\n";
}

struct Options {
    std::string config_path = "config/council.conf";  /* Default config file */
    std::vector<std::string> ollama_urls;  /* Multi-endpoint support */
    int rounds = 0;  /* 0 means use config file default */
    std::vector<std::string> models;  /* Empty means use config file default */
    std::string query;
    bool web_mode = false;
};

bool parse_arguments(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            opts.config_path = argv[++i];
        } else if (strcmp(argv[i], "--web") == 0) {
            opts.web_mode = true;
        } else if (strcmp(argv[i], "--ollama") == 0 && i + 1 < argc) {
            opts.ollama_urls.push_back(argv[++i]);  /* Accumulate endpoints */
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

    /* Load configuration from file (with sensible defaults) */
    logger.log(util::LogLevel::Info, "Loading configuration from: " + opts.config_path);
    core::Configuration config = core::ConfigParser::load_or_default(opts.config_path);
    logger.log(util::LogLevel::Info, "Configuration loaded (rounds=" + std::to_string(config.rounds) +
               ", models=" + std::to_string(config.models.size()) + ")");

    /* Override config with CLI arguments if specified */
    if (opts.rounds > 0) {
        config.rounds = opts.rounds;
    }
    if (!opts.models.empty()) {
        config.models = opts.models;
    }
    if (!opts.ollama_urls.empty()) {
        config.ollama_urls = opts.ollama_urls;
    }

    /* Ensure we have at least 4 models */
    if (config.models.size() < 4) {
        logger.log(util::LogLevel::Warning,
                   "Less than 4 models specified; using defaults for base analysts");
        config.models = {"llama3.2", "mistral", "neural-chat", "dolphin-mixtral"};
    }

    /* Handle --web mode */
    if (opts.web_mode) {
        logger.log(util::LogLevel::Info, "Starting HTTP server for web dashboard...");
        logger.log(util::LogLevel::Info, "LLM Provider: " + config.api_type);

        /* Auto-config Ollama endpoints for web mode if needed */
        if (config.api_type == "ollama" && config.ollama_urls.empty()) {
            config.ollama_urls.push_back("http://localhost:11434");
        }

        try {
            auto llm_client = llm::ClientFactory::create_from_config(
                config.api_type,
                config.claude_api_key,
                config.claude_model,
                config.google_agy_api_key,
                config.google_agy_model,
                config.google_agy_endpoint,
                config.ollama_urls.empty() ? "" : config.ollama_urls[0]
            );

            if (!llm_client->is_available()) {
                logger.log(util::LogLevel::Error, "LLM service unavailable: " + llm_client->get_last_error());
                return 1;
            }

            logger.log(util::LogLevel::Info, "LLM client ready: " + llm_client->get_name());
            http::HttpServer server(config, std::move(llm_client));
            return server.start();

        } catch (const std::exception& e) {
            logger.log(util::LogLevel::Error, std::string("Failed to initialize LLM client: ") + e.what());
            return 1;
        }
    }

    logger.log(util::LogLevel::Info, "Starting Code-Tribunal debate system");
    logger.log(util::LogLevel::Info, "Query: " + opts.query);
    logger.log(util::LogLevel::Info, "Rounds: " + std::to_string(config.rounds));
    logger.log(util::LogLevel::Info, "LLM Provider: " + config.api_type);

    /* Auto-configuration for Ollama: multiple endpoints → parallel, single → sequential */
    if (config.api_type == "ollama") {
        if (config.ollama_urls.empty()) {
            config.ollama_urls.push_back("http://localhost:11434");
            logger.log(util::LogLevel::Info, "No Ollama endpoints specified; defaulting to localhost:11434");
            logger.log(util::LogLevel::Info, "Parallelism: 1 (sequential) - single endpoint");
        } else if (config.ollama_urls.size() == 1) {
            logger.log(util::LogLevel::Info, "Ollama endpoint: " + config.ollama_urls[0]);
            logger.log(util::LogLevel::Info, "Parallelism: 1 (sequential) - single endpoint for stability");
        } else {
            logger.log(util::LogLevel::Info, "Multi-endpoint Ollama configuration:");
            for (size_t i = 0; i < config.ollama_urls.size(); i++) {
                logger.log(util::LogLevel::Info, "  Endpoint " + std::to_string(i + 1) + ": " + config.ollama_urls[i]);
            }
            logger.log(util::LogLevel::Info,
                "Parallelism: " + std::to_string(config.ollama_urls.size()) + " (parallel batching enabled)");
        }
    }

    /* Classify query */
    ui::QueryClassifier classifier;
    auto classification = classifier.classify(opts.query);
    logger.log(util::LogLevel::Info, "Query classified: " + std::to_string(static_cast<int>(classification.primary_type)));

    /* Initialize LLM client using factory for selected provider */
    logger.log(util::LogLevel::Info, "Initializing LLM client...");
    std::unique_ptr<llm::LLMClient> llm_client;
    try {
        llm_client = llm::ClientFactory::create_from_config(
            config.api_type,
            config.claude_api_key,
            config.claude_model,
            config.google_agy_api_key,
            config.google_agy_model,
            config.google_agy_endpoint,
            config.ollama_urls.empty() ? "" : config.ollama_urls[0],
            120  /* timeout in seconds */
        );
    } catch (const std::exception& e) {
        logger.log(util::LogLevel::Error, std::string("Failed to initialize LLM client: ") + e.what());
        return 1;
    }

    if (!llm_client->is_available()) {
        logger.log(util::LogLevel::Error, "LLM service unavailable: " + llm_client->get_last_error());
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
