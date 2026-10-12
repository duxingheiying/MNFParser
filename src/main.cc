#include "mnf_parser.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

	std::string JsonEscape(const std::string& value) {
		std::ostringstream output;
		for (unsigned char character : value) {
			switch (character) {
			case '"': output << "\\\""; break;
			case '\\': output << "\\\\"; break;
			case '\n': output << "\\n"; break;
			case '\r': output << "\\r"; break;
			case '\t': output << "\\t"; break;
			default:
				if (character < 32) {
					output << ' ';
				}
				else {
					output << char(character);
				}
			}
		}
		return output.str();
	}

	bool WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes,
		uint64_t begin, uint64_t end) {
		if (begin > end || end > bytes.size()) return false;
		std::ofstream output(path, std::ios::binary);
		if (!output) return false;
		output.write(reinterpret_cast<const char*>(bytes.data() + begin),
			static_cast<std::streamsize>(end - begin));
		return static_cast<bool>(output);
	}

	bool WriteHex(const fs::path& path, const std::vector<uint8_t>& bytes,
		uint64_t begin, uint64_t end) {
		std::ofstream output(path);
		if (!output) return false;
		for (uint64_t offset = begin; offset < end; offset += 16) {
			output << std::hex << std::setw(8) << std::setfill('0') << offset << "  ";
			for (uint64_t index = offset; index < std::min<uint64_t>(offset + 16, end); ++index) {
				output << std::setw(2) << unsigned(bytes[index]) << ' ';
			}
			output << " |";
			for (uint64_t index = offset; index < std::min<uint64_t>(offset + 16, end); ++index) {
				unsigned char character = bytes[index];
				output << (character >= 32 && character < 127 ? char(character) : '.');
			}
			output << "|\n";
		}
		return true;
	}

	bool HasSection(const std::set<std::string>& sections, const std::string& section) {
		return sections.count("all") != 0 || sections.count(section) != 0;
	}

	double ReadFloat64BigEndianForExport(const std::vector<uint8_t>& bytes,
		size_t position) {
		uint64_t bits = 0;
		for (size_t i = 0; i < sizeof(bits); ++i) {
			bits = (bits << 8) | bytes[position + i];
		}
		double value = 0.0;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

}  // namespace

