#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct MnfNode {
	uint32_t id = 0;
	double x = 0.0, y = 0.0, z = 0.0;
};

// Generic connectivity record. vertexCount determines whether it is a line,
// triangle, quad, or another polygon type.
struct MnfElement {
	std::vector<uint32_t> indices;
};

struct MnfMode {
	uint32_t id = 0;
	double values[5] = {};
};

struct MnfDataBlock {
	std::string kind;
	uint64_t offset = 0;
	uint64_t byteSize = 0;
	uint64_t recordCount = 0;
};

struct MnfDocument {
	uint64_t fileSize = 0;
	bool bigEndian = true;
	uint64_t nodeBlockOffset = 0;
	uint64_t topologyBlockOffset = 0;
	uint64_t topologyBlockEnd = 0;
	std::vector<MnfNode> nodes;
	std::vector<MnfElement> elements;
	std::vector<MnfMode> modes;
	std::vector<MnfDataBlock> detectedBlocks;
	std::vector<uint8_t> fileBytes;

	size_t lineCount() const;
	size_t triangleCount() const;
	size_t quadCount() const;
	size_t polygonCount() const;
};
