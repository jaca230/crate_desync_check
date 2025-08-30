#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <regex>
#include <iomanip>
#include <chrono>

#include "midasio.h"

namespace fs = std::filesystem;

struct SubrunResult {
    int subrun_number;
    bool has_desync;
    size_t event_count;
    size_t first_desync_event;
    std::string status;
    std::string filepath;
};

struct RunInspection {
    int run_number;
    std::vector<SubrunResult> subruns;
    int total_subruns;
    int corrupted_subruns;
    double corruption_percentage;
};

// Extract triggers from CR07/CR08 - same function as other tools
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

// Inspect a single subrun file
SubrunResult inspectSubrun(const fs::path& filepath, int expected_subrun) {
    SubrunResult result;
    result.subrun_number = expected_subrun;
    result.has_desync = false;
    result.event_count = 0;
    result.first_desync_event = 0;
    result.filepath = filepath.string();
    
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        result.status = "FAILED_TO_OPEN";
        return result;
    }

    while (TMEvent* raw_event = TMReadEvent(reader)) {
        std::shared_ptr<TMEvent> evt(raw_event);
        result.event_count++;
        
        if (!result.has_desync && hasTriggerMismatch(evt)) {
            result.has_desync = true;
            result.first_desync_event = result.event_count;
        }
    }

    delete reader;
    
    if (result.has_desync) {
        result.status = "DESYNC_FOUND";
    } else {
        result.status = "OK";
    }
    
    return result;
}

// Find all subrun files for a given run
std::vector<fs::path> findSubrunFiles(const fs::path& base_dir, int run_number) {
    std::vector<fs::path> subrun_files;
    
    // Create run string with zero-padding
    std::string run_str = std::to_string(run_number);
    run_str = std::string(5 - run_str.length(), '0') + run_str;
    
    // Pattern to match run files: runXXXXX_YYYYY.mid.lz4
    std::regex pattern(R"(run)" + run_str + R"(_(\d{5})\.mid\.lz4$)");
    
    if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) {
        std::cerr << "[ERROR] Base directory does not exist or is not a directory: " << base_dir << "\n";
        return subrun_files;
    }
    
    // Scan directory for matching files
    for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (std::regex_search(filename, pattern)) {
            subrun_files.push_back(entry.path());
        }
    }
    
    // Sort files by subrun number
    std::sort(subrun_files.begin(), subrun_files.end(), [&](const fs::path& a, const fs::path& b) {
        std::smatch match_a, match_b;
        std::string filename_a = a.filename().string();
        std::string filename_b = b.filename().string();
        
        if (std::regex_search(filename_a, match_a, pattern) && 
            std::regex_search(filename_b, match_b, pattern)) {
            return std::stoi(match_a[1].str()) < std::stoi(match_b[1].str());
        }
        return false;
    });
    
    return subrun_files;
}

// Extract subrun number from filename
int extractSubrunNumber(const fs::path& filepath, int run_number) {
    std::string run_str = std::to_string(run_number);
    run_str = std::string(5 - run_str.length(), '0') + run_str;
    
    std::regex pattern(R"(run)" + run_str + R"(_(\d{5})\.mid\.lz4$)");
    std::string filename = filepath.filename().string();
    std::smatch match;
    
    if (std::regex_search(filename, match, pattern)) {
        return std::stoi(match[1].str());
    }
    
    return -1; // Invalid
}

// Generate output filename
std::string generateOutputFilename(int run_number, const std::string& base_name = "run_inspection") {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << base_name << "_run" << std::setfill('0') << std::setw(5) << run_number 
        << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
    return oss.str();
}

