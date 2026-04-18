"""Type stubs for the pyrelink_core pybind11 extension module (DCX + DDS)."""

from __future__ import annotations

__all__ = [
    # Binder
    "BinderVersion",
    "BinderFlags",
    "BinderVersion4Info",
    "BinderEntry",
    "BinderError",
    "Binder",
    # DCX
    "DCXType",
    "DCXResult",
    "DCXError",
    "is_dcx",
    "detect_dcx",
    "decompress_dcx",
    "compress_dcx",
    # Oodle
    "set_oodle_dll_path",
    "load_oodle_dll",
    "is_oodle_available",
    "oodle_dll_path",
    # DDS
    "DXGIFormat",
    "convert_dds_to_tga",
    "convert_dds_to_png",
    "convert_tga_to_dds",
    "convert_png_to_dds",
    # ImageImportManager
    "GameType",
    "ImageFormat",
    "ImageImportManager",
    # TPF
    "TPFPlatform",
    "TextureType",
    "TPFTexture",
    "TPFError",
    "TPF",
]

from enum import IntEnum
from typing import Union, Optional

# ---------------------------------------------------------------------------
# Binder
# ---------------------------------------------------------------------------

class BinderVersion(IntEnum):
    """Binder archive version."""
    V3 = 3
    V4 = 4

class BinderFlags:
    """Bit flags for a Binder archive header."""
    value: int
    @property
    def is_big_endian(self) -> bool: ...
    @property
    def has_ids(self) -> bool: ...
    @property
    def has_names(self) -> bool: ...
    @property
    def has_long_offsets(self) -> bool: ...
    @property
    def has_compression(self) -> bool: ...
    @property
    def entry_header_size(self) -> int: ...

class BinderVersion4Info:
    """Extra info for BND4 binders."""
    unknown1: bool
    unknown2: bool
    unicode: bool
    hash_table_type: int

class BinderEntry:
    """A single entry in a Binder archive."""
    entry_id: int
    path: str
    flags: int
    data: bytes
    @property
    def name(self) -> str: ...

class BinderError(RuntimeError):
    """Error raised by Binder operations."""
    ...

class Binder:
    """A FromSoftware BND3/BND4 multi-file archive."""
    version: BinderVersion
    signature: str
    flags: BinderFlags
    big_endian: bool
    bit_big_endian: bool
    entries: list[BinderEntry]

    @staticmethod
    def from_bytes(data: Union[bytes, bytearray, memoryview]) -> "Binder":
        """Parse a BND3 or BND4 archive from raw bytes (must already be DCX-decompressed)."""
        ...

    @staticmethod
    def from_split_bytes(
        bhd_data: Union[bytes, bytearray, memoryview],
        bdt_data: Union[bytes, bytearray, memoryview],
    ) -> "Binder":
        """Parse a split BHF/BDT binder from raw bytes."""
        ...

    def to_bytes(self) -> bytes:
        """Serialize the Binder back to BND3 or BND4 bytes."""
        ...

    @property
    def entry_count(self) -> int: ...

    def find_entry_by_id(self, entry_id: int) -> Optional[BinderEntry]:
        """Find entry by ID. Returns None if not found."""
        ...

    def find_entry_by_name(self, name: str) -> Optional[BinderEntry]:
        """Find entry by basename. Returns None if not found."""
        ...

    def __len__(self) -> int: ...


# ---------------------------------------------------------------------------
# DCX
# ---------------------------------------------------------------------------

class DCXType(IntEnum):
    """DCX compression type identifier."""
    Unknown = -1
    Null = 0
    Zlib = 1
    DCP_EDGE = 2
    DCP_DFLT = 3
    DCX_EDGE = 4
    DCX_DFLT_10000_24_9 = 5
    DCX_DFLT_10000_44_9 = 6
    DCX_DFLT_11000_44_8 = 7
    DCX_DFLT_11000_44_9 = 8
    DCX_DFLT_11000_44_9_15 = 9
    DCX_KRAK = 10
    DCX_ZSTD = 11

class DCXResult:
    """Result of DCX decompression."""
    @property
    def data(self) -> bytes:
        """Decompressed payload."""
        ...
    @property
    def type(self) -> DCXType:
        """Detected DCX variant."""
        ...

