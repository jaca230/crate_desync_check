#include <iostream>
#include <filesystem>
#include <string>
#include <memory>
#include <cstdint>
#include <fstream>
#include <vector>
#include <algorithm>
#include <regex>
#include "midasio.h"

namespace fs = std::filesystem;

struct CrateInfo {
    uint32_t trigger = 0;
    uint16_t gpu_fill = 0;
    uint16_t tcp_fill = 0;
    bool has_trigger = false;
    bool has_fills = false;
};

struct RunFile {
    int run;
    int subrun;
    fs::path path;
};

CrateInfo parseBank(const std::shared_ptr<TMEvent>& event, const char* crBankName, const char* ccBankName) {
    CrateInfo info;
    event->FindAllBanks();
    
    // First, look for CR bank to get trigger number
    for (const auto& bank : event->banks) {
        if (bank.name != crBankName) continue;
        
        const char* data = event->GetBankData(&bank);
        if (!data || bank.data_size < 8) continue;
        
        // First 8 bytes hold header → trigger number in high 24 bits
        uint64_t header = 0;
        for (int i = 0; i < 8; ++i)
            header = (header << 8) | static_cast<uint8_t>(data[i]);
        info.trigger = (header >> 32) & 0xFFFFFF;
        info.has_trigger = true;
        break;
    }
    
    // Then, look for CC bank to get TCP/GPU fills
    for (const auto& bank : event->banks) {
        if (bank.name != ccBankName) continue;
        
        const char* data = event->GetBankData(&bank);
        if (!data || bank.data_size < 16) continue;
        
        // For CC banks, get the last 4 32-bit words
        const uint32_t* words = reinterpret_cast<const uint32_t*>(data);
        size_t num_words = bank.data_size / 4;
        
        if (num_words >= 4) {
            // Last 2 words are GPU fill (take the first non-zero one)
            uint32_t gpu_word1 = words[num_words - 2];
            uint32_t gpu_word2 = words[num_words - 1];
            info.gpu_fill = (gpu_word1 != 0) ? (gpu_word1 & 0xFFFF) : (gpu_word2 & 0xFFFF);
            
            // 2 words before last 2 are TCP fill (take the first non-zero one)
            uint32_t tcp_word1 = words[num_words - 4];
            uint32_t tcp_word2 = words[num_words - 3];
            info.tcp_fill = (tcp_word1 != 0) ? (tcp_word1 & 0xFFFF) : (tcp_word2 & 0xFFFF);
            
            info.has_fills = true;
        }
        break;
    }
    
    return info;
}

bool processFile(const fs::path& filepath, std::ofstream& logFile, int& globalEventIndex) {
    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << "\n";
        return false;
    }

    int fileEventNum = 0;
    while (TMEvent* raw_event = TMReadEvent(reader)) {
        std::shared_ptr<TMEvent> evt(raw_event);
        ++fileEventNum;
        ++globalEventIndex;

        // Parse crate 7: get trigger from CR07, fills from CC07
        CrateInfo crate7 = parseBank(evt, "CR07", "CC07");
        
        // Parse crate 8: get trigger from CR08, fills from CC08
        CrateInfo crate8 = parseBank(evt, "CR08", "CC08");

        // Log in CSV format for easy plotting
        logFile << globalEventIndex << ","
                << (crate7.has_trigger ? std::to_string(crate7.trigger) : "NA") << ","
                << (crate7.has_fills ? std::to_string(crate7.tcp_fill) : "NA") << ","
                << (crate7.has_fills ? std::to_string(crate7.gpu_fill) : "NA") << ","
                << (crate8.has_trigger ? std::to_string(crate8.trigger) : "NA") << ","
                << (crate8.has_fills ? std::to_string(crate8.tcp_fill) : "NA") << ","
                << (crate8.has_fills ? std::to_string(crate8.gpu_fill) : "NA") << ","
                << filepath.filename().string() << "\n";
    }

    delete reader;
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <base_path> <run_number>\n";
        std::cerr << "Example: " << argv[0] << " ../midas_files 322\n";
        return EXIT_FAILURE;
    }

    fs::path basePath = argv[1];
    int targetRun = std::stoi(argv[2]);

    if (!fs::exists(basePath)) {
        std::cerr << "[ERROR] Base path does not exist: " << basePath << "\n";
        return EXIT_FAILURE;
    }

    // Find all files matching the target run
    std::regex pattern(R"(run)" + std::string(5 - std::to_string(targetRun).length(), '0') + 
                      std::to_string(targetRun) + R"(_(\d{5})\.mid\.lz4$)");
    std::vector<RunFile> files;

    for (auto& entry : fs::recursive_directory_iterator(basePath)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(fname, match, pattern)) {
            int subrun = std::stoi(match[1].str());
            files.push_back({targetRun, subrun, entry.path()});
        }
    }

    if (files.empty()) {
        std::cout << "[INFO] No matching .mid.lz4 files found for run " << targetRun << "\n";
        return EXIT_SUCCESS;
    }

    // Sort by subrun
    std::sort(files.begin(), files.end(),
              [](const RunFile& a, const RunFile& b) {
                  return a.subrun < b.subrun;
              });

    // Create output file
    std::string logFilename = "run" + std::string(5 - std::to_string(targetRun).length(), '0') + 
                             std::to_string(targetRun) + "_crate_data.csv";
    std::ofstream logFile(logFilename);
    if (!logFile.is_open()) {
        std::cerr << "[ERROR] Could not create log file: " << logFilename << "\n";
        return EXIT_FAILURE;
    }

    // Write CSV header - simplified to reflect actual data structure
    logFile << "event_index,crate7_trigger,crate7_tcp_fill,crate7_gpu_fill,"
            << "crate8_trigger,crate8_tcp_fill,crate8_gpu_fill,filename\n";

    std::cout << "[INFO] Processing run " << targetRun << " with " << files.size() << " subruns\n";
    std::cout << "[INFO] Logging to: " << logFilename << "\n";

    int globalEventIndex = 0;
    int processedFiles = 0;

    for (const auto& rf : files) {
        std::cout << "[PROGRESS] Processing subrun " << rf.subrun 
                  << " (" << (processedFiles + 1) << "/" << files.size() << "): "
                  << rf.path.filename() << std::flush;

        int eventsBefore = globalEventIndex;
        if (processFile(rf.path, logFile, globalEventIndex)) {
            int eventsInFile = globalEventIndex - eventsBefore;
            std::cout << " - " << eventsInFile << " events processed\n";
            ++processedFiles;
        } else {
            std::cout << " - FAILED\n";
        }
    }

    std::cout << "[SUMMARY] Processed " << processedFiles << " subruns, "
              << globalEventIndex << " total events\n";
    std::cout << "[SUMMARY] Data logged to: " << logFilename << "\n";

    return EXIT_SUCCESS;
}