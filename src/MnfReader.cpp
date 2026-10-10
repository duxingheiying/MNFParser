#include "MnfReader.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <array>
#include <vector>

namespace {
	bool validDouble(double v)
	{
		return std::isfinite(v) && std::abs(v) < 1.0e12;
	}
}

MnfReader::MnfReader(const std::string& filename)
	: filename_(filename), reader_(filename)
{
}

bool MnfReader::load()
{
	if (!reader_.open()) {
		std::cerr << "Cannot open MNF file: " << filename_ << '\n';
		return false;
	}
	document_ = {};
	document_.fileSize = reader_.size();
	return parseNodes() && parseBodyProperties() && parseModes() && parseTopology();
}

const MnfDocument& MnfReader::document() const
{
	return document_;
}

bool MnfReader::parseNodes()
{
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
	document_.nodes.clear();
	document_.nodes.reserve(nodeCount);

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
		document_.nodes.push_back(n);
	}
	return true;
}

bool MnfReader::parseBodyProperties()
{
	constexpr uint64_t BODY_OFFSET = 340688;
	reader_.seek(BODY_OFFSET);
	for (double& v : document_.body.values) v = reader_.readDouble();
	return true;
}

bool MnfReader::parseModes()
{
	constexpr uint64_t MODE_OFFSET = 340796;
	constexpr uint32_t MODE_COUNT = 30;
	reader_.seek(MODE_OFFSET);
	document_.modeCount = MODE_COUNT;
	document_.modes.reserve(MODE_COUNT);

	for (uint32_t i = 0; i < MODE_COUNT; ++i) {
		MnfMode m;
		m.id = reader_.readU32();
		m.value0 = reader_.readDouble();
		m.value1 = reader_.readDouble();
		m.value2 = reader_.readDouble();
		m.value3 = reader_.readDouble();
		m.value4 = reader_.readDouble();
		document_.modes.push_back(m);
	}
	return true;
}

bool MnfReader::parseTopology() {
	// 支持混合三角/四边形面：读取每条记录的 count 字段（3 或 4），再根据文件中顶点表示（0-based/1-based/ID）逐条归一化为节点数组的 0-based 索引。
	constexpr uint64_t TOPOLOGY_OFFSET = 18091064;
	constexpr uint32_t TRIANGLE_COUNT = 53502;
	reader_.seek(TOPOLOGY_OFFSET);

	struct RawFace {/* uint32_t id;*/ uint32_t count; std::array<uint32_t, 4> v; };
	std::vector<RawFace> raw;
	raw.reserve(TRIANGLE_COUNT);

	for (uint32_t i = 0; i < TRIANGLE_COUNT; ++i) {
		RawFace r;
		r.count = reader_.readU32();

		//r.id = reader_.readU32();
		if (r.count != 3 && r.count != 4) {
			std::cerr << "Unsupported face vertex count (" << r.count << ") at index " << i << '\n';
			return false;
		}
		for (uint32_t j = 0; j < r.count; ++j) {
			r.v[j] = reader_.readU32();
		}
		// 若为三角形，置第4个元素为 0（不会被使用）
		if (r.count == 3) {
			r.v[3] = 0;
		}
		else {
			std::cout << r.count << std::endl;
		}
		raw.push_back(r);
	}

	// 构建 id -> index 映射（0-based）
	std::unordered_map<uint32_t, uint32_t> idToIndex;
	idToIndex.reserve(document_.nodes.size());
	for (uint32_t i = 0; i < document_.nodes.size(); ++i) {
		idToIndex[document_.nodes[i].id] = i;
	}

	const uint32_t n = static_cast<uint32_t>(document_.nodes.size());

	// 将每个 raw 条目归一化为 0-based 索引（逐条尝试三种解析方式，优先保证成功）
	document_.triangles.clear();
	document_.triangles.reserve(raw.size());

	for (size_t i = 0; i < raw.size(); ++i) {
		const auto& r = raw[i];
		MnfTriangle t;
		t.count = r.count;

		bool parsed = false;

		// 尝试 0-based 索引
		bool ok0 = true;
		for (uint32_t j = 0; j < r.count; ++j) {
			if (r.v[j] >= n) { ok0 = false; break; }
		}
		if (ok0) {
			t.v0 = r.v[0];
			t.v1 = r.v[1];
			t.v2 = r.v[2];
			t.v3 = (r.count == 4) ? r.v[3] : 0;
			parsed = true;
		}

		if (!parsed) {
			// 尝试 1-based 索引
			bool ok1 = true;
			for (uint32_t j = 0; j < r.count; ++j) {
				if (r.v[j] < 1 || r.v[j] > n) { ok1 = false; break; }
			}
			if (ok1) {
				t.v0 = r.v[0] - 1;
				t.v1 = r.v[1] - 1;
				t.v2 = r.v[2] - 1;
				t.v3 = (r.count == 4) ? (r.v[3] - 1) : 0;
				parsed = true;
			}
		}

		if (!parsed) {
			// 尝试以节点 ID 映射
			bool okId = true;
			std::array<uint32_t, 4> mapped{};
			for (uint32_t j = 0; j < r.count; ++j) {
				auto it = idToIndex.find(r.v[j]);
				if (it == idToIndex.end()) { okId = false; break; }
				mapped[j] = it->second;
			}
			if (okId) {
				t.v0 = mapped[0];
				t.v1 = mapped[1];
				t.v2 = mapped[2];
				t.v3 = (r.count == 4) ? mapped[3] : 0;
				parsed = true;
			}
		}

		if (!parsed) {
			std::cerr << "Failed to interpret face at topology index " << i << '\n';
			return false;
		}

		// 最后验证索引范围
		if (t.v0 >= n || t.v1 >= n || t.v2 >= n || (t.count == 4 && t.v3 >= n)) {
			std::cerr << "Face index out of range at topology index " << i << '\n';
			return false;
		}

		document_.triangles.push_back(t);
	}

	std::cout << "Topology parsed: " << document_.triangles.size() << " faces (mixed tri/quad supported)\n";
	return true;
}

