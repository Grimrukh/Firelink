"""Type stubs for the pyrelink_flver pybind11 extension module."""

from __future__ import annotations

__all__ = [
    "Bone",
    "Dummy",
    "Texture",
    "GXItem",
    "Material",
    "FaceSet",
    "Mesh",
    "VertexUsage",
    "VertexDataFormat",
    "VertexDataType",
    "VertexArrayLayout",
    "SplitMeshDef",
    "SplitMeshParams",
    "MergedMesh",
    "FLVER",
    # TextureFinder
    "ImageFormat",
    "TextureFinder",
]

from enum import IntEnum
from pathlib import Path
from typing import Sequence

import numpy as np
from numpy.typing import ArrayLike, NDArray

from pyrelink.core import (
    GameFile, GameType, Binder, TPFTexture, Vector2, Vector3, EulerRad, Color4b, AABB
)

# --- Bone --------------------------------------------------------------------

class Bone:

    name: str
    usage_flags: int
    parent_bone_index: int
    child_bone_index: int
    next_sibling_bone_index: int
    previous_sibling_bone_index: int
    translate: Vector3
    rotate: EulerRad
    scale: Vector3
    bounding_box: AABB

    def __init__(self) -> None: ...

# --- Dummy -------------------------------------------------------------------

class Dummy:

    reference_id: int
    parent_bone_index: int
    attach_bone_index: int
    follows_attach_bone: bool
    use_upward_vector: bool
    unk_x30: int
    unk_x34: int
    translate: Vector3
    forward: Vector3
    upward: Vector3
    color: Color4b
    """RGBA color as bytes (0-255)."""

    def __init__(self) -> None: ...

# --- Texture -----------------------------------------------------------------

class Texture:
    """FLVER texture reference. Strings decoded in-place."""

    path: str
    texture_type: str | None
    """Decoded sampler type (e.g. ``'g_Diffuse'``), or ``None`` if absent."""
    scale: Vector2
    f2_unk_x10: int
    f2_unk_x11: bool
    f2_unk_x14: float
    f2_unk_x18: float
    f2_unk_x1c: float

    def __init__(self) -> None: ...

# --- GXItem ------------------------------------------------------------------

class GXItem:
    """C++ FLVER GXItem."""

    index: int

    def __init__(self) -> None: ...

    @property
    def category(self) -> bytes:
        """4-byte category identifier."""
        ...
    @category.setter
    def category(self, value: bytes) -> None:
        """Must be exactly 4 bytes."""
        ...
    @property
    def data(self) -> bytes:
        """Raw GX item payload."""
        ...
    @data.setter
    def data(self, value: bytes) -> None: ...
    @property
    def is_terminator(self) -> bool: ...

# --- Material ----------------------------------------------------------------

class Material:
    """FLVER Material. Strings decoded in-place."""

    name: str
    mat_def_path: str
    flags: int
    f2_unk_x18: int

    def __init__(self) -> None: ...

    @property
    def textures(self) -> list[Texture]:
        """Live reference to texture list (mutable in place)."""
        ...
    @textures.setter
    def textures(self, value: Sequence[Texture]) -> None:
        """Reassigns the whole texture list."""
        ...
    @property
    def gx_items(self) -> list[GXItem]:
        """Live reference to GX item list (mutable in place)."""
        ...
    @gx_items.setter
    def gx_items(self, value: Sequence[GXItem]) -> None:
        """Reassigns the whole GX item list."""
        ...

# --- FaceSet -----------------------------------------------------------------

class FaceSet:

    flags: int
    is_triangle_strip: bool
    use_backface_culling: bool
    unk_x06: int

    def __init__(self) -> None: ...

    @property
    def vertex_indices(self) -> NDArray[np.uint32]:
        """1-D array of vertex indices (zero-copy view when read)."""
        ...
    @vertex_indices.setter
    def vertex_indices(self, value: ArrayLike) -> None: ...

# --- Mesh --------------------------------------------------------------------

