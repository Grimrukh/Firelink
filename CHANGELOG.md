# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## Unreleased

### Changed
- More convenient BufferReader/Writer methods for using decoded strings.

### Fixed
- Big-endian files (e.g. Demon's Souls) have strings decoded correctly.

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
