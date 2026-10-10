#pragma once
#include <cstdint>
#include <vector>

struct MnfNode {
    uint32_t id = 0;
    double x = 0.0, y = 0.0, z = 0.0;
};

struct MnfLine {
    uint32_t count = 2;
    uint32_t v0 = 0, v1 = 0;
};

struct MnfTriangle {
    uint32_t count = 3;
    uint32_t v0 = 0, v1 = 0, v2 = 0;
};

struct MnfQuad {
    uint32_t count = 4;
    uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
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
    uint64_t fileSize_ = 0;
    bool bigEndian_ = true;
    std::vector<MnfNode> nodes_;
    MnfBodyProperties body_;
    uint32_t modeCount_ = 0;
    std::vector<MnfMode> modes_;

    // Mixed element-connectivity block.
    std::vector<MnfLine> lines_;
    std::vector<MnfTriangle> triangles_;
    std::vector<MnfQuad> quads_;

    // A second line/edge block found near the end of the supplied file.
    std::vector<MnfLine> edgeLines_;
};
