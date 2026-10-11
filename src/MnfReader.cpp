#include "MnfReader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
	uint32_t u32be(const std::vector<uint8_t>& b, size_t p) {
		return (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) |
			(uint32_t(b[p + 2]) << 8) | uint32_t(b[p + 3]);
	}
	double f64be(const std::vector<uint8_t>& b, size_t p) {
		uint64_t bits = 0;
		for (int i = 0; i < 8; ++i) bits = (bits << 8) | b[p + i];
		double value;
		static_assert(sizeof(value) == sizeof(bits), "Unexpected double size");
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}
	bool plausibleDouble(double x) {
		return std::isfinite(x) && std::abs(x) < 1.0e12;
	}
	bool plausibleCoordinate(double x) {
		return std::isfinite(x) && std::abs(x) < 1.0e9;
	}
	struct NodeCandidate { size_t offset = 0, count = 0; };
	struct TopologyCandidate { size_t offset = 0, end = 0, count = 0; };
	struct ModeCandidate { size_t offset = 0, count = 0; };
}

size_t MnfDocument::lineCount() const {
	return static_cast<size_t>(std::count_if(elements.begin(), elements.end(), [](const MnfElement& e) { return e.indices.size() == 2; }));
}
size_t MnfDocument::triangleCount() const {
	return static_cast<size_t>(std::count_if(elements.begin(), elements.end(), [](const MnfElement& e) { return e.indices.size() == 3; }));
}
size_t MnfDocument::quadCount() const {
	return static_cast<size_t>(std::count_if(elements.begin(), elements.end(), [](const MnfElement& e) { return e.indices.size() == 4; }));
}
size_t MnfDocument::polygonCount() const {
	return static_cast<size_t>(std::count_if(elements.begin(), elements.end(), [](const MnfElement& e) { return e.indices.size() > 4; }));
}

MnfReader::MnfReader(const std::string& filename) : m_filename(filename) {}

bool MnfReader::load() {
	std::ifstream in(m_filename, std::ios::binary);
	if (!in) {
		std::cerr << "Cannot open MNF file: " << m_filename << '\n';
		return false;
	}
	m_document = {};
	m_document.fileBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	m_document.fileSize = m_document.fileBytes.size();
	if (m_document.fileBytes.size() < 64) {
		std::cerr << "File is too small to be a valid MNF file.\n";
		return false;
	}
	if (!detectNodes()) {
		std::cerr << "Could not reliably identify a node-coordinate block. The file may use a different MNF variant.\n";
		return false;
	}
	if (!detectTopology()) {
		std::cerr << "Could not reliably identify a connectivity block.\n";
		return false;
	}
	detectModes(); // Optional heuristic; failure does not invalidate geometry.
	return true;
}

const MnfDocument& MnfReader::document() const { return m_document; }

bool MnfReader::detectNodes() {
	const auto& b = m_document.fileBytes;
	constexpr size_t stride = 28; // uint32 ID + three IEEE-754 doubles
	NodeCandidate best;

	// Search every 4-byte-aligned candidate. A node block is accepted only if
	// node IDs form a consecutive run and all coordinates are finite/plausible.
	for (size_t off = 0; off + stride <= b.size(); off += 4) {
		const uint32_t firstId = u32be(b, off);
		if (firstId == 0) continue;
		size_t count = 0;
		uint32_t prevId = firstId;
		size_t p = off;
		while (p + stride <= b.size()) {
			const uint32_t id = u32be(b, p);
			const double x = f64be(b, p + 4), y = f64be(b, p + 12), z = f64be(b, p + 20);
			if (id == 0 || (count > 0 && id <= prevId) ||
				!plausibleCoordinate(x) || !plausibleCoordinate(y) || !plausibleCoordinate(z)) break;
			++count; prevId = id; p += stride;
		}
		if (count > best.count) best = { off, count };
		if (count >= 8) off = ((p - 1) / 4) * 4;
	}
	if (best.count < 8) return false;
	m_document.nodeBlockOffset = best.offset;
	m_document.nodes.reserve(best.count);
	for (size_t i = 0, p = best.offset; i < best.count; ++i, p += stride) {
		MnfNode n;
		n.id = u32be(b, p);
		n.x = f64be(b, p + 4); n.y = f64be(b, p + 12); n.z = f64be(b, p + 20);
		m_document.nodes.push_back(n);
	}
	m_document.detectedBlocks.push_back({ "nodes", best.offset, best.count * stride, best.count });
	return true;
}

