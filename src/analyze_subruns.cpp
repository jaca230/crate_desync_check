#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <regex>
#include <set>
#include <iomanip>
#include <random>
#include <chrono>

#include "midasio.h"

namespace fs = std::filesystem;

struct RunSubrunAnalysis {
    int run_number;
    int first_corrupted_subrun;
    int max_subrun = -1;
    std::set<int> corrupted_subruns;
    std::set<int> all_subruns;
    double corruption_percentage = 0.0;
};

bool hasTriggerMismatch(const std::shared_ptr<TMEvent>& event) {
    event->FindAllBanks();
    uint32_t trigger_CR07 = 0, trigger_CR08 = 0;
    bool found_CR07 = false, found_CR08 = false;

    for (const auto& bank : event->banks) {
        if (bank.name != "CR07" && bank.name != "CR08") continue;
        const char* data = event->GetBankData(&bank);
        if (!data || bank.data_size < 8) continue;

        uint64_t header = 0;
        for (int i = 0; i < 8; ++i)
            header = (header << 8) | static_cast<uint8_t>(data[i]);
        uint32_t trigger = (header >> 32) & 0xFFFFFF;

        if (bank.name == "CR07") { trigger_CR07 = trigger; found_CR07 = true; }
        if (bank.name == "CR08") { trigger_CR08 = trigger; found_CR08 = true; }
    }
    return (found_CR07 && found_CR08) && (trigger_CR07 != trigger_CR08);
}

struct CorruptionInfo {
    bool is_corrupted = false;
    int event_number = -1;
    uint32_t trigger_CR07 = 0;
    uint32_t trigger_CR08 = 0;
    int total_events = 0;
};

CorruptionInfo hasCorruption(const fs::path& filepath) {
    CorruptionInfo info;
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << "\n";
        return info;
    }

    int event_count = 0;
    while (TMEvent* raw_event = TMReadEvent(reader)) {
        std::shared_ptr<TMEvent> evt(raw_event);
        event_count++;
        
        evt->FindAllBanks();
        uint32_t trigger_CR07 = 0, trigger_CR08 = 0;
        bool found_CR07 = false, found_CR08 = false;

        for (const auto& bank : evt->banks) {
            if (bank.name != "CR07" && bank.name != "CR08") continue;
            const char* data = evt->GetBankData(&bank);
            if (!data || bank.data_size < 8) continue;

            uint64_t header = 0;
            for (int i = 0; i < 8; ++i)
                header = (header << 8) | static_cast<uint8_t>(data[i]);
            uint32_t trigger = (header >> 32) & 0xFFFFFF;

            if (bank.name == "CR07") { trigger_CR07 = trigger; found_CR07 = true; }
            if (bank.name == "CR08") { trigger_CR08 = trigger; found_CR08 = true; }
        }
        
        if ((found_CR07 && found_CR08) && (trigger_CR07 != trigger_CR08)) {
            info.is_corrupted = true;
            info.event_number = event_count;
            info.trigger_CR07 = trigger_CR07;
            info.trigger_CR08 = trigger_CR08;
            info.total_events = event_count;
            delete reader;
            return info;
        }
    }
    
    info.total_events = event_count;
    delete reader;
    return info;
}

std::vector<RunSubrunAnalysis> parseResultsFile(const fs::path& results_path) {
    std::vector<RunSubrunAnalysis> analyses;
    std::ifstream file(results_path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open " << results_path << "\n";
        return analyses;
    }

    std::regex pattern(R"(Run (\d+) desync in file: (.+))");
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            RunSubrunAnalysis analysis;
            analysis.run_number = std::stoi(match[1].str());
            fs::path corrupted_file_path = match[2].str();

            std::regex subrun_pattern(R"(run\d{5}_(\d{5})\.mid\.lz4$)");
            std::string filename = corrupted_file_path.filename().string();
            std::smatch subrun_match;
            if (std::regex_search(filename, subrun_match, subrun_pattern)) {
                analysis.first_corrupted_subrun = std::stoi(subrun_match[1].str());
            }
            analyses.push_back(analysis);
        }
    }
    return analyses;
}

