# MNF Universal Inspection Tool (experimental)

A C++17/CMake command-line tool for inspecting binary MNF files without hard-coded node/element counts or fixed topology offsets. It was tested against the two supplied files:

- `xxx.mnf`
- `xxx_xxx_.mnf`

## Important truth about “complete MNF parsing”

MNF is a binary modal-neutral format. Public vendor documentation describes its semantic content (geometry, nodal mass/inertia, mode shapes, generalized mass/stiffness, units and header metadata), but does not provide a complete public byte-level schema covering every producer/version. Therefore this tool separates:

1. **Decoded/inferred records**: node records, 2/3/4-index connectivity, a 13-double body-property record inferred from both supplied samples, and a sequential-ID table of five-double mode parameters.
2. **Opaque regions**: all unrecognized bytes can be exported unchanged. The modal-shape region is preserved byte-for-byte, but is not falsely claimed to be semantically decoded.
3. **Unresolved fields**: unit identifiers, complete header semantics, nodal mass matrices, full per-node/per-mode DOFs, generalized mass/stiffness matrices and vendor-specific data need a verified producer/version schema.

This is deliberately safer than labeling arbitrary binary values as physical quantities. Do not use inferred fields for engineering calculations until they are cross-checked against the producing software or its official MNF documentation.


## Source layout and coding style

The parser is now separated from the command-line/export layer:

- `include/mnf_types.h`: parsed-document and record data structures.
- `include/mnf_parser.h`: public `MnfParser` class interface.
- `src/mnf_parser.cc`: binary reading and structural detection implementation.
- `src/main.cc`: command-line options and output/export code only.
- `.clang-format`: `BasedOnStyle: Google`; use `clang-format -i include/*.h src/*.cc`.

The parser API is `MnfParser::Load(path, document)`. A caller can use this class
directly without depending on the command-line executable. The code follows
Google C++ style conventions for naming, indentation, include guards, and
formatting configuration.

## Build (Windows + VS2022)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

```powershell
# Export all supported outputs, including original-file copy and opaque raw regions
build\Release\mnf_tool.exe input.mnf --out output

# Select only the requested categories
build\Release\mnf_tool.exe input.mnf --out output --sections nodes,connectivity,obj,report

# Focus on physical metadata and preserve unparsed regions
build\Release\mnf_tool.exe input.mnf --out output --sections header,units,mass-inertia,mode-metadata,mode-shapes,raw-blocks,report
```

Sections:
- `header`: raw prefix bytes + hex dump + minimal metadata (not a semantic header decoder)
- `units`: explicit unresolved status; no guessed unit values
- `nodes`: `nodes.csv`
- `connectivity`: `elements.csv`
- `mass-inertia`: `mass_inertia.csv` when the 13-double record passes validation
- `mode-metadata`: `mode_metadata.csv` for the detected ID + five-double rows
- `mode-shapes`: `modal_region_raw.bin` and boundary metadata; raw, not semantically decoded
- `raw-blocks`: exports every opaque region plus `complete_original_copy.mnf`
- `obj`: `model.obj`
- `report`: `parse_report.json`

## Output files

`parse_report.json` records offsets, counts, recognition status and limitations. `opaque_regions.csv` gives raw-region byte ranges. All offsets are zero-based and half-open `[offset, end_offset)`.

## Current inferred layout in the supplied pair

Both samples have a contiguous node block encoded as big-endian `uint32 node_id + 3 x IEEE-754 float64` (28 bytes/record). Immediately after it, both samples contain 13 big-endian doubles that look like mass, center of mass XYZ, and a symmetric 3x3 inertia tensor. A sequential ID + five-double table appears after that. A large following region is consistent with modal data but is not yet decoded.

These are sample-derived format observations, not a universal guarantee for all MNF variants. The scanner validates ranges and reports uncertainty instead of assuming every producer uses the same layout.
