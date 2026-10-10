# MNFParser (updated topology support)

C++17 / CMake sample parser tailored to the supplied `swing_arm_0.mnf`.

## Topology additions
The parser now reads mixed connectivity records (`vertexCount` followed by indices):
- 323 two-index records in the mixed connectivity block
- 53,502 triangle records
- 8,367 quadrilateral records
- A separate block of 1,015 two-index edge/line records

The code retains the two-index records from the mixed block as `MnfDocument::lines_`
and the separate line/edge block as `MnfDocument::edgeLines_`. OBJ export writes
mixed-block lines_ using `l`, triangles_ using `f`, and quads_ using `f`.

## Build (VS2022)
```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run
```powershell
build\\Release\\mnf_parser.exe swing_arm_0.mnf output
```

Output files: `model.obj`, `nodes_.csv`, `modes_.csv`.

## Scope note
Offsets/counts are tailored to the supplied file and should not be treated as
a universal MNF format specification. Verify the connectivity index convention
and mapping to parsed node records against your MNF producer before relying on
geometry for engineering calculations.
