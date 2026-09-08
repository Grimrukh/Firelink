# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## Unreleased

### Added
- Bloodborne (PS4) DDS texture de/swizzling support.
- `TPFTexture::ToDDS` method added to get new `DDS` instance.
- `MergedMeshSplit` prototype with Python bindings.

### Changed
- More convenient BufferReader/Writer methods for using decoded strings.
- `DDS` class used instead of static namespace functions.

### Fixed
- Big-endian files (e.g. Demon's Souls) have strings decoded/encoded correctly.
- Big-endian FLVERs have vertex data decoded/encoded correctly.
- Some `constexpr std::string` expressions fixed (can't compile in MSVC Debug).
- `TextureFinder` correctly finds Common* TPFs in 'parts' folder
- `TextureFinder` correctly loads loose map TPFs

---

## [0.1.0] - 2026-05-14

**Initial release.**

### Added
- Libraries:
  - FirelinkCore: Container classes, game definitions, DCX compression, DDS textures, read/write, logging, utilities.
  - FirelinkDSRHook: Memory hook for Dark Souls: Remastered.
  - FirelinkERHook: Memory hook for Elden Ring.
  - FirelinkERMaps: Map classes (MSB) for Elden Ring.
  - FirelinkFLVER: FLVER class support for all games and DDS texture utilities.
- Experimental FirelinkERHavok started (not yet built).
- `pyrelink` bindings for Python, with matching stub annotations for IDE support.
