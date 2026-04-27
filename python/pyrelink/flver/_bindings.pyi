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
    "MergedMesh",
    "FLVER",
    "batch_from_path",
    "batch_from_bytes",
    # TextureFinder
    "ImageFormat",
    "TextureFinder",
]

from enum import IntEnum
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from pyrelink.core import GameFile, GameType, Binder, TPFTexture, Vector2, Vector3, Vector4, EulerRad, Color4b, AABB

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

# --- GXItem ------------------------------------------------------------------

class GXItem:
    """C++ FLVER GXItem."""

    index: int

    @property
    def category(self) -> bytes:
        """4-byte category identifier."""
        ...
    @property
    def data(self) -> bytes:
        """Raw GX item payload."""
        ...
    @property
    def is_terminator(self) -> bool: ...

# --- Material ----------------------------------------------------------------

class Material:
    """FLVER Material. Strings decoded in-place."""

    name: str
    mat_def_path: str
    flags: int
    f2_unk_x18: int

    @property
    def textures(self) -> list[Texture]:
        """Live reference to texture list (mutable)."""
        ...
    @property
    def gx_items(self) -> list[GXItem]:
        """Live reference to GX item list (mutable)."""
        ...

# --- FaceSet -----------------------------------------------------------------

class FaceSet:

    flags: int
    is_triangle_strip: bool
    use_backface_culling: bool
    unk_x06: int

    @property
    def vertex_indices(self) -> NDArray[np.uint32]:
        """1-D array of vertex indices (zero-copy view)."""
        ...

# --- Mesh --------------------------------------------------------------------

class Mesh:

    is_dynamic: bool
    default_bone_index: int
    bone_indices: list[int]
    uses_bounding_boxes: bool
    invalid_layout: bool
    index: int

    @property
    def material(self) -> Material: ...
    @property
    def face_sets(self) -> list[FaceSet]: ...
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

# --- MergedMesh --------------------------------------------------------------

class MergedMesh:
    """Merged mesh built from all FLVER meshes, with deduplicated vertices.

    Array properties are zero-copy numpy views into C++ memory; they remain
    valid as long as this ``MergedMesh`` object is alive.
    """

    vertex_count: int
    total_loop_count: int
    face_count: int
    vertices_merged: bool

    @property
    def positions(self) -> NDArray[np.float32]:
        """Shape ``(vertex_count, 3)``."""
        ...
    @property
    def bone_weights(self) -> NDArray[np.float32]:
        """Shape ``(vertex_count, 4)``."""
        ...
    @property
    def bone_indices(self) -> NDArray[np.int32]:
        """Shape ``(vertex_count, 4)``."""
        ...
    @property
    def loop_vertex_indices(self) -> NDArray[np.uint32]:
        """Shape ``(total_loop_count,)``."""
        ...
    @property
    def loop_normals(self) -> NDArray[np.float32] | None:
        """Shape ``(total_loop_count, 3)`` or ``None``."""
        ...
    @property
    def loop_normals_w(self) -> NDArray[np.uint8] | None:
        """Shape ``(total_loop_count, 1)`` or ``None``."""
        ...
    @property
    def loop_tangents(self) -> list[NDArray[np.float32]]:
        """List of tangent slot arrays, each shape ``(total_loop_count, 4)``."""
        ...
    @property
    def loop_bitangents(self) -> NDArray[np.float32] | None:
        """Shape ``(total_loop_count, 4)`` or ``None``."""
        ...
    @property
    def loop_vertex_colors(self) -> list[NDArray[np.float32]]:
        """List of vertex color slot arrays, each shape ``(total_loop_count, 4)``."""
        ...
    @property
    def loop_uvs(self) -> dict[str, NDArray[np.float32]]:
        """UV layers keyed by name."""
        ...
    @property
    def faces(self) -> NDArray[np.uint32]:
        """Shape ``(face_count, 4)``."""
        ...

# --- FLVER -------------------------------------------------------------------

class FLVER(GameFile):
    """Top-level FLVER container. Inherits all GameFile I/O methods."""

    version: int
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
        """Live reference to bone list (mutable)."""
        ...
    @property
    def dummies(self) -> list[Dummy]:
        """Live reference to dummy list (mutable)."""
        ...
    @property
    def meshes(self) -> list[Mesh]:
        """Live reference to mesh list (mutable)."""
        ...

    @property
    def bone_count(self) -> int: ...
    @property
    def dummy_count(self) -> int: ...
    @property
    def mesh_count(self) -> int: ...

    def build_merged_mesh(
        self,
        mesh_material_indices: list[int] | None = None,
        material_uv_layer_names: list[list[str]] | None = None,
        merge_vertices: bool = True,
    ) -> MergedMesh:
        """Build a MergedMesh from this FLVER."""
        ...


# --- Module-level functions --------------------------------------------------

def batch_from_path(
    paths: list[str | Path],
    max_threads: int = 0,
    cache_merged_mesh: bool = False,
) -> list[FLVER]:
    """Load multiple FLVERs from file paths in parallel.

    Args:
        paths: List of file paths (``str`` or ``Path``).
        max_threads: Maximum number of threads. ``0`` uses hardware concurrency.
        cache_merged_mesh: Automatically build MergedMesh during load.

    Returns:
        List of ``FLVER`` objects in input order.
    """
    ...


def batch_from_bytes(
    data_list: list[bytes],
    max_threads: int = 0,
    cache_merged_mesh: bool = False,
) -> list[FLVER]:
    """Parse multiple FLVERs from raw (decompressed) byte buffers in parallel.

    Args:
        data_list: List of raw FLVER ``bytes`` objects.
        max_threads: Maximum number of threads. ``0`` uses hardware concurrency.
        cache_merged_mesh: Automatically build MergedMesh during load.

    Returns:
        List of ``FLVER`` objects in input order.
    """
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

    def __init__(self, game: GameType, data_root: str) -> None:
        """Create a TextureFinder for the given game and data root directory."""
        ...

    def register_flver_sources(
        self,
        flver_source_path: str,
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
