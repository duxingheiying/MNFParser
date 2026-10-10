#pragma once
#include <cstdint>
#include <vector>

struct MnfNode {
    uint32_t id = 0;
    double x = 0.0, y = 0.0, z = 0.0;
};

struct MnfTriangle {
	//uint32_t id = 0;
    uint32_t count = 3;
    uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0; // v3 用于四边形（若为三角形则忽略）
};

struct MnfMode {
    uint32_t id = 0;
    double value0 = 0.0, value1 = 0.0, value2 = 0.0;
    double value3 = 0.0, value4 = 0.0;
};

struct MnfBodyProperties {
    double values[13] = {};
};

struct MnfDocument {
    uint64_t fileSize = 0;
    bool bigEndian = true;
    std::vector<MnfNode> nodes;
    MnfBodyProperties body;
    uint32_t modeCount = 0;
    std::vector<MnfMode> modes;
    std::vector<MnfTriangle> triangles;
};
