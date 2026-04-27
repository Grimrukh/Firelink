"""Type stubs for the pyrelink_core pybind11 extension module."""

from __future__ import annotations

__all__ = [
    # Core
    "GameType",
    "Vector2",
    "Vector3",
    "Vector4",
    "EulerRad",
    "Color4b",
    "AABB",
    "GroupBitSet128",
    "GroupBitSet256",
    "GroupBitSet1024",
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
    # TPF
    "TPFPlatform",
    "TextureType",
    "TPFTexture",
    "TPFError",
    "TPF",
]

from pathlib import Path
from typing import Union, Optional, Sequence
from enum import IntEnum

# ---------------------------------------------------------------------------
# Primitive math types
# ---------------------------------------------------------------------------

class Vector2:
    """Basic XY vector."""
    x: float
    y: float
    def __init__(self, x: float = 0.0, y: float = 0.0) -> None: ...
    def __init__(self, seq: Sequence[float]) -> None: ...  # type: ignore[misc]
    def __len__(self) -> int: ...
    def __getitem__(self, i: int) -> float: ...
    def __setitem__(self, i: int, val: float) -> None: ...
    def __iter__(self): ...
    def __repr__(self) -> str: ...

class Vector3:
    """Basic XYZ vector."""
    x: float
    y: float
    z: float
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None: ...
    def __init__(self, seq: Sequence[float]) -> None: ...  # type: ignore[misc]
    def __len__(self) -> int: ...
    def __getitem__(self, i: int) -> float: ...
    def __setitem__(self, i: int, val: float) -> None: ...
    def __iter__(self): ...
    def __repr__(self) -> str: ...
    @staticmethod
    def zero() -> Vector3: ...
    @staticmethod
    def one() -> Vector3: ...

class Vector4:
    """Basic XYZW vector."""
    x: float
    y: float
    z: float
    w: float
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0, w: float = 0.0) -> None: ...
    def __init__(self, seq: Sequence[float]) -> None: ...  # type: ignore[misc]
    def __len__(self) -> int: ...
    def __getitem__(self, i: int) -> float: ...
    def __setitem__(self, i: int, val: float) -> None: ...
    def __iter__(self): ...
    def __repr__(self) -> str: ...

class EulerRad:
    """Euler XYZ angles in radians."""
    x: float
    y: float
    z: float
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None: ...
    def __init__(self, seq: Sequence[float]) -> None: ...  # type: ignore[misc]
    def __len__(self) -> int: ...
    def __getitem__(self, i: int) -> float: ...
    def __setitem__(self, i: int, val: float) -> None: ...
    def __iter__(self): ...
    def __repr__(self) -> str: ...
    @staticmethod
    def zero() -> EulerRad: ...

class Color4b:
    """Color RGBA as bytes [0-255]."""
    r: int
    g: int
    b: int
    a: int
    def __init__(self, r: int = 0, g: int = 0, b: int = 0, a: int = 255) -> None: ...
    def __init__(self, seq: Sequence[int]) -> None: ...  # type: ignore[misc]
    def __getitem__(self, i: int) -> int: ...
    def __setitem__(self, i: int, val: int) -> None: ...
    def __iter__(self): ...

class AABB:
    """Axis-aligned bounding box."""
    min: Vector3
    max: Vector3
    def __init__(self) -> None: ...

