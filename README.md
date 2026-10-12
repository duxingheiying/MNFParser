# MNF Universal Inspection Tool (C++17, experimental)

This project separates binary parsing into the `MnfParser` class. It scans a file without hard-coded node, element, or mode counts and can preserve the original file and all unclassified byte ranges.

## Important format limitation

MNF is a binary modal-neutral format. Public documentation describes semantic contents such as header/date/version/units, geometry/connectivity, nodal mass and inertia, mode shapes, generalized mass and stiffness matrices, interface normal modes and constraint modes. Public product documentation does not provide one complete byte-level schema covering every MNF producer and version. A complete semantic decoder therefore requires a verified producer/version profile. This tool intentionally labels sample-derived interpretations as candidates instead of inventing meanings for unknown numbers.

References:
- Hexagon, *Adams Flex User Guide*: https://documentation-be.hexagon.com/bundle/Adams_2024.1_Adams_Flex_User_Guide/raw/resource/enus/Adams_2024.1_Adams_Flex_User_Guide.pdf
- Ansys, *Methodology Behind the Ansys-Adams Interface*: https://ansyshelp.ansys.com/public/Views/Secured/corp/v251/en/ans_substr/advmeth2402.html

## Implemented

- Reads the entire file into memory; if the current sample profile cannot identify nodes, it still completes a raw inspection instead of failing.
- Sample-derived recognition for big-endian `uint32 node_id + 3 x float64` node records.
- Sample-derived recognition for big-endian connectivity records: arity followed by indices.
- Candidate 13-double body-property record and sequential-ID/five-double parameter table.
- Printable-string extraction with byte offsets.
- Heuristic numeric-block candidate listing; it does not claim these are matrices.
- Raw preservation of every unclassified byte region and a byte-identical complete file copy.
- Export selection by `--sections`.

## Not yet semantically decoded

- Full header field meanings and unit enumerations.
- Nodal mass/inertia records for all format variants.
- Full per-node/per-mode modal DOF vectors, including DOF ordering and sparse/dense encodings.
- Generalized mass/stiffness matrices and vendor-specific extra blocks.

These require a verified byte-level format profile. The raw bytes are preserved so no data is discarded while that profile is developed.

## Build (Windows + VS2022)

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Usage

```powershell
# Full inspection and raw preservation
build\Release\mnf_tool.exe input.mnf --out output --sections all

# Selected exports
build\Release\mnf_tool.exe input.mnf --out output --sections header,units,nodes,connectivity,mass-inertia,mode-metadata,mode-shapes,matrix-candidates,strings,raw-blocks,obj,report

# Only geometry
build\Release\mnf_tool.exe input.mnf --out output --sections nodes,connectivity,obj,report
```

Available sections: `all`, `header`, `units`, `nodes`, `connectivity`, `mass-inertia`, `mode-metadata`, `mode-shapes`, `matrix-candidates`, `strings`, `raw-blocks`, `obj`, `report`.

## Output notes

- `header_info.json`, `prefix_hex.txt`, `prefix_unclassified.bin`: raw prefix and basic location metadata, not a claim that the prefix is the complete header.
- `units.json`: candidate text-token scan only; not authoritative unit decoding.
- `nodes.csv`, `elements.csv`, `model.obj`: geometry recognized by the current sample-derived profile.
- `body_properties_candidate.csv`: sample-derived candidate mapping of 13 doubles, not vendor-validated.
- `mode_metadata.csv`: ID plus five-double rows; value meanings are not asserted.
- `matrix_candidates.csv`: regions with many finite big-endian float64 interpretations; dimensions and physical meanings unknown.
- `printable_strings.csv`: printable ASCII runs and offsets.
- `opaque_regions.csv`, `opaque_*.bin`, `complete_original_copy.mnf`: raw preservation of unknown ranges and the full input.
- `modal_region_raw.bin`: a candidate region between recognized mode-parameter rows and connectivity, not guaranteed to contain only modal shapes.

The `.clang-format` file is based on Google C++ Style. Apply it with `clang-format -i include/*.h src/*.cc`.
