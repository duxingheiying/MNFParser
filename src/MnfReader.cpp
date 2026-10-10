#include "MnfReader.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {
bool validDouble(double v) { return std::isfinite(v) && std::abs(v) < 1.0e12; }
}

MnfReader::MnfReader(const std::string& filename) : filename_(filename), reader_(filename) {}

bool MnfReader::load() {
    if (!reader_.open()) {
        std::cerr << "Cannot open MNF file: " << filename_ << '\n';
        return false;
    }
    document_ = {};
    document_.fileSize_ = reader_.size();
    return parseNodes() && parseBodyProperties() && parseModes() &&
           parseTopology() && parseEdgeLines();
}

const MnfDocument& MnfReader::document() const { return document_; }

bool MnfReader::parseNodes() {
    // These node-section offsets/counts are specific to the supplied file.
    constexpr uint64_t NODE_OFFSET = 3176;
    constexpr uint32_t NODE_COUNT = 12054;
    reader_.seek(NODE_OFFSET);
    document_.nodes_.reserve(NODE_COUNT);

    for (uint32_t i = 0; i < NODE_COUNT; ++i) {
        MnfNode n;
        n.id = reader_.readU32();
        n.x = reader_.readDouble();
        n.y = reader_.readDouble();
        n.z = reader_.readDouble();
        if (!validDouble(n.x) || !validDouble(n.y) || !validDouble(n.z)) {
            std::cerr << "Invalid node at index " << i << '\n';
            return false;
        }
        document_.nodes_.push_back(n);
    }
    return true;
}

bool MnfReader::parseBodyProperties() {
    constexpr uint64_t BODY_OFFSET = 340688;
    reader_.seek(BODY_OFFSET);
    for (double& v : document_.body_.values) v = reader_.readDouble();
    return true;
}

bool MnfReader::parseModes() {
    constexpr uint64_t MODE_OFFSET = 340796;
    constexpr uint32_t MODE_COUNT = 30;
    reader_.seek(MODE_OFFSET);
    document_.modeCount_ = MODE_COUNT;
    document_.modes_.reserve(MODE_COUNT);

    for (uint32_t i = 0; i < MODE_COUNT; ++i) {
        MnfMode m;
        m.id = reader_.readU32();
        m.value0 = reader_.readDouble();
        m.value1 = reader_.readDouble();
        m.value2 = reader_.readDouble();
        m.value3 = reader_.readDouble();
        m.value4 = reader_.readDouble();
        document_.modes_.push_back(m);
    }
    return true;
}

bool MnfReader::parseTopology() {
    // Mixed connectivity block identified in swing_arm_0.mnf.
    // Each record is: uint32 vertexCount, followed by vertexCount uint32 indices.
    constexpr uint64_t TOPOLOGY_OFFSET = 17923724;
    constexpr uint32_t EXPECTED_LINES = 323;
    constexpr uint32_t EXPECTED_TRIANGLES = 53502;
    constexpr uint32_t EXPECTED_QUADS = 8367;

    reader_.seek(TOPOLOGY_OFFSET);
    document_.lines_.reserve(EXPECTED_LINES);
    document_.triangles_.reserve(EXPECTED_TRIANGLES);
    document_.quads_.reserve(EXPECTED_QUADS);

    for (uint32_t i = 0;
         i < EXPECTED_LINES + EXPECTED_TRIANGLES + EXPECTED_QUADS;
         ++i) {
        const uint32_t count = reader_.readU32();
        if (count == 2) {
            MnfLine e;
            e.count = count;
            e.v0 = reader_.readU32();
            e.v1 = reader_.readU32();
            document_.lines_.push_back(e);
        } else if (count == 3) {
            MnfTriangle t;
            t.count = count;
            t.v0 = reader_.readU32();
            t.v1 = reader_.readU32();
            t.v2 = reader_.readU32();
            document_.triangles_.push_back(t);
        } else if (count == 4) {
            MnfQuad q;
            q.count = count;
            q.v0 = reader_.readU32();
            q.v1 = reader_.readU32();
            q.v2 = reader_.readU32();
            q.v3 = reader_.readU32();
            document_.quads_.push_back(q);
        } else {
            std::cerr << "Unexpected connectivity record size " << count
                      << " at record " << i << '\n';
            return false;
        }
    }

    if (document_.lines_.size() != EXPECTED_LINES ||
        document_.triangles_.size() != EXPECTED_TRIANGLES ||
        document_.quads_.size() != EXPECTED_QUADS) {
        std::cerr << "Connectivity count mismatch: lines="
                  << document_.lines_.size() << ", triangles="
                  << document_.triangles_.size() << ", quads="
                  << document_.quads_.size() << '\n';
        return false;
    }
    return true;
}