void PrintHelp() {
	std::cout <<
		"MNF Universal Inspection Tool (C++17)\n"
		"Usage: mnf_tool <input.mnf> [--out DIR] [--sections LIST]\n\n"
		"Sections: all,header,units,nodes,connectivity,mass-inertia,mode-metadata,mode-shapes,matrix-candidates,strings,raw-blocks,obj,report\n"
		"Examples:\n"
		"  mnf_tool part.mnf --out out\n"
		"  mnf_tool part.mnf --out out --sections nodes,connectivity,obj,report\n"
		"  mnf_tool part.mnf --out out --sections header,units,mass-inertia,mode-metadata,mode-shapes,raw-blocks\n\n"
		"Note: undocumented binary regions are preserved as raw bytes; heuristic recognition is not a vendor-certified full semantic decoder.\n";
}
int main(int argc, char** argv) {
	if (argc < 2) { PrintHelp(); return 0; }
	std::string input = argv[1], outDir = "mnf_output"; std::set<std::string> sections = { "all" };
	for (int i = 2; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--help" || a == "-h") { PrintHelp(); return 0; }
		if (a == "--out" && i + 1 < argc) { outDir = argv[++i]; continue; }
		if (a == "--sections" && i + 1 < argc) { sections.clear(); std::stringstream ss(argv[++i]); std::string x; while (std::getline(ss, x, ',')) { if (!x.empty())sections.insert(x); }continue; }
		std::cerr << "Unknown argument: " << a << "\n"; PrintHelp(); return 2;
	}
	fs::create_directories(outDir);
	MnfDocument d; MnfParser parser;
	if (!parser.Load(input, d)) { std::cerr << "Parse failed: " << d.warnings << "\n"; return 1; }
	fs::path out(outDir);
	auto saveBytes = [&](const std::string& name, uint64_t a, uint64_t z) {if (!WriteBytes(out / name, d.bytes, a, z))std::cerr << "Warning: failed to write " << name << "\n"; };
	if (HasSection(sections, "header")) {
		saveBytes("prefix_unclassified.bin", 0, d.node_offset);
		WriteHex(out / "prefix_hex.txt", d.bytes, 0, d.node_offset);
		std::ofstream h(out / "header_info.json");
		h << "{\n  \"input\": \"" << JsonEscape(input)
			<< "\",\n  \"file_size_bytes\": " << d.file_size
			<< ",\n  \"prefix_region_bytes\": " << d.node_offset
			<< ",\n  \"status\": \"raw prefix preserved; semantic header fields are not validated\"\n}\n";
	}
	if (HasSection(sections, "units")) {
		const std::vector<std::string> unit_tokens = {
			"mm", "cm", "in", "ft", "kg", "tonne", "kN", "lbf",
			"MPa", "Pa", "ms", "rad", "deg", "meter", "metre",
			"kilogram", "newton", "second" };
		std::set<std::string> candidates;
		const std::regex token_pattern_cache(
			"(^|[^A-Za-z])(mm|cm|in|ft|kg|tonne|kN|lbf|MPa|Pa|ms|rad|deg|meter|metre|kilogram|newton|second)($|[^A-Za-z])",
			std::regex::icase);
		for (const auto& run : d.text_runs) {
			// Unit labels are most likely in the prefix/header. Do not scan
			// random printable runs throughout large numeric payloads.
			if (run.offset >= d.node_offset) continue;
			std::smatch match;
			if (std::regex_search(run.text, match, token_pattern_cache)) {
				std::string candidate = match.str(2);
				for (const auto& token : unit_tokens) {
					if (candidate.size() == token.size() &&
						std::equal(candidate.begin(), candidate.end(),
							token.begin(), [](char left, char right) {
								return std::tolower(static_cast<unsigned char>(left)) ==
									std::tolower(static_cast<unsigned char>(right));
							})) {
						candidates.insert(token);
					}
				}
			}
		}
		std::ofstream units_file(out / "units.json");
		units_file << "{\n  \"status\": \"candidate_scan_only\",\n"
			<< "  \"recognized_tokens\": [";
		bool first_token = true;
		for (const auto& token : candidates) {
			if (!first_token) units_file << ", ";
			units_file << "\"" << JsonEscape(token) << "\"";
			first_token = false;
		}
		units_file << "],\n  \"warning\": \"Text-token matches are not proof of unit metadata; no unit is asserted unless a producer-specific schema validates it.\"\n}\n";
	}
	if (HasSection(sections, "strings")) {
		std::ofstream strings_file(out / "printable_strings.csv");
		strings_file << "offset,text\n";
		for (const auto& run : d.text_runs) {
			strings_file << run.offset << ",\"";
			for (char character : run.text) {
				if (character == '"') strings_file << "\"\"";
				else strings_file << character;
			}
			strings_file << "\"\n";
		}
	}
	if (HasSection(sections, "matrix-candidates")) {
		std::ofstream matrix_file(out / "matrix_candidates.csv");
		matrix_file << "region,offset,end_offset,byte_size,aligned_double_count,finite_double_count,classification\n";
		for (const auto& range : d.ranges) {
			const uint64_t size = range.end - range.begin;
			const uint64_t count = size / sizeof(double);
			if (count < 4) continue;
			uint64_t finite_count = 0;
			const uint64_t sample_count = std::min<uint64_t>(count, 256);
			for (uint64_t i = 0; i < sample_count; ++i) {
				const double value = ReadFloat64BigEndianForExport(
					d.bytes, static_cast<size_t>(range.begin + i * sizeof(double)));
				if (std::isfinite(value)) ++finite_count;
			}
			const double ratio = sample_count == 0 ? 0.0 :
				static_cast<double>(finite_count) / static_cast<double>(sample_count);
			if (ratio >= 0.95) {
				matrix_file << range.name << ',' << range.begin << ','
					<< range.end << ',' << size << ',' << count << ','
					<< finite_count
					<< ",\"numeric block candidate only; first 256 doubles sampled; dimensions and physical meaning unknown\"\n";
			}
		}
	}

	if (HasSection(sections, "nodes")) {
		std::ofstream f(out / "nodes.csv"); f << "record_index,node_id,x,y,z\n" << std::setprecision(17);
		for (size_t i = 0; i < d.nodes.size(); ++i) { auto& n = d.nodes[i]; f << i << ',' << n.id << ',' << n.x << ',' << n.y << ',' << n.z << '\n'; }
	}
	if (HasSection(sections, "connectivity")) {
		std::ofstream f(out / "elements.csv"); f << "element_index,arity,indices_zero_based\n";
		for (size_t i = 0; i < d.elements.size(); ++i) { f << i << ',' << d.elements[i].indices.size() << ",\""; for (size_t j = 0; j < d.elements[i].indices.size(); ++j) { if (j)f << ' '; f << d.elements[i].indices[j]; }f << "\"\n"; }
	}
	if (HasSection(sections, "mass-inertia") && d.body_properties_found) {
		std::ofstream f(out / "body_properties_candidate.csv"); f << "candidate_field,value\n" << std::setprecision(17);
		f << "mass," << d.body[0] << "\ncenter_of_mass_x," << d.body[1] << "\ncenter_of_mass_y," << d.body[2] << "\ncenter_of_mass_z," << d.body[3] << "\n";
		f << "inertia_xx," << d.body[4] << "\ninertia_xy," << d.body[5] << "\ninertia_xz," << d.body[6] << "\ninertia_yx," << d.body[7] << "\ninertia_yy," << d.body[8] << "\ninertia_yz," << d.body[9] << "\ninertia_zx," << d.body[10] << "\ninertia_zy," << d.body[11] << "\ninertia_zz," << d.body[12] << "\n";
	}
	if (HasSection(sections, "mode-metadata") && d.modes_found) {
		std::ofstream f(out / "mode_metadata.csv"); f << "mode_id,value_1,value_2,value_3,value_4,value_5\n" << std::setprecision(17);
		for (auto& m : d.mode_rows)f << m.id << ',' << m.values[0] << ',' << m.values[1] << ',' << m.values[2] << ',' << m.values[3] << ',' << m.values[4] << '\n';
	}
	if (HasSection(sections, "mode-shapes")) {
		if (d.modes_found && d.topology_found && d.mode_end <= d.topology_offset) {
			saveBytes("modal_region_raw.bin", d.mode_end, d.topology_offset);
			std::ofstream f(out / "modal_region_info.json");
			f << "{\n  \"offset\":" << d.mode_end << ",\n  \"end_offset\":" << d.topology_offset << ",\n  \"byte_size\":" << (d.topology_offset - d.mode_end) << ",\n  \"decoded\":false,\n  \"note\":\"Complete raw bytes between the mode-parameter table and connectivity block are preserved. A verified vendor schema is required to decode per-node/per-mode DOFs and sparse records safely.\"\n}\n";
		}
		else std::cerr << "Warning: mode-shape raw region boundaries could not be inferred.\n";
	}
	if (HasSection(sections, "raw-blocks")) {
		std::ofstream f(out / "opaque_regions.csv"); f << "name,offset,end_offset,byte_size,description\n";
		for (auto& r : d.ranges) { f << r.name << ',' << r.begin << ',' << r.end << ',' << (r.end - r.begin) << ",\"" << r.confidence << "\"\n"; saveBytes(r.name + ".bin", r.begin, r.end); }
		saveBytes("complete_original_copy.mnf", 0, d.file_size);
	}
	if (HasSection(sections, "obj") && d.nodes_found && d.topology_found) {
		std::ofstream f(out / "model.obj"); f << std::setprecision(17);
		for (auto& n : d.nodes)f << "v " << n.x << ' ' << n.y << ' ' << n.z << '\n';
		for (auto& e : d.elements) { if (e.indices.size() == 2)f << "l " << e.indices[0] + 1 << ' ' << e.indices[1] + 1 << '\n'; else if (e.indices.size() >= 3) { f << "f"; for (auto i : e.indices)f << ' ' << i + 1; f << '\n'; } }
	}
	if (HasSection(sections, "report")) {
		std::ofstream f(out / "parse_report.json");
		f << "{\n  \"input\":\"" << JsonEscape(input) << "\",\n  \"file_size_bytes\":" << d.file_size << ",\n  \"byte_order\":\"big_endian (detected pattern)\",\n";
		f << "  \"nodes\":{\"found\":" << (d.nodes_found ? "true" : "false") << ",\"offset\":" << d.node_offset << ",\"count\":" << d.nodes.size() << ",\"end_offset\":" << d.node_end << "},\n";
		f << "  \"connectivity\":{\"found\":" << (d.topology_found ? "true" : "false") << ",\"offset\":" << d.topology_offset << ",\"count\":" << d.elements.size() << ",\"end_offset\":" << d.topology_end << ",\"lines\":" << d.LineCount() << ",\"triangles\":" << d.TriangleCount() << ",\"quads\":" << d.QuadCount() << ",\"polygons_gt4\":" << d.PolygonCount() << "},\n";
		f << "  \"body_mass_inertia\":{\"recognized\":" << (d.body_properties_found ? "true" : "false") << ",\"schema\":\"13 doubles: mass, COM xyz, 3x3 inertia tensor; inferred from two sample files\"},\n";
		f << "  \"mode_parameter_rows\":{\"recognized\":" << (d.modes_found ? "true" : "false") << ",\"offset\":" << d.mode_offset << ",\"count\":" << d.mode_rows.size() << ",\"end_offset\":" << d.mode_end << ",\"schema\":\"ID + five doubles; field meanings not asserted\"},\n";
		f << "  \"limitations\":[\"Header and unit semantics are not fully decoded; see raw prefix and printable-string scan.\",\"Modal-shape region is preserved raw, not semantically decoded.\",\"Generalized mass/stiffness matrices and other undocumented blocks are not interpreted.\",\"Numeric matrix candidates are only heuristic regions, not confirmed matrices.\",\"Connectivity indices are treated as zero-based for OBJ export; validate against producer documentation.\"],\n";
		f << "  \"opaque_regions\":[";
		for (size_t i = 0; i < d.ranges.size(); ++i) { auto& r = d.ranges[i]; if (i)f << ','; f << "{\"name\":\"" << r.name << "\",\"offset\":" << r.begin << ",\"end_offset\":" << r.end << ",\"byte_size\":" << (r.end - r.begin) << "}"; }
		f << "]\n}\n";
	}
	std::cout << "MNF inspection complete\nFile size: " << d.file_size << "\n";
	std::cout << "Nodes: " << d.nodes.size() << " at " << d.node_offset << "\n";
	std::cout << "Connectivity records: " << d.elements.size() << " at " << d.topology_offset << ".." << d.topology_end << "\n";
	std::cout << "  lines=" << d.LineCount() << ", triangles=" << d.TriangleCount() << ", quads=" << d.QuadCount() << ", polygons>4=" << d.PolygonCount() << "\n";
	std::cout << "Body mass/inertia: " << (d.body_properties_found ? "recognized (13 doubles)" : "not recognized") << "\n";
	std::cout << "Mode parameter rows: " << d.mode_rows.size() << " (heuristic; not full mode shapes)\n";
	std::cout << "Output: " << fs::absolute(out).string() << "\n";
	std::cout << "Important: raw preservation is complete when --sections raw-blocks is selected. Matrix candidates and printable strings are forensic hints, not semantic proof.\n";
	return 0;
}
