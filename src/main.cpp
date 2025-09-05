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

#include "midasio.h"

namespace fs = std::filesystem;

// Extract triggers from CR07/CR08
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

// Scan one file, return true if mismatch found
bool scanFile(const fs::path& filepath) {
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

struct RunFile {
    int run;
    int subrun;
    fs::path path;
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <repo_or_data_directory>\n";
        return EXIT_FAILURE;
    }

    fs::path root = argv[1];
    if (!fs::exists(root)) {
        std::cerr << "[ERROR] Path does not exist: " << root << "\n";
        return EXIT_FAILURE;
    }

    std::ofstream log("results.txt", std::ios::app);
    if (!log.is_open()) {
        std::cerr << "[ERROR] Could not open results.txt for writing\n";
        return EXIT_FAILURE;
    }

    std::regex pattern(R"(run(\d{5})_(\d{5})\.mid\.lz4$)");
    std::vector<RunFile> files;

    // Collect valid files
    for (auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(fname, match, pattern)) {
            int run = std::stoi(match[1].str());
            int subrun = std::stoi(match[2].str());
            files.push_back({run, subrun, entry.path()});
        }
    }

    if (files.empty()) {
        std::cout << "[INFO] No matching .mid.lz4 files found.\n";
        return EXIT_SUCCESS;
    }

    // Sort by run then subrun
    std::sort(files.begin(), files.end(),
              [](const RunFile& a, const RunFile& b) {
                  if (a.run != b.run) return a.run < b.run;
                  return a.subrun < b.subrun;
              });

    size_t scanned_files = 0;
    size_t files_with_mismatch = 0;
    size_t runs_with_mismatch = 0;
    std::set<int> all_runs; // Track all unique runs
    int current_run = -1;
    bool run_has_mismatch = false;

    for (const auto& rf : files) {
        all_runs.insert(rf.run); // Track this run

        if (rf.run != current_run) {
            // Moving to a new run
            if (current_run != -1 && run_has_mismatch) {
                ++runs_with_mismatch;
            }
            current_run = rf.run;
            run_has_mismatch = false;
        }

        // Skip remaining subruns if this run already has a mismatch
        if (run_has_mismatch) {
            continue;
        }

        ++scanned_files;
        std::cout << "[INFO] Scanning run " << rf.run
                  << ", subrun " << rf.subrun
                  << " (" << rf.path << ")\n";

        if (scanFile(rf.path)) {
            ++files_with_mismatch;
            run_has_mismatch = true;
            log << "Run " << rf.run
                << " desync in file: " << rf.path.string()
                << "\n" << std::flush;
            std::cout << "[RESULT] Run " << rf.run
                      << " has desync (found in " << rf.path << ")\n";
        }
    }

    // Account for the last run
    if (current_run != -1 && run_has_mismatch) {
        ++runs_with_mismatch;
    }

    // Summary
    std::string summary = "[SUMMARY] Scanned " + std::to_string(scanned_files) +
                          " files across " + std::to_string(all_runs.size()) +
                          " runs, " + std::to_string(runs_with_mismatch) +
                          " of " + std::to_string(all_runs.size()) + " runs had desyncs.\n";

    std::cout << summary;
    log << summary << std::flush;

    return EXIT_SUCCESS;
}

