#include "Pool.h"
#include <thread>
#include <algorithm>

namespace tribunal {
namespace core {

AnalystPool::AnalystPool(const std::vector<std::string>& models)
    : base_analyst_count_(4) {
    /* Create 4 base analysts */
    const AnalystRole base_roles[] = {
        AnalystRole::Security,
        AnalystRole::Performance,
        AnalystRole::Correctness,
        AnalystRole::Style
    };

    const std::string base_prompts[] = {
        "You are a security expert. Look for vulnerabilities, exploits, and unsafe patterns.",
        "You are a performance expert. Look for inefficiencies, resource usage, and optimization opportunities.",
        "You are a correctness expert. Look for logic errors, edge cases, and specification violations.",
        "You are a style expert. Look for code quality, readability, maintainability, and best practices."
    };

    for (size_t i = 0; i < 4 && i < models.size(); i++) {
        auto analyst = std::make_unique<Analyst>(
            base_roles[i],
            models[i],
            base_prompts[i]
        );
        analysts_.push_back(std::move(analyst));
    }
}

bool AnalystPool::add_analyst(
    AnalystRole role,
    const std::string& model,
    const std::string& system_prompt
) {
    if (analysts_.size() >= config::MAX_MODELS) {
        return false;  /* At capacity */
    }

    auto analyst = std::make_unique<Analyst>(role, model, system_prompt);
    analysts_.push_back(std::move(analyst));
    return true;
}

bool AnalystPool::remove_analyst(size_t index) {
    if (index >= analysts_.size()) {
        return false;
    }

    analysts_.erase(analysts_.begin() + index);
    return true;
}

int AnalystPool::execute_round(
    llm::LLMClient& llm_client,
    const std::string& prompt
) {
    int success_count = 0;

    /* Execute all analysts in parallel */
    std::vector<std::thread> threads;
    for (auto& analyst : analysts_) {
        if (!analyst->is_active()) {
            continue;  /* Skip pruned analysts */
        }

        threads.emplace_back([this, &analyst, &llm_client, &prompt, &success_count]() {
            if (analyst->execute_round(llm_client, prompt)) {
                success_count++;
            }
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return success_count;
}

Analyst* AnalystPool::get_analyst(size_t index) {
    if (index >= analysts_.size()) {
        return nullptr;
    }
    return analysts_[index].get();
}

const Analyst* AnalystPool::get_analyst(size_t index) const {
    if (index >= analysts_.size()) {
        return nullptr;
    }
    return analysts_[index].get();
}

Analyst* AnalystPool::get_analyst_by_role(AnalystRole role) {
    for (auto& analyst : analysts_) {
        if (analyst->get_role() == role) {
            return analyst.get();
        }
    }
    return nullptr;
}

size_t AnalystPool::active_count() const {
    size_t count = 0;
    for (const auto& analyst : analysts_) {
        if (analyst->is_active()) {
            count++;
        }
    }
    return count;
}

std::vector<AnalystStats> AnalystPool::get_stats() const {
    std::vector<AnalystStats> stats;
    for (const auto& analyst : analysts_) {
        stats.push_back(analyst->get_stats());
    }
    return stats;
}

}  /* namespace core */
}  /* namespace tribunal */
