#pragma once
#include "MnfTypes.h"
#include <string>

class MnfReader {
public:
	explicit MnfReader(const std::string& filename);
	bool load();
	const MnfDocument& document() const;
	bool exportOBJ(const std::string& filename) const;
	bool exportNodesCSV(const std::string& filename) const;
	bool exportModesCSV(const std::string& filename) const;
	bool exportDetectedBlocksCSV(const std::string& filename) const;
	void printSummary() const;

private:
	bool detectNodes();
	bool detectTopology();
	bool detectModes();
	std::string m_filename;
	MnfDocument m_document;
};