bool MnfReader::parseEdgeLines() {
    constexpr uint64_t NODE_OFFSET = 2588;
    constexpr uint64_t BODY_OFFSET = 340688; // 与 parseBodyProperties 中的偏移保持一致
    constexpr uint32_t NODE_RECORD_SIZE = 4 + 8 * 3; // id (u32) + x,y,z (double x3)

    if (BODY_OFFSET <= NODE_OFFSET) {
        std::cerr << "Invalid offsets for node region\n";
        return false;
    }

    uint64_t blockSize = BODY_OFFSET - NODE_OFFSET;
    if (blockSize % NODE_RECORD_SIZE != 0) {
        std::cerr << "Warning: node block size is not an exact multiple of node record size\n";
    }

    uint32_t nodeCount = static_cast<uint32_t>(blockSize / NODE_RECORD_SIZE);

    reader_.seek(NODE_OFFSET);
    document_.nodes_.clear();
    document_.nodes_.reserve(nodeCount);

    for (uint32_t i = 0; i < nodeCount; ++i) {
        MnfNode n;
        n.id = reader_.readU32();
        n.x = reader_.readDouble();
        n.y = reader_.readDouble();
        n.z = reader_.readDouble();
        if (!validDouble(n.x) || !validDouble(n.y) || !validDouble(n.z)) {
            std::cerr << "Invalid node at index " << i << '\n';
            return false;
        }
        //else {
        //	std::cout<<std::setw(16)<<n.id<<std::setw(16)<<n.x<<std::setw(16)<<n.y<<std::setw(16)<<n.z<<'\n';
        //}
        document_.nodes_.push_back(n);
    }
    return true;
}

bool MnfReader::exportOBJ(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return false;

    out << "# MNF converted OBJ\n";
    out << "# Nodes: " << document_.nodes_.size() << "\n";
    out << "# Lines: " << document_.lines_.size() << "\n";
    out << "# Triangles: " << document_.triangles_.size() << "\n";
    out << "# Quads: " << document_.quads_.size() << "\n\n";

    for (const auto& n : document_.nodes_)
        out << std::setprecision(17) << "v " << n.x << ' ' << n.y << ' ' << n.z << '\n';

    // OBJ face indices are 1-based. The source connectivity indices are written
    // as found; confirm index convention against the producer if rendering looks shifted.
    out << '\n';
    for (const auto& e : document_.lines_)
        out << "l " << e.v0 << ' ' << e.v1 << '\n';
    for (const auto& t : document_.triangles_)
        out << "f " << t.v0 << ' ' << t.v1 << ' ' << t.v2 << '\n';
    for (const auto& q : document_.quads_)
        out << "f " << q.v0 << ' ' << q.v1 << ' ' << q.v2 << ' ' << q.v3 << '\n';
    return true;
}

bool MnfReader::exportNodesCSV(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return false;
    out << "Index,NodeID,X,Y,Z\n";
    for (size_t i = 0; i < document_.nodes_.size(); ++i) {
        const auto& n = document_.nodes_[i];
        out << i << ',' << n.id << ',' << std::setprecision(17)
            << n.x << ',' << n.y << ',' << n.z << '\n';
    }
    return true;
}

bool MnfReader::exportModesCSV(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return false;
    out << "Mode,Value0,Value1,Value2,Value3,Value4\n";
    for (const auto& m : document_.modes_)
        out << m.id << ',' << std::setprecision(17)
            << m.value0 << ',' << m.value1 << ',' << m.value2 << ','
            << m.value3 << ',' << m.value4 << '\n';
    return true;
}

void MnfReader::printSummary() const {
    std::cout << "============================\n"
              << "MNF File Summary\n"
              << "============================\n"
              << "File size      : " << document_.fileSize_ << " bytes\n"
              << "Nodes          : " << document_.nodes_.size() << '\n'
              << "Mixed-block lines     : " << document_.lines_.size() << '\n'
              << "Triangles      : " << document_.triangles_.size() << '\n'
              << "Quads          : " << document_.quads_.size() << '\n'
              << "Separate edge lines : " << document_.edgeLines_.size() << '\n'
              << "Modes          : " << document_.modeCount_ << '\n'
              << "============================\n";
}