class Mesh:

    is_dynamic: bool
    default_bone_index: int
    bone_indices: list[int]
    uses_bounding_boxes: bool
    invalid_layout: bool
    index: int

    def __init__(self) -> None: ...

    @property
    def bounding_box(self) -> AABB: ...
    @property
    def material(self) -> Material: ...
    @material.setter
    def material(self, value: Material) -> None: ...
    @property
    def face_sets(self) -> list[FaceSet]: ...
    @face_sets.setter
    def face_sets(self, value: Sequence[FaceSet]) -> None:
        """Reassigns the whole face set list."""
        ...
    @property
    def vertex_color_count(self) -> int: ...
    @property
    def vertex_array_count(self) -> int: ...
    @property
    def vertex_count(self) -> int:
        """Vertex count of the first vertex array (0 if none)."""
        ...
    @property
    def use_backface_culling(self) -> bool:
        """Value of first FaceSet; raises if FaceSets disagree."""
        ...

# --- Vertex layout types -------------------------------------------------

class VertexUsage(IntEnum):
    """What role a vertex-layout field plays. Values match internal FLVER enum."""

    Position = 0
    BoneWeights = 1
    BoneIndices = 2
    Normal = 3,
    # 4 is unused.
    UV = 5
    Tangent = 6
    Bitangent = 7
    Color = 10
    Ignore = 0xFFFFFFFF # internal usage, not in real FLVERs


class VertexDataFormat(IntEnum):
    """Raw on-disk vertex field format code."""

    TwoFloats = 0x01
    ThreeFloats = 0x02
    FourFloats = 0x03
    FourBytesA = 0x10
    FourBytesB = 0x11
    FourBytesC = 0x13
    FourBytesD_NormalW = 0x12
    TwoShorts = 0x15
    FourShorts = 0x16
    FourShortsBones = 0x18
    FourShortsToFloats = 0x1A
    FourShortsToFloatsB = 0x2E
    FourBytesE = 0x2F
    EdgeCompressed = 0xF0
    Ignored = 0xFF


class VertexDataType:
    """A single field in a vertex layout."""

    usage: VertexUsage
    format: VertexDataFormat
    instance_index: int
    """For multi-instance usages (uv_0, uv_1, color_0, ...)."""
    unk_x00: int
    data_offset: int
    """Byte offset of this field within the compressed vertex."""

    def __init__(
        self,
        usage: VertexUsage = VertexUsage.Ignore,
        format: VertexDataFormat = VertexDataFormat.Ignored,
        instance_index: int = 0,
        unk_x00: int = 0,
        data_offset: int = 0,
    ) -> None: ...
    @property
    def compressed_size(self) -> int:
        """Size on disk."""
        ...


class VertexArrayLayout:
    """Ordered list of `VertexDataType` fields describing a decompressed vertex."""

    types: list[VertexDataType]

    def __init__(self, types: Sequence[VertexDataType] = ()) -> None: ...
    @property
    def compressed_vertex_size(self) -> int:
        """Cached total; filled in on read/build."""
        ...
    @property
    def decompressed_vertex_size(self) -> int:
        """Stride of the decompressed interleaved buffer."""
        ...
    def get_compressed_vertex_size(self) -> int:
        """Compute compressed vertex size for writing."""
        ...
    def get_hash(self) -> int: ...

# --- SplitMeshDef / SplitMeshParams -------------------------------------------

class SplitMeshDef:
    """One output FLVER submesh definition, supplied per distinct value of
    ``MergedMesh.faces[:, 3]``.
    """

    material: Material
    layout: VertexArrayLayout
    """Target (decompressed) layout for this submesh."""
    is_dynamic: bool
    """Skinned (always `bone_indices`) vs rigid (`normal_w` bone in newer games)."""
    use_backface_culling: bool
    default_bone_index: int
    uses_bounding_boxes: bool
    face_set_count: int
    """1..3; >1 duplicates the base face set as LOD copies."""
    uv_layer_names: list[str]
    """Global UV layer names (keys in `MergedMesh.loop_uvs`) feeding each local
    ``uv_<i>`` field of `layout`, indexed by local UV slot. If empty, each local
    slot defaults to "UVMap<i>".
    """

    def __init__(
        self,
        material: Material,
        layout: VertexArrayLayout,
        is_dynamic: bool = False,
        use_backface_culling: bool = True,
        default_bone_index: int = 0,
        uses_bounding_boxes: bool = True,
        face_set_count: int = 1,
        uv_layer_names: Sequence[str] = (),
    ) -> None: ...


