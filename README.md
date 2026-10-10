# MNFParser

C++17/CMake parser tailored to the supplied 

## Features
- Big-endian binary reader
- Node parsing
- 13 body-property doubles
- 30 modal header records
- 53,502 triangle records
- OBJ export
- Nodes CSV export
- Modes CSV export

## Build

Windows / VS2022:
```powershell
cmake -S . -B build
cmake --build build --config Release
```

Linux:
```bash
cmake -S . -B build
cmake --build build -j
```

## Run
```powershell
build\Release\mnf_parser.exe xxx.mnf output
```

The current offsets/counts are specific to the supplied file and are not
claimed to be a universal MNF specification. Several modal/body fields remain
generic until their physical meanings are validated.
# MNFParser