bool MnfReader::detectTopology() {
	const auto& b = m_document.fileBytes;
	const size_t nodeCount = m_document.nodes.size();
	TopologyCandidate best;

	// Connectivity records are: uint32 vertexCount, followed by vertexCount
	// uint32 node indices. Accept line/triangle/quad records (2..4 indices)
	// and require every index to be in range. This makes the parser independent
	// of the element counts.
	for (size_t off = 0; off + 12 <= b.size(); off += 4) {
		const uint32_t firstCount = u32be(b, off);
		if (firstCount < 2 || firstCount > 4 || off + 4ull + 4ull * firstCount > b.size()) continue;
		size_t p = off, count = 0;
		while (p + 4 <= b.size()) {
			const uint32_t n = u32be(b, p);
			if (n < 2 || n > 4 || p + 4ull + 4ull * n > b.size()) break;
			bool valid = true;
			for (uint32_t j = 0; j < n; ++j) {
				const uint32_t idx = u32be(b, p + 4 + 4ull * j);
				if (idx >= nodeCount) { valid = false; break; }
			}
			if (!valid) break;
			p += 4 + 4ull * n;
			++count;
		}
		if (count > best.count) best = { off, p, count };
		// Once a substantial run has been decoded, its interior cannot be a
		// longer run than the one starting at its first record. Skip its bytes
		// to avoid quadratic rescanning of large connectivity blocks.
		if (count >= 8) off = ((p - 1) / 4) * 4;
	}
	if (best.count < 8) return false;
	m_document.topologyBlockOffset = best.offset;
	m_document.topologyBlockEnd = best.end;

	size_t p = best.offset;
	m_document.elements.reserve(best.count);
	while (p < best.end) {
		const uint32_t n = u32be(b, p); p += 4;
		MnfElement e;
		e.indices.reserve(n);
		for (uint32_t j = 0; j < n; ++j, p += 4) e.indices.push_back(u32be(b, p));
		m_document.elements.push_back(std::move(e));
	}
	m_document.detectedBlocks.push_back({ "connectivity", best.offset, best.end - best.offset, best.count });
	return true;
}

bool MnfReader::detectModes() {
	const auto& b = m_document.fileBytes;
	constexpr size_t stride = 44; // uint32 mode ID + five doubles
	ModeCandidate best;
	for (size_t off = 0; off + stride <= b.size(); off += 4) {
		const uint32_t first = u32be(b, off);
		if (first == 0 || first > 1000000) continue;
		size_t p = off, count = 0;
		uint32_t prev = first;
		while (p + stride <= b.size()) {
			const uint32_t id = u32be(b, p);
			bool ok = id != 0 && (count == 0 || (prev != std::numeric_limits<uint32_t>::max() && id == prev + 1));
			for (int k = 0; ok && k < 5; ++k) ok = plausibleDouble(f64be(b, p + 4 + 8ull * k));
			if (!ok) break;
			++count; prev = id; p += stride;
		}
		if (count > best.count) best = { off, count };
	}
	if (best.count < 2) return false;
	// Avoid mistaking arbitrary numerical data for modes: a sequential-ID run
	// must begin after the detected node block and before the connectivity block.
	if (best.offset < m_document.nodeBlockOffset + m_document.nodes.size() * 28 ||
		best.offset >= m_document.topologyBlockOffset) return false;
	m_document.modes.reserve(best.count);
	for (size_t i = 0, p = best.offset; i < best.count; ++i, p += stride) {
		MnfMode mode; mode.id = u32be(b, p);
		for (int k = 0; k < 5; ++k) mode.values[k] = f64be(b, p + 4 + 8ull * k);
		m_document.modes.push_back(mode);
	}
	m_document.detectedBlocks.push_back({ "mode-like records (heuristic)", best.offset, best.count * stride, best.count });
	return true;
}

bool MnfReader::exportOBJ(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "# MNF geometry export; source indices are assumed 0-based and converted to OBJ 1-based\n";
	for (const auto& n : m_document.nodes)
		out << std::setprecision(17) << "v " << n.x << ' ' << n.y << ' ' << n.z << '\n';
	for (const auto& e : m_document.elements) {
		if (e.indices.size() == 2) out << "l";
		else out << "f";
		for (uint32_t idx : e.indices) out << ' ' << (static_cast<uint64_t>(idx) + 1);
		out << '\n';
	}
	return true;
}

bool MnfReader::exportNodesCSV(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "Index,NodeID,X,Y,Z\n";
	for (size_t i = 0; i < m_document.nodes.size(); ++i) {
		const auto& n = m_document.nodes[i];
		out << i << ',' << n.id << ',' << std::setprecision(17) << n.x << ',' << n.y << ',' << n.z << '\n';
	}
	return true;
}

bool MnfReader::exportModesCSV(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "ModeID,Value0,Value1,Value2,Value3,Value4\n";
	for (const auto& m : m_document.modes)
		out << m.id << ',' << std::setprecision(17) << m.values[0] << ',' << m.values[1] << ','
		<< m.values[2] << ',' << m.values[3] << ',' << m.values[4] << '\n';
	return true;
}

bool MnfReader::exportDetectedBlocksCSV(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "Kind,Offset,ByteSize,RecordCount\n";
	for (const auto& block : m_document.detectedBlocks)
		out << block.kind << ',' << block.offset << ',' << block.byteSize << ',' << block.recordCount << '\n';
	return true;
}

void MnfReader::printSummary() const {
	std::cout << "========================================================\nMNF structural scan summary\n========================================================\n"
		<< "File size       : " << m_document.fileSize << " bytes\n"
		<< "Node block      : offset " << m_document.nodeBlockOffset << "\n"
		<< "Nodes           : " << m_document.nodes.size() << "\n"
		<< "Connectivity    : offset " << m_document.topologyBlockOffset << " .. " << m_document.topologyBlockEnd << "\n"
		<< "Elements        : " << m_document.elements.size() << "\n"
		<< "Line records    : " << m_document.lineCount() << "\n"
		<< "Triangles       : " << m_document.triangleCount() << "\n"
		<< "Quads           : " << m_document.quadCount() << "\n"
		<< "Other polygons  : " << m_document.polygonCount() << "\n"
		<< "Mode-like rows  : " << m_document.modes.size() << " (heuristic)\n"
		<< "========================================================\n";
}
