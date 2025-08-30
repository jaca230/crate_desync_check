#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <regex>
#include <map>
#include <sstream>
#include <iomanip>

#include "midasio.h"

namespace fs = std::filesystem;

struct RunCorruption {
    int run_number;
    fs::path corrupted_file_path;
    int corrupted_subrun;
    int max_subrun = -1;
    size_t events_before_corruption = 0;
    size_t total_events = 0;
    double corruption_percentage = 0.0;
};

// Count events in a single file
size_t countEventsInFile(const fs::path& filepath) {
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << " for event counting\n";
        return 0;
    }

    size_t event_count = 0;
    while (TMEvent* raw_event = TMReadEvent(reader)) {
        ++event_count;
        // Clean up the event
        delete raw_event;
    }
    delete reader;
    return event_count;
}

// Parse results.txt to extract corrupted runs
std::vector<RunCorruption> parseResultsFile(const fs::path& results_path) {
    std::vector<RunCorruption> corruptions;
    std::ifstream file(results_path);
    
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open " << results_path << "\n";
        return corruptions;
    }

    std::regex pattern(R"(Run (\d+) desync in file: (.+))");
    std::string line;
    
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            RunCorruption corruption;
            corruption.run_number = std::stoi(match[1].str());
            corruption.corrupted_file_path = match[2].str();
            
            // Extract subrun from filename
            std::regex subrun_pattern(R"(run\d{5}_(\d{5})\.mid\.lz4$)");
            std::string filename = corruption.corrupted_file_path.filename().string();
            std::smatch subrun_match;
            if (std::regex_search(filename, subrun_match, subrun_pattern)) {
                corruption.corrupted_subrun = std::stoi(subrun_match[1].str());
            }
            
            corruptions.push_back(corruption);
        }
    }
    
    return corruptions;
}

// Find all files for a given run and determine max subrun
void analyzeRunFiles(RunCorruption& corruption) {
    // Extract data directory from the corrupted file path
    fs::path data_directory = corruption.corrupted_file_path.parent_path();
    // Create pattern for run number with proper zero-padding
    std::string run_str = std::to_string(corruption.run_number);
    run_str = std::string(5 - run_str.length(), '0') + run_str; // Zero-pad to 5 digits
    std::regex pattern(R"(run)" + run_str + R"(_(\d{5})\.mid\.lz4$)");
    std::vector<int> subruns; // Just collect subrun numbers
    
    // Search for all files matching this run to find max subrun
    for (auto& entry : fs::recursive_directory_iterator(data_directory)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_search(filename, match, pattern)) {
            int subrun = std::stoi(match[1].str());
            subruns.push_back(subrun);
        }
    }
    
    if (subruns.empty()) {
        std::cerr << "[WARN] No files found for run " << corruption.run_number << "\n";
        return;
    }
    
    // Find max subrun
    corruption.max_subrun = *std::max_element(subruns.begin(), subruns.end());
    
    std::cout << "[INFO] Analyzing run " << corruption.run_number 
              << " (corrupted at subrun " << corruption.corrupted_subrun
              << ", max subrun " << corruption.max_subrun << ")\n";
    
    // Count events only in the corrupted file
    std::cout << "[INFO] Counting events in corrupted file: " << corruption.corrupted_file_path.filename() << "\n";
    size_t events_per_subrun = countEventsInFile(corruption.corrupted_file_path);
    
    if (events_per_subrun == 0) {
        std::cerr << "[WARN] Could not count events in corrupted file, skipping calculations\n";
        return;
    }
    
    std::cout << "[INFO] Events per subrun (estimated): " << events_per_subrun << "\n";
    
    // Calculate estimates assuming all subruns have same number of events
    // Total subruns = max_subrun + 1 (assuming starts at 0)
    size_t total_subruns = corruption.max_subrun + 1;
    corruption.total_events = total_subruns * events_per_subrun;
    corruption.events_before_corruption = corruption.corrupted_subrun * events_per_subrun;
    
    if (corruption.total_events > 0) {
        corruption.corruption_percentage = 
            (double)(corruption.total_events - corruption.events_before_corruption) / corruption.total_events * 100.0;
    }
    
    std::cout << "[INFO] Estimated total subruns: " << total_subruns << "\n";
    std::cout << "[INFO] Estimated events before corruption: " << corruption.events_before_corruption << "\n";
    std::cout << "[INFO] Estimated total events: " << corruption.total_events << "\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <results.txt_path>\n";
        std::cerr << "  results.txt_path: Path to the results.txt file\n";
        std::cerr << "  Note: Data directory is automatically extracted from file paths in results.txt\n";
        return EXIT_FAILURE;
    }

    fs::path results_path = argv[1];
    
    if (!fs::exists(results_path)) {
        std::cerr << "[ERROR] Results file does not exist: " << results_path << "\n";
        return EXIT_FAILURE;
    }

    // Parse results file
    std::vector<RunCorruption> corruptions = parseResultsFile(results_path);
    
    if (corruptions.empty()) {
        std::cout << "[INFO] No corruptions found in results file.\n";
        return EXIT_SUCCESS;
    }
    
    std::cout << "[INFO] Found " << corruptions.size() << " corrupted runs in results file.\n\n";
    
    // Analyze each corrupted run
    for (auto& corruption : corruptions) {
        analyzeRunFiles(corruption);
        std::cout << "\n";
    }
    
    // Generate summary report
    std::cout << "=== CORRUPTION ANALYSIS SUMMARY ===\n\n";
    std::cout << "Run#  | Corrupt@Sub | MaxSub | Events Before | Total Events | Max Est. Corrupt %\n";
    std::cout << "------|-------------|--------|---------------|--------------|-------------------\n";
    
    for (const auto& corruption : corruptions) {
        std::cout << std::setw(5) << corruption.run_number << " | "
                  << std::setw(11) << corruption.corrupted_subrun << " | "
                  << std::setw(6) << corruption.max_subrun << " | "
                  << std::setw(13) << corruption.events_before_corruption << " | "
                  << std::setw(12) << corruption.total_events << " | "
                  << std::setw(17) << std::fixed << std::setprecision(2) 
                  << corruption.corruption_percentage << "%\n";
    }
    
    // Write detailed report to file
    std::ofstream report("corruption_analysis.txt");
    if (report.is_open()) {
        report << "=== DETAILED CORRUPTION ANALYSIS ===\n\n";
        
        for (const auto& corruption : corruptions) {
            report << "Run " << corruption.run_number << ":\n";
            report << "  Corrupted file: " << corruption.corrupted_file_path.string() << "\n";
            report << "  Corrupted at subrun: " << corruption.corrupted_subrun << "\n";
            report << "  Maximum subrun: " << corruption.max_subrun << "\n";
            report << "  Events before corruption: " << corruption.events_before_corruption << "\n";
            report << "  Total events in run: " << corruption.total_events << "\n";
            report << "  Maximum estimated corruption: " << std::fixed << std::setprecision(2)
                   << corruption.corruption_percentage << "%\n";
            report << "  Note: This is a maximum estimate - data may have recovered in later subruns\n\n";
        }
        
        report.close();
        std::cout << "\n[INFO] Detailed report written to corruption_analysis.txt\n";
    }
    
    return EXIT_SUCCESS;
}