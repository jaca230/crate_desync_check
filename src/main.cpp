#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>

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
        for (int i = 0; i < 8; ++i) header = (header << 8) | static_cast<uint8_t>(data[i]);
        uint32_t trigger = (header >> 32) & 0xFFFFFF;

        if (bank.name == "CR07") { trigger_CR07 = trigger; found_CR07 = true; }
        if (bank.name == "CR08") { trigger_CR08 = trigger; found_CR08 = true; }
    }

    return (found_CR07 && found_CR08) && (trigger_CR07 != trigger_CR08);
}

// Check a single subrun file
bool subrunHasMismatch(const fs::path& filepath) {
    std::cout << "[INFO] Processing: " << filepath << "\n";
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << "\n";
        return false;
    }

    std::shared_ptr<TMEvent> first_event = nullptr;
    std::shared_ptr<TMEvent> last_event = nullptr;
    size_t event_count = 0;

    while (TMEvent* raw_event = TMReadEvent(reader)) {
        ++event_count;
        std::shared_ptr<TMEvent> wrapped_event(raw_event);

        // Skip absolute first event
        if (event_count == 1) continue;

        wrapped_event->FindAllBanks();
        bool has_CR07 = false, has_CR08 = false;
        for (const auto& bank : wrapped_event->banks) {
            if (bank.name == "CR07") has_CR07 = true;
            if (bank.name == "CR08") has_CR08 = true;
        }
        if (has_CR07 && has_CR08) {
            if (!first_event) first_event = wrapped_event;
            last_event = wrapped_event;
        }
    }
    delete reader;

    if (!first_event && !last_event) return false;

    bool mismatch_first = first_event && hasTriggerMismatch(first_event);
    bool mismatch_last  = last_event  && hasTriggerMismatch(last_event);

    return mismatch_first || mismatch_last;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <directory> <run_number>\n"
                  << "  " << argv[0] << " <single_midas_file>\n";
        return EXIT_FAILURE;
    }

    fs::path path_arg = argv[1];
    if (!fs::exists(path_arg)) {
        std::cerr << "[ERROR] Path does not exist: " << path_arg << "\n";
        return EXIT_FAILURE;
    }

    std::vector<fs::path> files_to_check;

    if (argc == 3) {
        // Directory + run_number mode
        std::string run_str = argv[2];
        if (!fs::is_directory(path_arg)) {
            std::cerr << "[ERROR] Expected directory, got: " << path_arg << "\n";
            return EXIT_FAILURE;
        }

        for (auto& entry : fs::directory_iterator(path_arg)) {
            if (!entry.is_regular_file()) continue;
            std::string fname = entry.path().filename().string();
            if (fname.find("run" + run_str + "_") == 0) files_to_check.push_back(entry.path());
        }
        if (files_to_check.empty()) {
            std::cerr << "[ERROR] No subruns found for run " << run_str << "\n";
            return EXIT_FAILURE;
        }
        std::sort(files_to_check.begin(), files_to_check.end());
    } else {
        // Single file mode
        files_to_check.push_back(path_arg);
    }

    // If multiple files, do binary search for earliest mismatch
    if (files_to_check.size() > 1) {
        int left = 0, right = files_to_check.size() - 1;
        int earliest_idx = -1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (subrunHasMismatch(files_to_check[mid])) {
                earliest_idx = mid;
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }
        if (earliest_idx >= 0) {
            std::cout << "[RESULT] Earliest subrun with trigger mismatch: " 
                      << files_to_check[earliest_idx] << "\n";
        } else {
            std::cout << "[RESULT] No trigger mismatches found\n";
        }
    } else {
        // Single file
        if (subrunHasMismatch(files_to_check[0]))
            std::cout << "[RESULT] Trigger mismatch found in " << files_to_check[0] << "\n";
        else
            std::cout << "[RESULT] No trigger mismatch in " << files_to_check[0] << "\n";
    }

    return EXIT_SUCCESS;
}
