#include <iostream>
#include <filesystem>
#include <memory>
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
        for (int i = 0; i < 8; ++i)
            header = (header << 8) | static_cast<uint8_t>(data[i]);
        uint32_t trigger = (header >> 32) & 0xFFFFFF;

        if (bank.name == "CR07") { trigger_CR07 = trigger; found_CR07 = true; }
        if (bank.name == "CR08") { trigger_CR08 = trigger; found_CR08 = true; }
    }

    return (found_CR07 && found_CR08) && (trigger_CR07 != trigger_CR08);
}

// Inspect one file, print result and return true if mismatch found
bool inspectFile(const fs::path& filepath) {
    std::cout << "[INFO] Opening file: " << filepath << std::endl;
    
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[ERROR] Failed to open file: " << filepath << std::endl;
        return false;
    }

    size_t event_count = 0;
    bool desync_found = false;

    while (TMEvent* raw_event = TMReadEvent(reader)) {
        std::shared_ptr<TMEvent> evt(raw_event);
        event_count++;
        
        if (hasTriggerMismatch(evt)) {
            std::cout << "[RESULT] DESYNC DETECTED at event " << event_count << std::endl;
            desync_found = true;
            break; // Stop at first desync
        }
        
        // Print progress for large files
        if (event_count % 10000 == 0) {
            std::cout << "[INFO] Processed " << event_count << " events..." << std::endl;
        }
    }

    delete reader;
    
    std::cout << "[INFO] Processed " << event_count << " total events" << std::endl;
    
    if (desync_found) {
        std::cout << "[RESULT] FILE HAS DESYNC" << std::endl;
    } else {
        std::cout << "[RESULT] NO DESYNC FOUND" << std::endl;
    }
    
    return desync_found;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file_path>" << std::endl;
        std::cerr << "       Inspect a single .mid.lz4 file for trigger desyncs" << std::endl;
        return EXIT_FAILURE;
    }

    fs::path filepath = argv[1];
    
    if (!fs::exists(filepath)) {
        std::cerr << "[ERROR] File does not exist: " << filepath << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!fs::is_regular_file(filepath)) {
        std::cerr << "[ERROR] Path is not a regular file: " << filepath << std::endl;
        return EXIT_FAILURE;
    }

    bool has_desync = inspectFile(filepath);
    
    // Return exit code: 0 if no desync, 1 if desync found, 2 for other errors
    return has_desync ? 1 : 0;
}