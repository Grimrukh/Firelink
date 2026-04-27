"""Pytest suite for Elden Ring MSB via pyrelink_er_maps — mirrors test_er_msb.cpp.

Uses the bundled Stormveil Castle MSB (m10_00_00_00.msb, DCX removed) to verify:
  1. MSB loads without error.
  2. All param counts are positive.
  3. Known subtype distributions are non-empty.
  4. Names, types, and cross-references resolve correctly.
  5. repr() works on entries.
  6. Draw-parent cross-references can be mutated.
  7. Write -> re-read produces identical param counts and entry data.
  8. Double write produces byte-identical files (stable serialization).
"""

from __future__ import annotations

import math
from pathlib import Path

import pytest

from _pyrelink_er_maps import *

# ============================================================================
# Fixtures
# ============================================================================

RESOURCES_DIR = Path(__file__).resolve().parent.parent / "resources"
MSB_PATH = RESOURCES_DIR / "m10_00_00_00.msb"


@pytest.fixture(scope="session")
def original_msb() -> MSB:
    """Load the bundled MSB once for the whole test session."""
    assert MSB_PATH.exists(), f"MSB resource not found: {MSB_PATH}"
    return MSB.from_path(MSB_PATH)


@pytest.fixture()
def tmp_dir(tmp_path: Path) -> Path:
    return tmp_path


# ============================================================================
# Basic loading
# ============================================================================


def test_loads_without_error(original_msb: MSB):
    assert original_msb is not None


# ============================================================================
# Param counts
# ============================================================================


def test_all_param_counts_positive(original_msb: MSB):
    assert len(original_msb.model_param) > 0
    assert len(original_msb.event_param) > 0
    assert len(original_msb.region_param) > 0
    assert len(original_msb.route_param) > 0
    assert len(original_msb.part_param) > 0


# ============================================================================
# Subtype distributions
# ============================================================================


def test_model_subtype_distribution(original_msb: MSB):
    counts: dict[ModelType, int] = {}
    for m in original_msb.models:
        counts[m.model_type] = counts.get(m.model_type, 0) + 1
    assert counts.get(ModelType.MapPiece, 0) > 0
    assert counts.get(ModelType.Character, 0) > 0
    assert counts.get(ModelType.Collision, 0) > 0
    assert counts.get(ModelType.Asset, 0) > 0


def test_part_subtype_distribution(original_msb: MSB):
    counts: dict[PartType, int] = {}
    for p in original_msb.parts:
        counts[p.part_type] = counts.get(p.part_type, 0) + 1
    assert counts.get(PartType.MapPiece, 0) > 0
    assert counts.get(PartType.Character, 0) > 0
    assert counts.get(PartType.Collision, 0) > 0
    assert counts.get(PartType.Asset, 0) > 0


def test_region_count(original_msb: MSB):
    assert len(original_msb.region_param) > 10


def test_at_least_one_treasure_event(original_msb: MSB):
    treasures = [e for e in original_msb.events if e.event_type == EventType.Treasure]
    assert len(treasures) > 0


# ============================================================================
# Entry names and cross-references
# ============================================================================


def test_all_entry_names_nonempty(original_msb: MSB):
    for m in original_msb.models:
        assert m.name
    for e in original_msb.events:
        assert e.name
    for r in original_msb.regions:
        assert r.name
    for p in original_msb.parts:
        assert p.name


def test_every_part_has_model(original_msb: MSB):
    for p in original_msb.parts:
        assert p.model is not None, f"Part '{p.name}' has no model"


def test_first_part_model_in_model_list(original_msb: MSB):
    parts = original_msb.parts
    models = original_msb.models
    assert len(parts) > 0
    assert parts[0].model is not None
    model_name = parts[0].model.name
    assert any(m.name == model_name for m in models)


# ============================================================================
# repr
# ============================================================================


def test_entry_repr_nonempty(original_msb: MSB):
    parts = original_msb.parts
    assert len(parts) > 0
    assert repr(parts[0])

    characters = [p for p in parts if isinstance(p, CharacterPart)]
    assert len(characters) > 0
    assert repr(characters[0])

    if characters[0].model is not None:
        assert repr(characters[0].model)


# ============================================================================
# Character part fields
# ============================================================================


def test_character_translate_finite(original_msb: MSB):
    characters = [p for p in original_msb.parts if isinstance(p, CharacterPart)]
    assert len(characters) > 0
    x, y, z = characters[0].translate
    assert math.isfinite(x) and math.isfinite(y) and math.isfinite(z)


def test_character_scale_positive(original_msb: MSB):
    characters = [p for p in original_msb.parts if isinstance(p, CharacterPart)]
    assert len(characters) > 0
    sx, sy, sz = characters[0].scale
    assert sx > 0 and sy > 0 and sz > 0


# ============================================================================
# Collision part fields
# ============================================================================


def test_collision_hit_filter(original_msb: MSB):
    collisions = [p for p in original_msb.parts if isinstance(p, CollisionPart)]
    assert len(collisions) > 0
    hf = collisions[0].hit_filter_id
    assert 8 <= hf <= 29


# ============================================================================
# Region shapes
# ============================================================================


