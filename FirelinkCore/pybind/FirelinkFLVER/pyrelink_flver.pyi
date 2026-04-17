"""Type stubs for the flver_cpp pybind11 extension module."""

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
]

from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from soulstruct.flver.base import (
    BaseFLVER,
    BaseFLVERBone,
    BaseFLVERDummy,
    BaseFLVERTexture,
    BaseFLVERGXItem,
    BaseFLVERMaterial,
    BaseFLVERFaceSet,
    BaseFLVERMesh,
    BaseMergedMesh,
)

# --- Bone --------------------------------------------------------------------

class Bone(BaseFLVERBone):
    """Read-only view of a C++ FLVERBone. Strings decoded in-place to UTF-8."""

    name: str
    usage_flags: int
    parent_bone_index: int
    child_bone_index: int
    next_sibling_bone_index: int
    previous_sibling_bone_index: int

    @property
    def translate(self) -> tuple[float, float, float]: ...
    @property
    def rotate(self) -> tuple[float, float, float]: ...
    @property
    def scale(self) -> tuple[float, float, float]: ...
    @property
    def bounding_box_min(self) -> tuple[float, float, float]: ...
    @property
    def bounding_box_max(self) -> tuple[float, float, float]: ...

# --- Dummy -------------------------------------------------------------------

class Dummy(BaseFLVERDummy):
    """Read-only view of a C++ FLVER Dummy."""

    reference_id: int
    parent_bone_index: int
    attach_bone_index: int
    follows_attach_bone: bool
    use_upward_vector: bool
    unk_x30: int
    unk_x34: int

    @property
    def translate(self) -> tuple[float, float, float]: ...
    @property
    def forward(self) -> tuple[float, float, float]: ...
    @property
    def upward(self) -> tuple[float, float, float]: ...
    @property
    def color_rgba(self) -> tuple[int, int, int, int]:
        """RGBA color as a 4-tuple of ints (0-255)."""
        ...

# --- Texture -----------------------------------------------------------------

class Texture(BaseFLVERTexture):
    """Read-only view of a C++ FLVER Texture. Strings decoded in-place."""

    path: str
    f2_unk_x10: int
    f2_unk_x11: bool
    f2_unk_x14: float
    f2_unk_x18: float
    f2_unk_x1c: float

    @property
    def texture_type(self) -> str | None:
        """Decoded sampler type (e.g. ``'g_Diffuse'``), or ``None`` if absent."""
        ...
    @property
    def scale(self) -> tuple[float, float]: ...

# --- GXItem ------------------------------------------------------------------

class GXItem(BaseFLVERGXItem):
    """Read-only view of a C++ FLVER GXItem."""

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

class Material(BaseFLVERMaterial):
    """Read-only view of a C++ FLVER Material. Strings decoded in-place."""

    name: str
    mat_def_path: str
    flags: int
    f2_unk_x18: int

    @property
    def textures(self) -> list[Texture]: ...
    @property
    def gx_items(self) -> list[GXItem]: ...

# --- FaceSet -----------------------------------------------------------------

class FaceSet(BaseFLVERFaceSet):
    """Read-only view of a C++ FLVER FaceSet."""

    flags: int
    is_triangle_strip: bool
    use_backface_culling: bool
    unk_x06: int

    @property
    def vertex_indices(self) -> NDArray[np.uint32]:
        """1-D array of vertex indices (zero-copy view)."""
        ...

# --- Mesh --------------------------------------------------------------------

class Mesh(BaseFLVERMesh):
    """Read-only view of a C++ FLVERMesh."""

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
    def use_backface_culling(self):
        """Uses value of first FaceSet and throws if other FaceSets have different values."""
        ...

# --- MergedMesh --------------------------------------------------------------