class DCXError(RuntimeError):
    """Error raised by DCX operations."""
    ...

def is_dcx(data: Union[bytes, bytearray, memoryview]) -> bool:
    """Return True if data appears to be DCX/DCP compressed (or raw zlib)."""
    ...

def detect_dcx(data: Union[bytes, bytearray, memoryview]) -> DCXType:
    """Detect the DCXType from the header without decompressing."""
    ...

def decompress_dcx(data: Union[bytes, bytearray, memoryview]) -> DCXResult:
    """Decompress DCX/DCP/raw-zlib data.

    Returns a DCXResult with .data (bytes) and .type (DCXType).
    """
    ...

def compress_dcx(
    data: Union[bytes, bytearray, memoryview],
    dcx_type: DCXType,
) -> bytes:
    """Compress data into the given DCX format."""
    ...

# ---------------------------------------------------------------------------
# Oodle DLL configuration
# ---------------------------------------------------------------------------

def set_oodle_dll_path(directory: str) -> None:
    """Set the directory to search for oo2core_6_win64.dll before default paths.

    Must be called before the first DCX_KRAK compress/decompress call to take effect.
    """
    ...

def load_oodle_dll(directory: str) -> bool:
    """Explicitly load oo2core_6_win64.dll from the given directory.

    Returns True on success. Can be called at any time.
    """
    ...

def is_oodle_available() -> bool:
    """Return True if the Oodle DLL has been loaded and is ready for use."""
    ...

def oodle_dll_path() -> str:
    """Return the path the Oodle DLL was loaded from, or empty string if not loaded."""
    ...


# ---------------------------------------------------------------------------
# DDS
# ---------------------------------------------------------------------------

class DXGIFormat(IntEnum):
    """DXGI pixel format identifiers (subset relevant to FromSoftware textures)."""
    UNKNOWN = 0
    # Uncompressed
    R8G8B8A8_UNORM = 28
    R8G8B8A8_UNORM_SRGB = 29
    B8G8R8A8_UNORM = 87
    B8G8R8A8_UNORM_SRGB = 91
    # BC compressed
    BC1_UNORM = 71
    BC1_UNORM_SRGB = 72
    BC2_UNORM = 74
    BC2_UNORM_SRGB = 75
    BC3_UNORM = 77
    BC3_UNORM_SRGB = 78
    BC4_UNORM = 80
    BC4_SNORM = 81
    BC5_UNORM = 83
    BC5_SNORM = 84
    BC6H_UF16 = 95
    BC6H_SF16 = 96
    BC7_UNORM = 98
    BC7_UNORM_SRGB = 99
    # Single/dual channel
    R8_UNORM = 61
    R8G8_UNORM = 49
    R16_FLOAT = 54
    R16G16_FLOAT = 34
    R16G16B16A16_FLOAT = 10
    R32_FLOAT = 41
    R32G32B32A32_FLOAT = 2


def convert_dds_to_tga(data: Union[bytes, bytearray, memoryview]) -> bytes:
    """Convert DDS texture bytes to TGA format.

    Block-compressed formats (BC1-BC7) are automatically decompressed.
    """
    ...

def convert_dds_to_png(data: Union[bytes, bytearray, memoryview]) -> bytes:
    """Convert DDS texture bytes to PNG format.

    Block-compressed formats (BC1-BC7) are automatically decompressed.
    """
    ...

def convert_tga_to_dds(
    data: Union[bytes, bytearray, memoryview],
    format: DXGIFormat,
) -> bytes:
    """Convert TGA image bytes to DDS with the specified DXGI pixel format.

    BC6H/BC7 compression uses GPU acceleration when available.
    """
    ...

def convert_png_to_dds(
    data: Union[bytes, bytearray, memoryview],
    format: DXGIFormat,
) -> bytes:
    """Convert PNG image bytes to DDS with the specified DXGI pixel format.

    BC6H/BC7 compression uses GPU acceleration when available.
    """
    ...


# ---------------------------------------------------------------------------
# ImageImportManager
# ---------------------------------------------------------------------------