def test_all_regions_have_shape(original_msb: MSB):
    for r in original_msb.regions:
        assert r.shape is not None, f"Region '{r.name}' has no shape"


# ============================================================================
# Draw-parent mutation
# ============================================================================


def test_draw_parent_clear():
    msb = MSB.from_path(MSB_PATH)
    characters = [p for p in msb.parts if isinstance(p, CharacterPart)]
    assert len(characters) > 0
    characters[0].draw_parent = None
    assert characters[0].draw_parent is None


def test_draw_parent_reassign():
    msb = MSB.from_path(MSB_PATH)
    parts = msb.parts
    characters = [p for p in parts if isinstance(p, CharacterPart)]
    assert len(parts) > 0 and len(characters) > 0
    characters[0].draw_parent = parts[0]
    assert characters[0].draw_parent is not None
    assert characters[0].draw_parent.name == parts[0].name


# ============================================================================
# Round-trip: write -> re-read -> compare
# ============================================================================


def _roundtrip(original: MSB, tmp_dir: Path) -> MSB:
    path = tmp_dir / "roundtrip.msb"
    original.write_to_path(path)
    return MSB.from_path(path)


def test_roundtrip_param_counts(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    assert len(original_msb.model_param) == len(reloaded.model_param)
    assert len(original_msb.event_param) == len(reloaded.event_param)
    assert len(original_msb.region_param) == len(reloaded.region_param)
    assert len(original_msb.route_param) == len(reloaded.route_param)
    assert len(original_msb.part_param) == len(reloaded.part_param)


def test_roundtrip_part_names(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig_parts = original_msb.parts
    new_parts = reloaded.parts
    assert len(orig_parts) == len(new_parts)
    for a, b in zip(orig_parts, new_parts):
        assert a.name == b.name


def test_roundtrip_model_names(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = original_msb.models
    new = reloaded.models
    assert len(orig) == len(new)
    for a, b in zip(orig, new):
        assert a.name == b.name


def test_roundtrip_event_names(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = original_msb.events
    new = reloaded.events
    assert len(orig) == len(new)
    for a, b in zip(orig, new):
        assert a.name == b.name


def test_roundtrip_region_names(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = original_msb.regions
    new = reloaded.regions
    assert len(orig) == len(new)
    for a, b in zip(orig, new):
        assert a.name == b.name


def test_roundtrip_character_fields(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig_chars = [p for p in original_msb.parts if isinstance(p, CharacterPart)]
    new_chars = [p for p in reloaded.parts if isinstance(p, CharacterPart)]
    assert len(orig_chars) == len(new_chars)

    for a, b in zip(orig_chars, new_chars):
        assert a.ai_id == b.ai_id
        assert a.character_id == b.character_id
        assert a.talk_id == b.talk_id
        assert a.entity_id == b.entity_id
        assert a.translate == b.translate

        dp_a = a.draw_parent
        dp_b = b.draw_parent
        if dp_a is not None:
            assert dp_b is not None
            assert dp_a.name == dp_b.name
        else:
            assert dp_b is None


def test_roundtrip_collision_fields(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = [p for p in original_msb.parts if isinstance(p, CollisionPart)]
    new = [p for p in reloaded.parts if isinstance(p, CollisionPart)]
    assert len(orig) == len(new)

    for a, b in zip(orig, new):
        assert a.hit_filter_id == b.hit_filter_id
        assert a.play_region_id == b.play_region_id
        assert a.entity_id == b.entity_id


def test_roundtrip_asset_fields(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = [p for p in original_msb.parts if isinstance(p, AssetPart)]
    new = [p for p in reloaded.parts if isinstance(p, AssetPart)]
    assert len(orig) == len(new)

    for a, b in zip(orig, new):
        assert a.entity_id == b.entity_id
        assert a.sfx_param_relative_id == b.sfx_param_relative_id


def test_roundtrip_region_fields(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = original_msb.regions
    new = reloaded.regions
    assert len(orig) == len(new)

    for a, b in zip(orig, new):
        assert a.entity_id == b.entity_id
        assert a.region_type == b.region_type
        assert a.shape_type == b.shape_type


def test_roundtrip_event_fields(original_msb: MSB, tmp_dir: Path):
    reloaded = _roundtrip(original_msb, tmp_dir)
    orig = original_msb.events
    new = reloaded.events
    assert len(orig) == len(new)

    for a, b in zip(orig, new):
        assert a.entity_id == b.entity_id
        assert a.event_type == b.event_type


# ============================================================================
# Stable serialization (double write)
# ============================================================================


def test_double_write_byte_identical(original_msb: MSB, tmp_dir: Path):
    path1 = tmp_dir / "stable_1.msb"
    path2 = tmp_dir / "stable_2.msb"

    original_msb.write_to_path(path1)
    assert path1.exists()

    reloaded = MSB.from_path(path1)
    reloaded.write_to_path(path2)
    assert path2.exists()

    bytes1 = path1.read_bytes()
    bytes2 = path2.read_bytes()
    assert len(bytes1) == len(bytes2)
    assert bytes1 == bytes2, (
        f"First diff at offset {next(i for i, (a, b) in enumerate(zip(bytes1, bytes2)) if a != b):#x}"
    )