// Analyze with random sampling
void analyzeAllSubruns(RunSubrunAnalysis& analysis, const fs::path& results_path,
                       double sample_fraction, std::ofstream& log) {
    std::string run_str = std::to_string(analysis.run_number);
    run_str = std::string(5 - run_str.length(), '0') + run_str;

    std::ifstream file(results_path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not reopen " << results_path << "\n";
        return;
    }

    std::regex dir_pattern(R"(Run )" + std::to_string(analysis.run_number) + R"( desync in file: (.+))");
    std::string line;
    fs::path data_directory;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, dir_pattern)) {
            fs::path corrupted_file_path = match[1].str();
            data_directory = corrupted_file_path.parent_path();
            break;
        }
    }
    file.close();

    std::regex pattern(R"(run)" + run_str + R"(_(\d{5})\.mid\.lz4$)");
    std::vector<int> subruns;
    for (auto& entry : fs::directory_iterator(data_directory)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_search(filename, match, pattern)) {
            int subrun = std::stoi(match[1].str());
            subruns.push_back(subrun);
            analysis.all_subruns.insert(subrun);
        }
    }
    if (subruns.empty()) return;
    std::sort(subruns.begin(), subruns.end());
    analysis.max_subrun = subruns.back();

    // Sampling setup
    std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
    std::vector<int> sampled_subruns;
    for (int subrun : subruns) {
        if (subrun >= analysis.first_corrupted_subrun) {
            if (std::generate_canonical<double, 10>(rng) <= sample_fraction) {
                sampled_subruns.push_back(subrun);
            }
        }
    }
    if (sampled_subruns.empty()) sampled_subruns.push_back(subruns.back());

    std::cout << "[INFO] Run " << analysis.run_number << ": checking "
              << sampled_subruns.size() << " sampled subruns ("
              << std::fixed << std::setprecision(1) << (sample_fraction*100)
              << "% sample)\n";
    std::cout.flush(); // Force console output immediately

    // Log the start of this run's analysis
    log << "=== Starting analysis for Run " << analysis.run_number << " ===" << std::endl;
    log << "Checking " << sampled_subruns.size() << " subruns out of " << subruns.size() 
        << " total (" << std::fixed << std::setprecision(1) << (sample_fraction*100) 
        << "% sample)" << std::endl;
    log.flush(); // Force log file write immediately

    int corrupted_count = 0;
    for (int subrun : sampled_subruns) {
        std::string subrun_str = std::to_string(subrun);
        subrun_str = std::string(5 - subrun_str.length(), '0') + subrun_str;
        fs::path subrun_file = data_directory / ("run" + run_str + "_" + subrun_str + ".mid.lz4");

        // Log before checking (useful for debugging if it crashes)
        log << "Checking Run " << analysis.run_number << ", subrun " << subrun << "... ";
        log.flush();

        CorruptionInfo corruption_info = hasCorruption(subrun_file);
        if (corruption_info.is_corrupted) {
            analysis.corrupted_subruns.insert(subrun);
            corrupted_count++;
        }

        std::string status = corruption_info.is_corrupted ? "CORRUPTED" : "OK";
        
        if (corruption_info.is_corrupted) {
            std::cout << "  Subrun " << subrun << ": " << status 
                      << " (event " << corruption_info.event_number 
                      << "/" << corruption_info.total_events 
                      << ", CR07=" << std::hex << corruption_info.trigger_CR07 
                      << ", CR08=" << std::hex << corruption_info.trigger_CR08 
                      << std::dec << ")" << std::endl;
            
            log << status << " at event " << corruption_info.event_number 
                << "/" << corruption_info.total_events 
                << " (CR07=0x" << std::hex << corruption_info.trigger_CR07 
                << ", CR08=0x" << std::hex << corruption_info.trigger_CR08 
                << std::dec << ")" << std::endl;
        } else {
            std::cout << "  Subrun " << subrun << ": " << status 
                      << " (" << corruption_info.total_events << " events checked)" << std::endl;
            
            log << status << " (" << corruption_info.total_events << " events checked)" << std::endl;
        }
        
        std::cout.flush(); // Force console output immediately
        log.flush(); // Force log file write immediately after each subrun
    }

    size_t total_possible_subruns = analysis.max_subrun + 1;
    analysis.corruption_percentage = (double)corrupted_count / total_possible_subruns * 100.0;
    
    // Log summary for this run
    log << "=== Run " << analysis.run_number << " Summary ===" << std::endl;
    log << "Corrupted subruns found: " << corrupted_count << " out of " << sampled_subruns.size() 
        << " checked" << std::endl;
    log << "Estimated corruption percentage: " << std::fixed << std::setprecision(2) 
        << analysis.corruption_percentage << "%" << std::endl;
    log << std::endl; // Add blank line between runs
    log.flush();
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <results.txt_path> [sample_fraction]\n";
        std::cerr << "  sample_fraction: 0.0-1.0 (default 1.0 = check all)\n";
        return EXIT_FAILURE;
    }

    fs::path results_path = argv[1];
    double sample_fraction = 1.0;
    if (argc == 3) {
        sample_fraction = std::stod(argv[2]);
        if (sample_fraction <= 0 || sample_fraction > 1.0) {
            std::cerr << "[ERROR] sample_fraction must be in (0,1]\n";
            return EXIT_FAILURE;
        }
    }

    std::ofstream log("subrun_corruption_log.txt", std::ios::out);
    if (!log.is_open()) {
        std::cerr << "[ERROR] Could not open log file\n";
        return EXIT_FAILURE;
    }

    // Add timestamp to log file
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    log << "=== Subrun Corruption Analysis Log ===" << std::endl;
    log << "Started: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
    log << "Results file: " << results_path << std::endl;
    log << "Sample fraction: " << sample_fraction << std::endl;
    log << std::endl;
    log.flush();

    auto analyses = parseResultsFile(results_path);
    if (analyses.empty()) {
        std::cout << "[INFO] No corruptions found.\n";
        log << "No corruptions found in results file." << std::endl;
        log.flush();
        return EXIT_SUCCESS;
    }

    std::cout << "[INFO] Found " << analyses.size() << " runs with corruption to analyze\n";
    log << "Found " << analyses.size() << " runs with corruption to analyze" << std::endl;
    log.flush();

    for (auto& analysis : analyses) {
        analyzeAllSubruns(analysis, results_path, sample_fraction, log);
    }

    log << "=== Analysis Complete ===" << std::endl;
    auto end_time = std::chrono::system_clock::now();
    auto end_time_t = std::chrono::system_clock::to_time_t(end_time);
    log << "Finished: " << std::put_time(std::localtime(&end_time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
    log.flush();

    std::cout << "[INFO] Finished analysis. Log written to subrun_corruption_log.txt\n";
    return EXIT_SUCCESS;
}