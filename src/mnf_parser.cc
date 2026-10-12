#include "mnf_parser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

namespace {

	uint32_t ReadUint32BigEndian(const std::vector<uint8_t>& bytes,
		size_t position) {
		return (uint32_t(bytes[position]) << 24) |
			(uint32_t(bytes[position + 1]) << 16) |
			(uint32_t(bytes[position + 2]) << 8) |
			uint32_t(bytes[position + 3]);
	}

	double ReadFloat64BigEndian(const std::vector<uint8_t>& bytes,
		size_t position) {
		uint64_t bits = 0;
		for (int i = 0; i < 8; ++i) {
			bits = (bits << 8) | bytes[position + i];
		}

		double value;
		static_assert(sizeof(value) == sizeof(bits));
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	bool IsPlausibleNumber(double value) {
		return std::isfinite(value) && std::abs(value) < 1.0e15;
	}

}  // namespace

size_t MnfDocument::LineCount() const {
	size_t count = 0;
	for (const auto& element : elements) {
		if (element.indices.size() == 2) {
			++count;
		}
	}
	return count;
}

size_t MnfDocument::TriangleCount() const {
	size_t count = 0;
	for (const auto& element : elements) {
		if (element.indices.size() == 3) {
			++count;
		}
	}
	return count;
}

size_t MnfDocument::QuadCount() const {
	size_t count = 0;
	for (const auto& element : elements) {
		if (element.indices.size() == 4) {
			++count;
		}
	}
	return count;
}

size_t MnfDocument::PolygonCount() const {
	size_t count = 0;
	for (const auto& element : elements) {
		if (element.indices.size() > 4) {
			++count;
		}
	}
	return count;
}

bool MnfParser::Load(const std::string& path, MnfDocument& document) {
	document = {};
	document.input_path = path;

	std::ifstream input(path, std::ios::binary);
	if (!input) {
		document.warnings = "Cannot open input file";
		return false;
	}

	document.bytes.assign(std::istreambuf_iterator<char>(input), {});
	document.file_size = document.bytes.size();
	if (document.file_size < 128) {
		document.warnings = "Input is too small";
		return false;
	}

	if (!DetectNodes(document)) {
		document.warnings =
			"No reliable node-coordinate block found (expected BE uint32 ID + "
			"3 BE float64 values)";
		return false;
	}

	DetectTopology(document);
	DetectBodyAndModes(document);

	// Preserve every byte range, including regions not semantically decoded.
	std::vector<std::pair<uint64_t, uint64_t>> used_ranges;
	used_ranges.push_back({ document.node_offset, document.node_end });
	if (document.topology_found) {
		used_ranges.push_back(
			{ document.topology_offset, document.topology_end });
	}
	if (document.body_properties_found) {
		used_ranges.push_back({ document.node_end, document.node_end + 104 });
	}
	if (document.modes_found) {
		used_ranges.push_back({ document.mode_offset, document.mode_end });
	}

	std::sort(used_ranges.begin(), used_ranges.end());
	uint64_t current_offset = 0;
	size_t opaque_index = 0;
	for (const auto& range : used_ranges) {
		if (range.first > current_offset) {
			document.ranges.push_back(
				{ "opaque_" + std::to_string(opaque_index++), current_offset,
				 range.first, 0, "unclassified raw bytes" });
		}
		if (range.second > current_offset) {
			current_offset = range.second;
		}
	}

	if (current_offset < document.file_size) {
		document.ranges.push_back(
			{ "opaque_" + std::to_string(opaque_index), current_offset,
			 document.file_size, 0, "unclassified raw bytes" });
	}
	return true;
}

bool MnfParser::DetectNodes(MnfDocument& document) {
	const auto& bytes = document.bytes;
	constexpr size_t kNodeRecordSize = 28;
	size_t best_offset = 0;
	size_t best_count = 0;

	for (size_t offset = 0; offset + kNodeRecordSize <= bytes.size();
		offset += 4) {
		const uint32_t first_id = ReadUint32BigEndian(bytes, offset);
		if (first_id == 0) {
			continue;
		}

		size_t position = offset;
		size_t count = 0;
		uint32_t previous_id = 0;
		while (position + kNodeRecordSize <= bytes.size()) {
			const uint32_t node_id = ReadUint32BigEndian(bytes, position);
			const double x = ReadFloat64BigEndian(bytes, position + 4);
			const double y = ReadFloat64BigEndian(bytes, position + 12);
			const double z = ReadFloat64BigEndian(bytes, position + 20);
			if (node_id == 0 || (count > 0 && node_id <= previous_id) ||
				!IsPlausibleNumber(x) || !IsPlausibleNumber(y) ||
				!IsPlausibleNumber(z)) {
				break;
			}
			++count;
			previous_id = node_id;
			position += kNodeRecordSize;
		}

		if (count > best_count) {
			best_count = count;
			best_offset = offset;
		}
		if (count >= 8) {
			offset = ((position - 1) / 4) * 4;
		}
	}

	if (best_count < 8) {
		return false;
	}

	document.node_offset = best_offset;
	document.node_end = best_offset + best_count * kNodeRecordSize;
	document.nodes_found = true;
	document.nodes.reserve(best_count);
	for (size_t index = 0, position = best_offset; index < best_count;
		++index, position += kNodeRecordSize) {
		document.nodes.push_back(
			{ ReadUint32BigEndian(bytes, position),
			 ReadFloat64BigEndian(bytes, position + 4),
			 ReadFloat64BigEndian(bytes, position + 12),
			 ReadFloat64BigEndian(bytes, position + 20) });
	}
	return true;
}

bool MnfParser::DetectTopology(MnfDocument& document) {
	const auto& bytes = document.bytes;
	const uint64_t node_count = document.nodes.size();
	size_t best_offset = 0;
	size_t best_end = 0;
	size_t best_count = 0;

	// Candidate records use: big-endian uint32 arity + node indices.
	// This heuristic recognizes arities 2, 3, and 4 in the supplied samples.
	for (size_t offset = 0; offset + 12 <= bytes.size(); offset += 4) {
		const uint32_t arity = ReadUint32BigEndian(bytes, offset);
		if (arity < 2 || arity > 4 ||
			offset + 4ull + 4ull * arity > bytes.size()) {
			continue;
		}

		size_t position = offset;
		size_t count = 0;
		while (position + 4 <= bytes.size()) {
			const uint32_t record_arity =
				ReadUint32BigEndian(bytes, position);
			if (record_arity < 2 || record_arity > 4 ||
				position + 4ull + 4ull * record_arity > bytes.size()) {
				break;
			}

			bool valid_record = true;
			for (uint32_t index = 0; index < record_arity; ++index) {
				if (ReadUint32BigEndian(bytes, position + 4 + 4ull * index) >=
					node_count) {
					valid_record = false;
					break;
				}
			}
			if (!valid_record) {
				break;
			}

			position += 4 + 4 * record_arity;
			++count;
		}

		if (count > best_count) {
			best_count = count;
			best_offset = offset;
			best_end = position;
		}
		if (count >= 8) {
			offset = ((position - 1) / 4) * 4;
		}
	}

	if (best_count < 8) {
		return false;
	}

	document.topology_offset = best_offset;
	document.topology_end = best_end;
	document.topology_found = true;
	for (size_t position = best_offset; position < best_end;) {
		const uint32_t arity = ReadUint32BigEndian(bytes, position);
		position += 4;
		MnfElement element;
		element.indices.reserve(arity);
		for (uint32_t index = 0; index < arity; ++index, position += 4) {
			element.indices.push_back(ReadUint32BigEndian(bytes, position));
		}
		document.elements.push_back(std::move(element));
	}
	return true;
}

bool MnfParser::DetectBodyAndModes(MnfDocument& document) {
	const auto& bytes = document.bytes;
	constexpr uint64_t kBodyPropertySize = 13 * sizeof(double);

	// The 13-double layout is inferred from the two supplied sample files.
	if (document.node_end + kBodyPropertySize <= bytes.size()) {
		bool valid_record = true;
		double values[13] = {};
		for (int index = 0; index < 13; ++index) {
			values[index] = ReadFloat64BigEndian(
				bytes, document.node_end + 8ull * index);
			valid_record = valid_record && IsPlausibleNumber(values[index]);
		}

		const double tolerance =
			1.0e-6 * std::max(1.0, std::abs(values[5]));
		const double tolerance_xz =
			1.0e-6 * std::max(1.0, std::abs(values[6]));
		const double tolerance_yz =
			1.0e-6 * std::max(1.0, std::abs(values[9]));
		if (valid_record && std::abs(values[5] - values[7]) < tolerance &&
			std::abs(values[6] - values[10]) < tolerance_xz &&
			std::abs(values[9] - values[11]) < tolerance_yz) {
			std::copy(values, values + 13, document.body);
			document.body_properties_found = true;
		}
	}

	if (!document.body_properties_found) {
		return false;
	}

	const uint64_t start = document.node_end + kBodyPropertySize;
	const uint64_t end_limit = document.topology_found
		? document.topology_offset
		: document.file_size;
	constexpr uint64_t kModeRecordSize = 44;
	size_t best_offset = 0;
	size_t best_count = 0;

	for (uint64_t offset = start;
		offset + kModeRecordSize <= end_limit; offset += 4) {
		const uint32_t first_id = ReadUint32BigEndian(bytes, offset);
		if (first_id == 0 || first_id > 1000000) {
			continue;
		}

		uint32_t previous_id = first_id;
		size_t count = 0;
		uint64_t position = offset;
		while (position + kModeRecordSize <= end_limit) {
			const uint32_t mode_id = ReadUint32BigEndian(bytes, position);
			bool valid_record = mode_id != 0 &&
				(count == 0 || mode_id == previous_id + 1);
			for (int index = 0; valid_record && index < 5; ++index) {
				valid_record = IsPlausibleNumber(
					ReadFloat64BigEndian(bytes, position + 4 + 8ull * index));
			}
			if (!valid_record) {
				break;
			}

			++count;
			previous_id = mode_id;
			position += kModeRecordSize;
		}

		if (count > best_count) {
			best_count = count;
			best_offset = static_cast<size_t>(offset);
		}
	}

	if (best_count < 2) {
		return false;
	}

	document.mode_offset = best_offset;
	document.mode_end = best_offset + best_count * kModeRecordSize;
	document.modes_found = true;
	for (size_t index = 0, position = best_offset; index < best_count;
		++index, position += kModeRecordSize) {
		MnfModeRow row;
		row.id = ReadUint32BigEndian(bytes, position);
		for (int value_index = 0; value_index < 5; ++value_index) {
			row.values[value_index] =
				ReadFloat64BigEndian(bytes, position + 4 + 8ull * value_index);
		}
		document.mode_rows.push_back(row);
	}
	return true;
}
