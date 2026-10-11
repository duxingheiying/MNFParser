#include "MnfReader.h"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    //if (argc < 2) {
    //    std::cout << "Usage: mnf_parser <input.mnf> [output_dir]\n";
    //    return 0;
    //}
    //const std::string input = argv[1];
    const std::string input = argc >= 2 ? argv[1]: "E:/model_data/mnf/swing_arm_0.mnf";
    const std::string output = argc >= 3 ? argv[2] : ".";
    std::error_code ec;
    std::filesystem::create_directories(output, ec);
    MnfReader reader(input);
    if (!reader.load()) {
        std::cerr << "Failed to parse MNF. This reader currently auto-detects big-endian files with\n"
                     "node records (ID + XYZ doubles) and variable-length connectivity records.\n";
        return 1;
    }
    reader.printSummary();
    if (!reader.exportOBJ(output + "/model.obj")) std::cerr << "Warning: failed to export model.obj\n";
    if (!reader.exportNodesCSV(output + "/nodes.csv")) std::cerr << "Warning: failed to export nodes.csv\n";
    if (!reader.exportModesCSV(output + "/modes.csv")) std::cerr << "Warning: failed to export modes.csv\n";
    if (!reader.exportDetectedBlocksCSV(output + "/detected_blocks.csv")) std::cerr << "Warning: failed to export detected_blocks.csv\n";
    std::cout << "Export completed.\n";
    return 0;
}