// Write detailed report
void writeReport(const RunInspection& inspection, const std::string& output_file) {
    std::ofstream report(output_file);
    if (!report.is_open()) {
        std::cerr << "[ERROR] Could not create output file: " << output_file << "\n";
        return;
    }
    
    // Header
    report << "=== RUN " << inspection.run_number << " INSPECTION REPORT ===\n";
    auto now = std::time(nullptr);
    report << "Generated: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    // Summary
    report << "SUMMARY:\n";
    report << "  Total subruns found: " << inspection.total_subruns << "\n";
    report << "  Corrupted subruns: " << inspection.corrupted_subruns << "\n";
    report << "  Corruption percentage: " << std::fixed << std::setprecision(2) 
           << inspection.corruption_percentage << "%\n\n";
    
    // Detailed results
    report << "DETAILED RESULTS:\n";
    report << "Subrun | Status       | Events | First Desync | File Path\n";
    report << "-------|--------------|--------|--------------|----------------------------------------------------------\n";
    
    for (const auto& subrun : inspection.subruns) {
        report << std::setw(6) << subrun.subrun_number << " | ";
        report << std::setw(12) << std::left << subrun.status << " | ";
        report << std::setw(6) << std::right << subrun.event_count << " | ";
        
        if (subrun.has_desync) {
            report << std::setw(12) << subrun.first_desync_event << " | ";
        } else {
            report << std::setw(12) << "N/A" << " | ";
        }
        
        report << subrun.filepath << "\n";
    }
    
    // Corrupted subruns list
    if (inspection.corrupted_subruns > 0) {
        report << "\nCORRUPTED SUBRUNS LIST:\n";
        bool first = true;
        for (const auto& subrun : inspection.subruns) {
            if (subrun.has_desync) {
                if (!first) report << ", ";
                report << subrun.subrun_number;
                first = false;
            }
        }
        report << "\n";
    }
    
    report.close();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <base_directory> <run_number>\n";
        std::cerr << "  base_directory: Directory to search for run files\n";
        std::cerr << "  run_number: Run number to inspect (e.g., 12345)\n";
        std::cerr << "\nThis tool will:\n";
        std::cerr << "  1. Find all subruns for the specified run\n";
        std::cerr << "  2. Check each subrun for trigger desyncs\n";
        std::cerr << "  3. Write a detailed report to a timestamped file\n";
        return EXIT_FAILURE;
    }

    fs::path base_dir = argv[1];
    int run_number;
    
    try {
        run_number = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Invalid run number: " << argv[2] << "\n";
        return EXIT_FAILURE;
    }
    
    if (run_number <= 0) {
        std::cerr << "[ERROR] Run number must be positive: " << run_number << "\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "[INFO] Inspecting run " << run_number << " in directory: " << base_dir << "\n";
    
    // Find all subrun files
    std::vector<fs::path> subrun_files = findSubrunFiles(base_dir, run_number);
    
    if (subrun_files.empty()) {
        std::cout << "[INFO] No subrun files found for run " << run_number << "\n";
        return EXIT_SUCCESS;
    }
    
    std::cout << "[INFO] Found " << subrun_files.size() << " subrun files\n";
    
    // Initialize inspection results
    RunInspection inspection;
    inspection.run_number = run_number;
    inspection.total_subruns = subrun_files.size();
    inspection.corrupted_subruns = 0;
    
    // Inspect each subrun
    std::cout << "[INFO] Starting inspection...\n\n";
    
    for (size_t i = 0; i < subrun_files.size(); ++i) {
        const auto& file = subrun_files[i];
        int subrun_num = extractSubrunNumber(file, run_number);
        
        std::cout << "[" << (i+1) << "/" << subrun_files.size() << "] ";
        std::cout << "Inspecting subrun " << subrun_num << "... ";
        std::cout.flush();
        
        SubrunResult result = inspectSubrun(file, subrun_num);
        inspection.subruns.push_back(result);
        
        if (result.has_desync) {
            inspection.corrupted_subruns++;
            std::cout << "CORRUPTED (first desync at event " << result.first_desync_event << ")\n";
        } else if (result.status == "FAILED_TO_OPEN") {
            std::cout << "FAILED TO OPEN\n";
        } else {
            std::cout << "OK (" << result.event_count << " events)\n";
        }
    }
    
    // Calculate corruption percentage
    if (inspection.total_subruns > 0) {
        inspection.corruption_percentage = (double)inspection.corrupted_subruns / inspection.total_subruns * 100.0;
    }
    
    std::cout << "\n=== INSPECTION COMPLETE ===\n";
    std::cout << "Total subruns: " << inspection.total_subruns << "\n";
    std::cout << "Corrupted subruns: " << inspection.corrupted_subruns << "\n";
    std::cout << "Corruption percentage: " << std::fixed << std::setprecision(2) 
              << inspection.corruption_percentage << "%\n";
    
    // Write report
    std::string output_file = generateOutputFilename(run_number);
    writeReport(inspection, output_file);
    
    std::cout << "\n[INFO] Detailed report written to: " << output_file << "\n";
    
    // Exit with appropriate code
    return (inspection.corrupted_subruns > 0) ? 1 : 0;
}