bool MnfReader::exportOBJ(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "# MNF converted OBJ\n";

	for (const auto& n : document_.nodes)
		out << std::setprecision(17) << "v " << n.x << ' ' << n.y << ' ' << n.z << '\n';

	// document_.triangles 中 v0/v1/v2/(v3) 为 0-based 节点数组索引
	for (const auto& t : document_.triangles) {
		if (t.count == 3) {
			out << "f " << (t.v0 + 1) << ' ' << (t.v1 + 1) << ' ' << (t.v2 + 1) << '\n';
		}
		else {
			out << "f " << (t.v0 + 1) << ' ' << (t.v1 + 1) << ' ' << (t.v2 + 1) << ' ' << (t.v3 + 1) << '\n';
		}
	}
	return true;
}

bool MnfReader::exportNodesCSV(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "Index,NodeID,X,Y,Z\n";
	for (size_t i = 0; i < document_.nodes.size(); ++i) {
		const auto& n = document_.nodes[i];
		out << i << ',' << n.id << ',' << std::setprecision(17)
			<< n.x << ',' << n.y << ',' << n.z << '\n';
	}
	return true;
}

bool MnfReader::exportModesCSV(const std::string& filename) const {
	std::ofstream out(filename);
	if (!out) return false;
	out << "Mode,Value0,Value1,Value2,Value3,Value4\n";
	for (const auto& m : document_.modes) {
		out << m.id << ',' << std::setprecision(17)
			<< m.value0 << ',' << m.value1 << ',' << m.value2 << ','
			<< m.value3 << ',' << m.value4 << '\n';
	}
	return true;
}

void MnfReader::printSummary() const {
	std::cout << "================================================================\n"
		<< "MNF File Summary\n"
		<< "================================================================\n"
		<< "File size : " << document_.fileSize << " bytes\n"
		<< "Nodes     : " << document_.nodes.size() << '\n'
		<< "Modes     : " << document_.modeCount << '\n'
		<< "Faces     : " << document_.triangles.size() << '\n';

	if (!document_.nodes.empty()) {
		const auto& n = document_.nodes.front();
		std::cout << "\nFirst node:"
			<< "  ID = " << n.id
			<< "  X  = " << n.x
			<< "  Y  = " << n.y
			<< "  Z  = " << n.z << '\n';
	}
	std::cout << "================================================================\n";
}