class GroupBitSet128:
    def __init__(self, bits: set[int] = ...) -> None: ...
    @staticmethod
    def all_on() -> GroupBitSet128: ...
    @staticmethod
    def all_off() -> GroupBitSet128: ...
    def enable(self, index: int) -> None: ...
    def disable(self, index: int) -> None: ...
    def __getitem__(self, i: int) -> bool: ...
    def to_list(self) -> list[int]: ...
    def __repr__(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...

class GroupBitSet256:
    def __init__(self, bits: set[int] = ...) -> None: ...
    @staticmethod
    def all_on() -> GroupBitSet256: ...
    @staticmethod
    def all_off() -> GroupBitSet256: ...
    def enable(self, index: int) -> None: ...
    def disable(self, index: int) -> None: ...
    def __getitem__(self, i: int) -> bool: ...
    def to_list(self) -> list[int]: ...
    def __repr__(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...

class GroupBitSet1024:
    def __init__(self, bits: set[int] = ...) -> None: ...
    @staticmethod
    def all_on() -> GroupBitSet1024: ...
    @staticmethod
    def all_off() -> GroupBitSet1024: ...
    def enable(self, index: int) -> None: ...
    def disable(self, index: int) -> None: ...
    def __getitem__(self, i: int) -> bool: ...
    def to_list(self) -> list[int]: ...
    def __repr__(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...

# ---------------------------------------------------------------------------
# GameFile (mixin — methods added to all GameFile subclasses by bind_game_file)
# ---------------------------------------------------------------------------

class GameFile:
    """Base mixin for all GameFile subclasses. Methods are defined on each concrete class."""

    def __init__(self) -> None:
        """All GameFiles are default constructible."""
        ...

    @classmethod
    def from_path(cls, path: Union[str, Path]) -> GameFile:
        """Read from a file path, decompressing DCX if needed."""
        ...

    @classmethod
    def from_paths_parallel(
        cls,
        paths: Sequence[Union[str, Path]],
        max_threads: int = 0,
    ) -> list[GameFile]:
        """Read multiple files in parallel, decompressing DCX if needed."""
        ...

    @classmethod
    def from_bytes(cls, data: Union[bytes, bytearray, memoryview]) -> GameFile:
        """Parse from raw (already DCX-decompressed) bytes."""
        ...

    @classmethod
    def from_bytes_parallel(
        cls,
        buffers: list[Union[bytes, bytearray, memoryview]],
        max_threads: int = 0,
    ) -> list[GameFile]:
        """Parse multiple byte buffers in parallel."""
        ...

    def to_bytes(self) -> bytes:
        """Serialize to bytes, compressing with DCX if dcx_type is set."""
        ...

    def write_to_path(self, path: Union[str, Path, None] = None) -> None:
        """Write to path (or stored path if None)."""
        ...

    @property
    def dcx_type(self) -> DCXType: ...
    @dcx_type.setter
    def dcx_type(self, value: DCXType) -> None: ...

    @property
    def path(self) -> str | None:
        """Stored file path as a string."""
        ...
    @path.setter
    def path(self, value: Union[str, Path, None]) -> None: ...

    @property
    def path_name(self) -> str | None:
        """Filename portion of the stored path (e.g. 'c1000.chrbnd.dcx'), or None."""
        ...

    @property
    def path_stem(self) -> str | None:
        """Minimal stem with ALL suffixes removed (e.g. 'c1000.chrbnd.dcx' -> 'c1000'), or None."""
        ...


class GameType(IntEnum):
    """Game identifier."""
    DemonsSouls = 0
    DarkSoulsPTDE = 1
    DarkSoulsDSR = 2
    Bloodborne = 3
    DarkSouls3 = 4
    Sekiro = 5
    EldenRing = 6

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
    def name(self) -> str:
        """Basename of the entry path."""
        ...
    @property
    def stem(self) -> str:
        """Minimal stem (before first '.') of the entry basename."""
        ...
    def __repr__(self) -> str: ...

class BinderError(RuntimeError):
    """Error raised by Binder operations."""
    ...

class Binder(GameFile):
    """A FromSoftware BND3/BND4 multi-file archive."""
    version: BinderVersion
    signature: str
    flags: BinderFlags
    big_endian: bool
    bit_big_endian: bool
    @property
    def entries(self) -> list[BinderEntry]:
        """Live reference to the internal entry list (mutable)."""
        ...

    @staticmethod
    def from_split_bytes(
        bhd_data: Union[bytes, bytearray, memoryview],
        bdt_data: Union[bytes, bytearray, memoryview],
    ) -> Binder:
        """Parse a split BHF/BDT binder from raw bytes."""
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
    def __repr__(self) -> str: ...


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
    def data(self) -> bytes: ...
    @property
    def type(self) -> DCXType: ...

class DCXError(RuntimeError): ...

def is_dcx(data: Union[bytes, bytearray, memoryview]) -> bool: ...
def detect_dcx(data: Union[bytes, bytearray, memoryview]) -> DCXType: ...
def decompress_dcx(data: Union[bytes, bytearray, memoryview]) -> DCXResult: ...
def compress_dcx(data: Union[bytes, bytearray, memoryview], dcx_type: DCXType) -> bytes: ...

# ---------------------------------------------------------------------------
# Oodle
# ---------------------------------------------------------------------------

def set_oodle_dll_path(directory: str) -> None: ...
def load_oodle_dll(directory: str) -> bool: ...
def is_oodle_available() -> bool: ...
def oodle_dll_path() -> str: ...

# ---------------------------------------------------------------------------
# DDS
# ---------------------------------------------------------------------------

class DXGIFormat(IntEnum):
    """DXGI pixel format identifiers (subset relevant to FromSoftware textures)."""
    UNKNOWN = 0
    R8G8B8A8_UNORM = 28
    R8G8B8A8_UNORM_SRGB = 29
    B8G8R8A8_UNORM = 87
    B8G8R8A8_UNORM_SRGB = 91
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
    R8_UNORM = 61
    R8G8_UNORM = 49
    R16_FLOAT = 54
    R16G16_FLOAT = 34
    R16G16B16A16_FLOAT = 10
    R32_FLOAT = 41
    R32G32B32A32_FLOAT = 2

def convert_dds_to_tga(data: Union[bytes, bytearray, memoryview]) -> bytes: ...
def convert_dds_to_png(data: Union[bytes, bytearray, memoryview]) -> bytes: ...
def convert_tga_to_dds(data: Union[bytes, bytearray, memoryview], format: DXGIFormat) -> bytes: ...
def convert_png_to_dds(data: Union[bytes, bytearray, memoryview], format: DXGIFormat) -> bytes: ...

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
    def __repr__(self) -> str: ...

class TPFError(RuntimeError): ...

class TPF(GameFile):
    """A FromSoftware TPF texture pack file."""
    platform: TPFPlatform
    tpf_flags: int
    encoding_type: int

    @property
    def textures(self) -> list[TPFTexture]:
        """Live reference to the internal texture list (mutable)."""
        ...

    @property
    def texture_count(self) -> int: ...

    def find_texture(self, stem: str) -> Optional[TPFTexture]:
        """Find texture by stem (case-insensitive). Returns None if not found."""
        ...

    def __len__(self) -> int: ...
    def __repr__(self) -> str: ...