class MergedMesh(BaseMergedMesh):
    """Merged mesh built from all FLVER meshes, with deduplicated vertices.

    Array properties are zero-copy numpy views into C++ memory. They remain
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

# --- FLVER (main entry point) -----------------------------------------------

class FLVER(BaseFLVER):
    """C++ FLVER reader. Construct from raw (decompressed) FLVER bytes.

    After parsing, all string fields on bones, materials, and textures are
    decoded in-place to UTF-8, so ``bone.name``, ``material.name``, etc.
    are proper Python ``str`` values.
    """

    def __init__(self, data: bytes) -> None:
        """Parse a FLVER from raw (decompressed) bytes."""
        ...

    @staticmethod
    def from_path(path: str | Path) -> FLVER:
        """Read a FLVER from a file path, automatically decompressing DCX if needed.

        Sets ``path`` and ``dcx_type`` on the returned object.
        """
        ...

    @property
    def dcx_type(self) -> int: ...
    @dcx_type.setter
    def dcx_type(self, dcx_type: int) -> None: ...

    @property
    def path(self) -> Path | None:
        """Must be set by caller."""
        ...

    @path.setter
    def path(self, path: Path | None) -> None:
        """Set the FLVER's path (for error reporting)."""
        ...

    @property
    def path_name(self) -> str | None:
        """Get name of `path`."""
        ...
    @property
    def path_stem(self) -> str | None:
        """NOTE: Like `path.stem`, this only removes the LAST suffix, e.g. 'c1000.chrbnd.dcx' -> 'c1000.chrbnd'."""
        ...
    @property
    def path_minimal_stem(self) -> str | None:
        """Removes ALL suffixes from `path` name, e.g. 'c1000.chrbnd.dcx' -> 'c1000'."""
        ...

    @property
    def version(self) -> int: ...
    @property
    def big_endian(self) -> bool: ...
    @property
    def unicode(self) -> bool: ...
    @property
    def bounding_box_min(self) -> tuple[float, float, float]: ...
    @property
    def bounding_box_max(self) -> tuple[float, float, float]: ...
    @property
    def true_face_count(self) -> int: ...
    @property
    def total_face_count(self) -> int: ...

    # FLVER0 unknowns
    @property
    def f0_unk_x4a(self) -> int: ...
    @property
    def f0_unk_x4b(self) -> int: ...
    @property
    def f0_unk_x4c(self) -> int: ...
    @property
    def f0_unk_x5c(self) -> int: ...

    # FLVER2 unknowns
    @property
    def f2_unk_x4a(self) -> bool: ...
    @property
    def f2_unk_x4c(self) -> int: ...
    @property
    def f2_unk_x5c(self) -> int: ...
    @property
    def f2_unk_x5d(self) -> int: ...
    @property
    def f2_unk_x68(self) -> int: ...

    # Collections (reference views into C++ data)
    @property
    def bones(self) -> list[Bone]: ...
    @property
    def dummies(self) -> list[Dummy]: ...
    @property
    def meshes(self) -> list[Mesh]: ...

    # Convenience counts
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
) -> list[FLVER]:
    """Load multiple FLVERs from file paths in parallel.

    Reads and DCX-decompresses all files, then parses them in parallel
    using C++ threads (releasing the GIL during parsing).

    Args:
        paths: List of file paths (``str`` or ``Path``).
        max_threads: Maximum number of threads. ``0`` (default) uses
            ``std::thread::hardware_concurrency()``.

    Returns:
        List of ``FLVER`` objects in the same order as the input paths.
    """
    ...


def batch_from_bytes(
    data_list: list[bytes],
    max_threads: int = 0,
) -> list[FLVER]:
    """Parse multiple FLVERs from raw (decompressed) byte buffers in parallel.

    Each element of *data_list* should be a ``bytes`` object containing
    already-decompressed FLVER data.  Parsing happens in parallel using
    C++ threads (the GIL is released during parsing).

    Args:
        data_list: List of raw FLVER ``bytes`` objects.
        max_threads: Maximum number of threads. ``0`` (default) uses
            ``std::thread::hardware_concurrency()``.

    Returns:
        List of ``FLVER`` objects in the same order as the input list.
    """
    ...