class GameType(IntEnum):
    """Game identifier for ImageImportManager."""
    DemonsSouls = 0
    DarkSoulsPTDE = 1
    DarkSoulsDSR = 2
    Bloodborne = 3
    DarkSouls3 = 4
    Sekiro = 5
    EldenRing = 6


class ImageFormat(IntEnum):
    DDS = 0
    PNG = 1
    TGA = 2


class ImportImageManager:
    """
    /// @brief Construct a manager for the given game.
        /// @param game       Which FromSoftware game.
        /// @param data_root  Root of unpacked game data (e.g. "DARK SOULS REMASTERED", "ELDEN RING/Game").
        ImageImportManager(GameType game, std::filesystem::path data_root);

        /// @brief Register texture source locations for a FLVER loaded from `flver_source_path`.
        ///
        /// `flver_source_path` is the path to the file the FLVER was loaded from
        /// (e.g. "chr/c2300.chrbnd.dcx" or a loose "map/.../m1234.flver").
        ///
        /// If the FLVER came from a Binder that has already been opened, pass it as
        /// `flver_binder` so its TPF entries can be scanned without re-reading the file.
        void RegisterFLVERSources(
            const std::filesystem::path& flver_source_path,
            const Binder* flver_binder = nullptr,
            bool prefer_hi_res = true);

        /// @brief Look up a texture by stem (case-insensitive). Returns nullptr if not found.
        ///
        /// Lazily loads pending TPFs and Binders as needed.
        const TPFTexture* GetTexture(const std::string& texture_stem,
                                     const std::string& model_name = "");

        /// @brief Get texture data converted to the requested format.
        ///
        /// Returns empty vector if texture not found.
        std::vector<std::byte> GetTextureAs(const std::string& texture_stem,
                                            ImageFormat format,
                                            const std::string& model_name = "");

        /// @brief Manually set the AET root directory for asset texture lookups.
        void SetAETRoot(const std::filesystem::path& aet_root) { aet_root_ = aet_root; }

        /// @brief Get the number of cached textures.
        [[nodiscard]] std::size_t CachedTextureCount() const;

    """

    def __init__(self, game: GameType, data_root: str) -> None: ...

    def register_flver_sources(
        self,
        flver_source_path: str,
        flver_binder: Optional[Binder] = None,
        prefer_hi_res: bool = True,
    ) -> None: ...

    def get_texture(self, texture_stem: str, model_name: str = "") -> Optional[TPFTexture]: ...

    def get_texture_as(self, texture_stem: str, format: ImageFormat, model_name: str = "") -> bytes: ...

    def set_aet_root(self, aet_root: str) -> None: ...

    @property
    def cached_texture_count(self) -> int: ...

    def __repr__(self) -> str: ...


# ---------------------------------------------------------------------------
# TPF
# ---------------------------------------------------------------------------

class TPFPlatform(IntEnum):
    """TPF target platform."""
    PC = 0
    Xbox360 = 1
    PS3 = 2
    PS4 = 4
    XboxOne = 5

class TextureType(IntEnum):
    """TPF texture type."""
    Texture = 0
    Cubemap = 1
    Volume = 2

class TPFTexture:
    """A single texture in a TPF archive."""
    stem: str
    format: int
    texture_type: TextureType
    mipmap_count: int
    texture_flags: int
    data: bytes

class TPFError(RuntimeError):
    """Error raised by TPF operations."""
    ...

class TPF:
    """A FromSoftware TPF texture pack file."""
    platform: TPFPlatform
    tpf_flags: int
    encoding_type: int
    textures: list[TPFTexture]

    @staticmethod
    def from_bytes(data: Union[bytes, bytearray, memoryview]) -> "TPF":
        """Parse a TPF from raw bytes (must already be DCX-decompressed)."""
        ...

    def to_bytes(self) -> bytes:
        """Serialize the TPF back to bytes."""
        ...

    @property
    def texture_count(self) -> int: ...

    def find_texture(self, stem: str) -> Optional[TPFTexture]:
        """Find texture by stem (case-insensitive). Returns None if not found."""
        ...

    def __len__(self) -> int: ...