class SplitMeshParams:
    """Settings for `MergedMesh.split_mesh()`.

    Defaults are geared towards Dark Souls 1 (PTDE/DSR).
    """

    use_mesh_bone_indices: bool
    """Whether vertex bone indices index into local mesh bone indices array
    (True, older games) or into the global FLVER bones (False, newer games).
    """
    max_bones_per_mesh: int
    """Maximum number of bones per FLVER mesh. Additional meshes are created
    automatically as required if this capacity is reached.
    """
    unused_bone_indices_are_minus_one: bool
    """If True, unused bone indices (zero weight) are marked -1 instead of 0."""
    normal_tangent_dot_threshold: float
    """Minimum dot product for merging FLVER vertices based on normal/tangent
    similarity. 1.0 (default) requires an exact match.
    """
    max_vertices_per_mesh: int
    """Maximum number of vertices per mesh (0 means unconstrained)."""
    is_flver0: bool
    """Whether this is an old `FLVER0` (e.g., Demon's Souls)."""

    def __init__(
        self,
        use_mesh_bone_indices: bool = True,
        max_bones_per_mesh: int = 38,
        unused_bone_indices_are_minus_one: bool = False,
        normal_tangent_dot_threshold: float = 1.0,
        max_vertices_per_mesh: int = 0,
        is_flver0: bool = False,
    ) -> None: ...

# --- MergedMesh --------------------------------------------------------------

class MergedMesh:
    """Merged mesh built from all FLVER meshes, with deduplicated vertices.

    Can also be default-constructed and populated manually from Python (e.g.
    to call `split_mesh()` without first building one from a `FLVER`).

    Array properties are zero-copy numpy views into C++ memory when read;
    they remain valid as long as this ``MergedMesh`` object is alive. Setters
    accept any array-like and copy their data into the underlying C++
    vectors, keeping `vertex_count` / `total_loop_count` / `face_count` in
    sync with whichever array is assigned (raising if row counts conflict
    with a previously assigned array).
    """

    class UVLayer:
        """One named UV layer. `data` is a flat ``(loop_count * dim)`` float array."""

        name: str
        """e.g. ``"UVMap0"``, ``"UVMap1"``."""
        dim: int
        """Columns per UV (usually 2, up to 4)."""
        data: list[float]
        """Flat ``(loop_count * dim)`` float array."""

        def __init__(
            self,
            name: str = "",
            dim: int = 2,
            data: Sequence[float] = (),
        ) -> None: ...

    def __init__(self) -> None:
        """Construct an empty MergedMesh to populate manually from Python."""
        ...

    vertex_count: int
    total_loop_count: int
    face_count: int
    vertices_merged: bool

    @property
    def positions(self) -> NDArray[np.float32]:
        """Shape ``(vertex_count, 3)``."""
        ...
    @positions.setter
    def positions(self, value: ArrayLike) -> None: ...
    @property
    def bone_weights(self) -> NDArray[np.float32]:
        """Shape ``(vertex_count, 4)``."""
        ...
    @bone_weights.setter
    def bone_weights(self, value: ArrayLike) -> None: ...
    @property
    def bone_indices(self) -> NDArray[np.int32]:
        """Shape ``(vertex_count, 4)``."""
        ...
    @bone_indices.setter
    def bone_indices(self, value: ArrayLike) -> None: ...
    @property
    def loop_vertex_indices(self) -> NDArray[np.uint32]:
        """Shape ``(total_loop_count,)``."""
        ...
    @loop_vertex_indices.setter
    def loop_vertex_indices(self, value: ArrayLike) -> None: ...
    @property
    def loop_normals(self) -> NDArray[np.float32] | None:
        """Shape ``(total_loop_count, 3)`` or ``None``."""
        ...
    @loop_normals.setter
    def loop_normals(self, value: ArrayLike | None) -> None: ...
    @property
    def loop_normals_w(self) -> NDArray[np.uint8] | None:
        """Shape ``(total_loop_count, 1)`` or ``None``."""
        ...
    @loop_normals_w.setter
    def loop_normals_w(self, value: ArrayLike | None) -> None: ...
    @property
    def loop_tangents(self) -> list[NDArray[np.float32]]:
        """List of tangent slot arrays, each shape ``(total_loop_count, 4)``."""
        ...
    @loop_tangents.setter
    def loop_tangents(self, value: Sequence[ArrayLike]) -> None: ...
    @property
    def loop_bitangents(self) -> NDArray[np.float32] | None:
        """Shape ``(total_loop_count, 4)`` or ``None``."""
        ...
    @loop_bitangents.setter
    def loop_bitangents(self, value: ArrayLike | None) -> None: ...
    @property
    def loop_vertex_colors(self) -> list[NDArray[np.float32]]:
        """List of vertex color slot arrays, each shape ``(total_loop_count, 4)``."""
        ...
    @loop_vertex_colors.setter
    def loop_vertex_colors(self, value: Sequence[ArrayLike]) -> None: ...
    @property
    def loop_uvs(self) -> dict[str, NDArray[np.float32]]:
        """UV layers keyed by name."""
        ...
    @loop_uvs.setter
    def loop_uvs(self, value: Sequence[MergedMesh.UVLayer] | dict[str, ArrayLike]) -> None:
        """Settable from a list of `MergedMesh.UVLayer` (preserves order and
        each layer's exact `dim`), or a dict of name -> (N, dim) array-like
        (dim inferred per-entry) for convenience.
        """
        ...
    @property
    def faces(self) -> NDArray[np.uint32]:
        """Shape ``(face_count, 4)``."""
        ...
    @faces.setter
    def faces(self, value: ArrayLike) -> None: ...

    def split_mesh(
        self,
        split_mesh_defs: Sequence[SplitMeshDef],
        params: SplitMeshParams,
    ) -> list[Mesh]:
        """Split this merged mesh into FLVER submeshes, one per entry of
        `split_mesh_defs` (and possibly several per entry, when bone-count
        sub-splitting is required).
        """
        ...

