#ifndef MNF_UNIVERSAL_TOOL_INCLUDE_MNF_PARSER_H_
#define MNF_UNIVERSAL_TOOL_INCLUDE_MNF_PARSER_H_

#include <string>

#include "mnf_types.h"

// Reads MNF-like binary files using sample-derived structural heuristics.
// This parser does not claim vendor-independent semantic decoding of all fields.
class MnfParser {
public:
	bool Load(const std::string& path, MnfDocument& document);

private:
	bool DetectNodes(MnfDocument& document);
	bool DetectTopology(MnfDocument& document);
	bool DetectBodyAndModes(MnfDocument& document);
	void ScanPrintableStrings(MnfDocument& document);
	void BuildOpaqueRanges(MnfDocument& document);
};

#endif  // MNF_UNIVERSAL_TOOL_INCLUDE_MNF_PARSER_H_
