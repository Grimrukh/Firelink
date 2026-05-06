# Firelink — Agent Reference

## Architecture Overview

Firelink is a **Windows-only C++20** project for hooking into FromSoftware games
and reading/writing their binary file formats. It produces:

- Native DLLs/static libs consumed by external C++ projects
- `pyrelink` — Python extension modules via pybind11 (built with scikit-build-core)

### Library Hierarchy (`lib/`)

| Library | Purpose |
|---|---|
| `FirelinkCore` | Foundation: `ManagedProcess`, `BaseHook`, `BinaryReadWrite`, `GameFile`, `DCX`, `TPF`, `Binder`, `Havok` |
| `FirelinkFLVER` | FLVER 3D model format (read/write) |
| `FirelinkDSRHook` | Dark Souls: Remastered memory hook (`DSRHook : BaseHook`) |
| `FirelinkERHook` | Elden Ring memory hook (`ERHook : BaseHook`) |
| `FirelinkERMaps` | Elden Ring MSB map format (read/write); ~100 ms for Stormveil Castle |
| `FirelinkERHavok` | Havok navmesh (WIP — commented out in `lib/CMakeLists.txt`) |

Each library lives in `lib/<Name>/` with `include/<Name>/` headers and `src/` sources.
Pybind11 modules for Core, FLVER, and ERMaps live under `python/pybind/<Name>/`.

## Key Design Patterns

### `GameFile<T>` CRTP Base (`include/FirelinkCore/GameFile.h`)
All file formats inherit `GameFile<T>`. Subclasses must implement:
```cpp
void Deserialize(BinaryReadWrite::BufferReader& reader);
void Serialize(BinaryReadWrite::BufferWriter& writer) const;
static BinaryReadWrite::Endian GetEndian() noexcept;
```
Use `T::FromPath(path)`, `T::FromBytes(data, size)`, or `T::FromPathsParallel(paths)` as
factory methods. `ToBytes()` / `WriteToPath()` handle DCX compression automatically based on
`m_dcxType`.

### `BufferReader` / `BufferWriter` (`include/FirelinkCore/BinaryReadWrite.h`)
All binary I/O goes through these in-memory helpers (not `std::ifstream` directly for new code).
Key idioms:
- `reader.TempOffset(offset)` — RAII temporary seek guard.
- `writer.Reserve<T>("label")` / `writer.Fill<T>("label", value)` — deferred offset fill.
- `writer.Finalize()` throws if any reservations are unfilled.

### MSB Entry Model (`lib/FirelinkERMaps/`)
`MSB` owns `ModelParam`, `EventParam`, `RegionParam`, `RouteParam`, `PartParam` — each is a
typed `EntryParam<T>` that stores `unique_ptr<T>` grouped by subtype enum. Cross-entry
references use `EntryReference<T>` (non-owning, RAII-managed); when a referenced `Entry` is
destroyed it automatically nulls all referrers via `m_incomingReferences`.

### Export Macros
Each library generates its own export header via `generate_firelink_export_header()` in
`CMake/FirelinkModules.cmake`. Every public symbol must be tagged with the library's API macro
(e.g. `FIRELINK_CORE_API`, `FIRELINK_ER_MAPS_API`). The `Export.h` is auto-generated into
`build/<type>/<Library>/include/<Library>/Export.h` — do not hand-edit it.

### Property Macros (`GameFile.h`)
Use `GAME_FILE_PROPERTY(type, member, PropName, default)` for value-type fields and
`GAME_FILE_PROPERTY_REF` / `GAME_FILE_PROPERTY_CONST_REF` for reference-type fields.

## Build Workflows

### Ninja (preferred, debug)
```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

### With tests
```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELINK_BUILD_TESTS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

### Python wheel (scikit-build-core)
```powershell
pip install scikit-build-core pybind11
pip install -e . --no-build-isolation
```
Build outputs go to `build/{wheel_tag}/` (see `pyproject.toml`). The installed Python package
is `pyrelink`; pybind modules are named `_bindings` in each sub-package.

### CMake options
| Option | Default | Effect |
|---|---|---|
| `FIRELINK_BUILD_TESTS` | ON | Build doctest unit tests |
| `FIRELINK_BUILD_PYBIND` | ON | Build pybind11 Python extension modules |
| `BUILD_SHARED_LIBS` | — | Static libs get `_static` output name suffix on MSVC |

## Testing

Tests use **doctest** (fetched automatically). Each library test lives in
`tests/<Library>/`. The `TEST_RESOURCES_DIR` compile definition points to
`test_resources/` containing real game files (e.g. Stormveil Castle MSB for ER).

Run a single test executable directly:
```powershell
./build/debug/tests/FirelinkERMaps/FirelinkERMaps_test.exe
```

Python tests use pytest and live in `python/tests/`.

## Conventions

- **Namespaces**: `Firelink` (core), `Firelink::DarkSouls1R` (DSR), `Firelink::EldenRing` (ER),
  `Firelink::EldenRing::Maps::MapStudio` (MSB), `Firelink::BinaryReadWrite` (I/O).
- **Precompiled headers**: `FirelinkCore` uses `src/pch.h` — add new common includes there,
  not scattered across source files.
- `NOMINMAX` is defined globally to suppress Windows `min`/`max` macros.
- `FirelinkERHavok` is currently disabled (commented out in `lib/CMakeLists.txt` and
  `tests/CMakeLists.txt`) — do not enable without checking readiness.
- Entries' names are stored as `std::u16string`; use `GetNameUTF8()` / `UTF16ToUTF8()` helpers
  when interfacing with standard strings.

## Key Files for Reference

| Purpose | File |
|---|---|
| CMake helper functions | `CMake/FirelinkModules.cmake` |
| CRTP GameFile base | `lib/FirelinkCore/include/FirelinkCore/GameFile.h` |
| Binary I/O primitives | `lib/FirelinkCore/include/FirelinkCore/BinaryReadWrite.h` |
| Memory hook base | `lib/FirelinkCore/include/FirelinkCore/BaseHook.h` |
| MSB top-level | `lib/FirelinkERMaps/include/FirelinkERMaps/MapStudio/MSB.h` |
| Entry reference system | `lib/FirelinkERMaps/include/FirelinkERMaps/MapStudio/EntryReference.h` |
| Binder archive | `lib/FirelinkCore/include/FirelinkCore/Binder.h` |

