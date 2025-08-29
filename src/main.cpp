#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>

#include "midasio.h"

namespace fs = std::filesystem;

// Compare raw bank data
bool banksEqual(const TMBank& b1, const char* data1, const TMBank& b2, const char* data2) {
    if (!data1 || !data2) return false;
    if (b1.data_size != b2.data_size) return false;
    for (size_t i = 0; i < b1.data_size; ++i) {
        if (static_cast<uint8_t>(data1[i]) != static_cast<uint8_t>(data2[i])) return false;
    }
    return true;
}

// Write a bank to a text file, 32 bytes per line
void writeBankTextFile(const TMEvent& evt, const TMBank& bank, size_t eventNum, const std::string& label) {
    const char* data = evt.GetBankData(&bank);
    if (!data || bank.data_size == 0) return;

    std::ostringstream fname;
    fname << label << "_event" << eventNum << "_" << bank.name << ".txt";

    std::ofstream out(fname.str());
    out << "# Event " << eventNum << ", Bank " << bank.name << "\n";
    out << "# " << label << "\n";

    for (size_t i = 0; i < bank.data_size; ++i) {
        if (i % 32 == 0 && i != 0) out << "\n";
        out << std::hex << std::setw(2) << std::setfill('0')
            << (static_cast<int>(static_cast<unsigned char>(data[i]))) << " ";
    }
    out << "\n";
    out.close();

    std::cout << "[INFO] Wrote bank " << bank.name << " of event "
              << eventNum << " to " << fname.str() << "\n";
}

// Check a single subrun file for duplicate triggers
void checkDuplicateTriggers(const fs::path& filepath) {
    std::cout << "[INFO] Processing: " << filepath << "\n";

    TMReaderInterface* reader = TMNewReader(filepath.c_str());
    if (!reader) {
        std::cerr << "[WARN] Failed to open " << filepath << "\n";
        return;
    }

    std::map<uint32_t, std::pair<std::shared_ptr<TMEvent>, size_t>> cr07Map;
    std::map<uint32_t, std::pair<std::shared_ptr<TMEvent>, size_t>> cr08Map;

    size_t eventCounter = 0;
    bool firstDuplicateWritten = false;

    while (TMEvent* raw_event = TMReadEvent(reader)) {
        ++eventCounter;
        std::shared_ptr<TMEvent> evt(raw_event);
        evt->FindAllBanks();

        for (const std::string& bankName : {"CR07", "CR08"}) {
            TMBank* bankPtr = evt->FindBank(bankName.c_str());
            if (!bankPtr || bankPtr->data_size < 8) continue;

            const char* data = evt->GetBankData(bankPtr);
            if (!data) continue;

            uint64_t header = 0;
            for (int i = 0; i < 8; ++i) header = (header << 8) | static_cast<uint8_t>(data[i]);
            uint32_t triggerNum = static_cast<uint32_t>((header >> 32) & 0xFFFFFF);

            auto& triggerMap = (bankName == "CR07") ? cr07Map : cr08Map;
            auto it = triggerMap.find(triggerNum);

            if (it != triggerMap.end() && !firstDuplicateWritten) {
                std::cout << "[DUPLICATE] Bank " << bankName << " trigger " << triggerNum
                          << " appears in events " << it->second.second
                          << " and " << eventCounter << "\n";

                // Write original and duplicate for both CR07 and CR08
                for (const std::string& bName : {"CR07", "CR08"}) {
                    TMBank* origBank = it->second.first->FindBank(bName.c_str());
                    TMBank* dupBank  = evt->FindBank(bName.c_str());
                    if (origBank) writeBankTextFile(*it->second.first, *origBank, it->second.second, "original_" + bName);
                    if (dupBank)  writeBankTextFile(*evt, *dupBank, eventCounter, "duplicate_" + bName);
                }

                firstDuplicateWritten = true;
            }

            if (it == triggerMap.end()) {
                triggerMap[triggerNum] = {evt, eventCounter};
            }
        }
    }

    delete reader;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <directory> <run_number>\n"
                  << "  " << argv[0] << " <single_midas_file>\n";
        return EXIT_FAILURE;
    }

    fs::path pathArg = argv[1];
    if (!fs::exists(pathArg)) {
        std::cerr << "[ERROR] Path does not exist: " << pathArg << "\n";
        return EXIT_FAILURE;
    }

    std::vector<fs::path> filesToCheck;
    if (argc == 3) {
        std::string runStr = argv[2];
        if (!fs::is_directory(pathArg)) {
            std::cerr << "[ERROR] Expected directory, got: " << pathArg << "\n";
            return EXIT_FAILURE;
        }

        for (auto& entry : fs::directory_iterator(pathArg)) {
            if (!entry.is_regular_file()) continue;
            std::string fname = entry.path().filename().string();
            if (fname.find("run" + runStr + "_") == 0) filesToCheck.push_back(entry.path());
        }
        if (filesToCheck.empty()) {
            std::cerr << "[ERROR] No subruns found for run " << runStr << "\n";
            return EXIT_FAILURE;
        }
        std::sort(filesToCheck.begin(), filesToCheck.end());
    } else {
        filesToCheck.push_back(pathArg);
    }

    for (const auto& f : filesToCheck) {
        checkDuplicateTriggers(f);
    }

    return EXIT_SUCCESS;
}