# --- FLVER -------------------------------------------------------------------

class FLVERVersion(IntEnum):
    Null = 0x00000
    # FLVER0 (Demon's Souls era)
    DemonsSouls_0x0F = 0x0000F  # e.g. o9993
    DemonsSouls_0x10 = 0x00010  # e.g. c1200
    DemonsSouls_0x14 = 0x00014  # e.g. c7080, m07 map pieces
    DemonsSouls = 0x00015  # "standard" Demon's Souls
    # NOTE: no FLVER1 versions (gap in [0x10000, 0x1FFFF]).
    # FLVER2
    DarkSouls2_Armor9320 = 0x20009,
    DarkSouls_PS3_o0700_o0701 = 0x2000B
    DarkSouls_A = 0x2000C
    DarkSouls_B = 0x2000D
    DarkSouls2_NT = 0x2000F
    DarkSouls2 = 0x20010  # includes SOTFS
    Bloodborne_DS3_A = 0x20013
    Bloodborne_DS3_B = 0x20014
    Sekiro_TestChr = 0x20016
    Sekiro_EldenRing = 0x2001A
    ArmoredCore6 = 0x2001B


class FLVER(GameFile):
    """Top-level FLVER container. Inherits all GameFile I/O methods."""

    version: FLVERVersion
    big_endian: bool
    unicode: bool

    @property
    def bounding_box(self) -> AABB:
        """Axis-aligned bounding box (mutable reference)."""
        ...

    @property
    def true_face_count(self) -> int: ...
    @property
    def total_face_count(self) -> int: ...

    # FLVER0 preserved unknowns (read-write)
    f0_unk_x4a: int
    f0_unk_x4b: int
    f0_unk_x4c: int
    f0_unk_x5c: int

    # FLVER2 preserved unknowns (read-write)
    f2_unk_x4a: bool
    f2_unk_x4c: int
    f2_unk_x5c: int
    f2_unk_x5d: int
    f2_unk_x68: int

    @property
    def bones(self) -> list[Bone]:
        """Live reference to bone list (mutable in place)."""
        ...
    @bones.setter
    def bones(self, value: Sequence[Bone]) -> None:
        """Reassigns the whole bone list."""
        ...
    @property
    def dummies(self) -> list[Dummy]:
        """Live reference to dummy list (mutable in place)."""
        ...
    @dummies.setter
    def dummies(self, value: Sequence[Dummy]) -> None:
        """Reassigns the whole dummy list."""
        ...
    @property
    def meshes(self) -> list[Mesh]:
        """Live reference to mesh list (mutable in place)."""
        ...
    @meshes.setter
    def meshes(self, value: Sequence[Mesh]) -> None:
        """Reassigns the whole mesh list."""
        ...

    @property
    def bone_count(self) -> int: ...
    @property
    def dummy_count(self) -> int: ...
    @property
    def mesh_count(self) -> int: ...

    def has_cached_merged_mesh(self) -> bool:
        """Return True if a cached MergedMesh is available for this FLVER."""
        ...

    def get_cached_merged_mesh(self) -> MergedMesh:
        """Get cached MergedMesh. Raises an exception if it doesn't exist."""
        ...

    def update_cached_merged_mesh(
        self,
        mesh_material_indices: Sequence[int] = (),
        material_uv_layer_names: Sequence[Sequence[str]] = (),
        merge_vertices: bool = True,
    ) -> MergedMesh:
        """Update and return new cached MergedMesh."""
        ...

    def clear_cached_merged_mesh(self) -> None:
        """Clear cached MergedMesh."""
        ...

    @classmethod
    def update_cached_merged_meshes_parallel(
        cls,
        flvers: Sequence[FLVER],
        mesh_material_indices: Sequence[list[int]] | None = None,
        material_uv_layer_names: Sequence[list[list[str]]] | None = None,
        merge_vertices: Sequence[bool] = None,
    ) -> list[bool]:
        """Update the cached Merged Meshes of multiple FLVERs at once, with per-FLVER arguments.

        Returns `True` or `False` for each FLVER, depending on whether any errors occurred.
        """
        ...

    def build_merged_mesh(
        self,
        mesh_material_indices: list[int] | None = None,
        material_uv_layer_names: list[list[str]] | None = None,
        merge_vertices: bool = True,
    ) -> MergedMesh:
        """Build a MergedMesh from this FLVER."""
        ...

    @classmethod
    def from_paths_parallel_with_merged_mesh(
        cls,
        paths: list[str | Path],
        max_threads: int = 0
    ) -> list[FLVER]:
        """Load FLVERs from multiple paths in parallel and auto-compute cached MergedMesh."""
        ...

    @classmethod
    def from_bytes_parallel_with_merged_mesh(
        cls,
        buffers: list[bytes | bytearray | memoryview],
        max_threads: int = 0,
    ) -> list[FLVER]:
        """Load FLVERs from multiple buffers in parallel and auto-compute cached MergedMesh."""
        ...


# ---------------------------------------------------------------------------
# TextureFinder
# ---------------------------------------------------------------------------

class ImageFormat(IntEnum):
    DDS = 0
    PNG = 1
    TGA = 2


class TextureFinder:
    """Lazy texture discovery and caching for FromSoftware game files."""

    def __init__(self, game: GameType, data_root: Path | str) -> None:
        """Create a TextureFinder for the given game and data root directory."""
        ...

    def register_flver_sources(
        self,
        flver_source_path: Path | str,
        flver_binder: Binder | None = None,
        prefer_hi_res: bool = True,
    ) -> None:
        """Register texture source locations for a FLVER file."""
        ...

    def get_texture(self, texture_stem: str, model_name: str = "") -> TPFTexture | None:
        """Look up a texture by stem. Returns None if not found."""
        ...

    def get_texture_as(
        self,
        texture_stem: str,
        format: ImageFormat,
        model_name: str = "",
    ) -> bytes:
        """Get texture converted to the requested format. Returns empty bytes if not found."""
        ...

    def set_aet_root(self, aet_root: str) -> None:
        """Manually set the AET root directory for asset texture lookups."""
        ...

    @property
    def cached_texture_count(self) -> int: ...

    def __repr__(self) -> str: ...
