#ifndef MNF_UNIVERSAL_TOOL_INCLUDE_MNF_TYPES_H_
#define MNF_UNIVERSAL_TOOL_INCLUDE_MNF_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct MnfNode {
	uint32_t id = 0;
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct MnfElement {
	std::vector<uint32_t> indices;
};

struct MnfModeRow {
	uint32_t id = 0;
	double values[5] = {};
};

struct MnfTextRun {
	uint64_t offset = 0;
	std::string text;
};

struct MnfRange {
	std::string name;
	uint64_t begin = 0;
	uint64_t end = 0;
	uint64_t count = 0;
	std::string confidence;
};

struct MnfDocument {
	std::string input_path;
	uint64_t file_size = 0;
	std::vector<uint8_t> bytes;
	uint64_t node_offset = 0;
	uint64_t node_end = 0;
	uint64_t topology_offset = 0;
	uint64_t topology_end = 0;
	uint64_t mode_offset = 0;
	uint64_t mode_end = 0;
	bool nodes_found = false;
	bool topology_found = false;
	bool modes_found = false;
	bool body_properties_found = false;
	std::vector<MnfNode> nodes;
	std::vector<MnfElement> elements;
	std::vector<MnfModeRow> mode_rows;
	double body[13] = {};
	std::vector<MnfRange> ranges;
	std::vector<MnfTextRun> text_runs;
	std::string warnings;

	size_t LineCount() const;
	size_t TriangleCount() const;
	size_t QuadCount() const;
	size_t PolygonCount() const;
};

#endif  // MNF_UNIVERSAL_TOOL_INCLUDE_MNF_TYPES_H_
