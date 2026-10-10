#pragma once
#include "MnfTypes.h"
#include "BigEndianReader.h"
#include <string>

class MnfReader {
public:
    explicit MnfReader(const std::string& filename);
    bool load();
    const MnfDocument& document() const;
    bool exportOBJ(const std::string& filename) const;
    bool exportNodesCSV(const std::string& filename) const;
    bool exportModesCSV(const std::string& filename) const;
    void printSummary() const;

private:
    bool parseNodes();
    bool parseBodyProperties();
    bool parseModes();
    bool parseTopology();

    std::string filename_;
    BigEndianReader reader_;
    MnfDocument document_;
};
