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

// Extract triggers from CR07/CR08 - same function as main.cpp
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

// Check if a single file has corruption
bool hasCorruption(const fs::path& filepath) {
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << "\n";
        return false;
    }

    while (TMEvent* raw_event = TMReadEvent(reader)) {
        std::shared_ptr<TMEvent> evt(raw_event);
        if (hasTriggerMismatch(evt)) {
            delete reader;
            return true;
        }
    }
    delete reader;
    return false;
}

// Parse results.txt to extract initially corrupted runs
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
            
            // Extract subrun from filename
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

// Analyze all subruns for a given run
void analyzeAllSubruns(RunSubrunAnalysis& analysis, const fs::path& results_path) {
    // Extract data directory from the results file path
    // We need to reconstruct the directory from results.txt
    std::string run_str = std::to_string(analysis.run_number);
    run_str = std::string(5 - run_str.length(), '0') + run_str; // Zero-pad to 5 digits
    
    // Parse results.txt again to get the directory path
    std::ifstream file(results_path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not reopen " << results_path << " to get directory\n";
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
    
    if (data_directory.empty()) {
        std::cerr << "[ERROR] Could not determine data directory for run " << analysis.run_number << "\n";
        return;
    }
    
    // Find all subruns for this run
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
    
    if (subruns.empty()) {
        std::cerr << "[WARN] No files found for run " << analysis.run_number << "\n";
        return;
    }
    
    std::sort(subruns.begin(), subruns.end());
    analysis.max_subrun = subruns.back();
    
    std::cout << "[INFO] Analyzing run " << analysis.run_number 
              << " (first corruption at subrun " << analysis.first_corrupted_subrun
              << ", max subrun " << analysis.max_subrun << ")\n";
    std::cout << "[INFO] Found " << subruns.size() << " total subruns to check\n";
    
    // Check each subrun starting from the first corrupted one
    int checked_subruns = 0;
    int corrupted_count = 0;
    
    for (int subrun : subruns) {
        if (subrun >= analysis.first_corrupted_subrun) {
            ++checked_subruns;
            
            // Construct file path
            std::string subrun_str = std::to_string(subrun);
            subrun_str = std::string(5 - subrun_str.length(), '0') + subrun_str;
            fs::path subrun_file = data_directory / ("run" + run_str + "_" + subrun_str + ".mid.lz4");
            
            std::cout << "[INFO] Checking subrun " << subrun << "... ";
            std::cout.flush();
            
            if (hasCorruption(subrun_file)) {
                analysis.corrupted_subruns.insert(subrun);
                ++corrupted_count;
                std::cout << "CORRUPTED\n";
            } else {
                std::cout << "OK\n";
            }
        }
    }
    
    // Calculate corruption percentage based on total possible subruns (0 to max_subrun)
    size_t total_possible_subruns = analysis.max_subrun + 1;
    if (total_possible_subruns > 0) {
        analysis.corruption_percentage = (double)corrupted_count / total_possible_subruns * 100.0;
    }
    
    std::cout << "[INFO] Checked " << checked_subruns << " subruns from " << analysis.first_corrupted_subrun 
              << " onwards\n";
    std::cout << "[INFO] Found " << corrupted_count << " corrupted subruns\n";
    std::cout << "[INFO] Total possible subruns (0-" << analysis.max_subrun << "): " << total_possible_subruns << "\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <results.txt_path>\n";
        std::cerr << "  results.txt_path: Path to the results.txt file\n";
        std::cerr << "  Note: This tool checks each subrun individually for corruption density\n";
        return EXIT_FAILURE;
    }

    fs::path results_path = argv[1];
    
    if (!fs::exists(results_path)) {
        std::cerr << "[ERROR] Results file does not exist: " << results_path << "\n";
        return EXIT_FAILURE;
    }

    // Parse results file
    std::vector<RunSubrunAnalysis> analyses = parseResultsFile(results_path);
    
    if (analyses.empty()) {
        std::cout << "[INFO] No corruptions found in results file.\n";
        return EXIT_SUCCESS;
    }
    
    std::cout << "[INFO] Found " << analyses.size() << " corrupted runs in results file.\n\n";
    
    // Analyze each corrupted run
    for (auto& analysis : analyses) {
        analyzeAllSubruns(analysis, results_path);
        std::cout << "\n";
    }
    
    // Generate summary report
    std::cout << "=== SUBRUN CORRUPTION DENSITY ANALYSIS ===\n\n";
    std::cout << "Run#  | First@Sub | MaxSub | Total Subs | Corrupted | Corruption % of Total\n";
    std::cout << "------|-----------|--------|------------|-----------|----------------------\n";
    
    for (const auto& analysis : analyses) {
        size_t total_possible_subruns = analysis.max_subrun + 1;
        
        std::cout << std::setw(5) << analysis.run_number << " | "
                  << std::setw(9) << analysis.first_corrupted_subrun << " | "
                  << std::setw(6) << analysis.max_subrun << " | "
                  << std::setw(10) << total_possible_subruns << " | "
                  << std::setw(9) << analysis.corrupted_subruns.size() << " | "
                  << std::setw(20) << std::fixed << std::setprecision(2) 
                  << analysis.corruption_percentage << "%\n";
    }
    
    // Write detailed report to file
    std::ofstream report("subrun_corruption_analysis.txt");
    if (report.is_open()) {
        report << "=== DETAILED SUBRUN CORRUPTION ANALYSIS ===\n\n";
        
        for (const auto& analysis : analyses) {
            report << "Run " << analysis.run_number << ":\n";
            report << "  First corrupted subrun: " << analysis.first_corrupted_subrun << "\n";
            report << "  Maximum subrun: " << analysis.max_subrun << "\n";
            report << "  Total subruns found: " << analysis.all_subruns.size() << "\n";
            
            int checked_subruns = 0;
            for (int subrun : analysis.all_subruns) {
                if (subrun >= analysis.first_corrupted_subrun) {
                    checked_subruns++;
                }
            }
            report << "  Subruns checked (from first corruption): " << checked_subruns << "\n";
            report << "  Corrupted subruns found: " << analysis.corrupted_subruns.size() << "\n";
            report << "  Corruption percentage of total subruns: " << std::fixed << std::setprecision(2)
                   << analysis.corruption_percentage << "%\n";
            
            report << "  Corrupted subrun list: ";
            bool first = true;
            for (int subrun : analysis.corrupted_subruns) {
                if (!first) report << ", ";
                report << subrun;
                first = false;
            }
            report << "\n\n";
        }
        
        report.close();
        std::cout << "\n[INFO] Detailed subrun analysis written to subrun_corruption_analysis.txt\n";
    }
    
    return EXIT_SUCCESS;
}