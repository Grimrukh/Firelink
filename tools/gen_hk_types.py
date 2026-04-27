#!/usr/bin/env python3
"""gen_hk_types.py

Parses soulstruct-style Havok Python type definitions and generates:
  - A C++ header (.h)  — struct declarations + forward decls
  - A C++ source (.cpp) — deserializer implementations + RegisterXxxDispatch()

Usage:
    python gen_hk_types.py <input_folder> <output_h> <output_cpp> [options]

Options:
    --reg-func NAME   Name of the registration function  (default: Register<Folder>Dispatch)
    --ns NAMESPACE    C++ namespace                       (default: Firelink::Havok)
    --tagfile-h PATH  TagFileUnpacker include path        (default: FirelinkCore/Havok/Tagfile.h)

Examples:
    python tools/gen_hk_types.py \\
        FirelinkCore/src/FirelinkCore/include/FirelinkCore/Havok/_hkai \\
        FirelinkCore/src/FirelinkCore/include/FirelinkCore/Havok/HkaiTypes.h \\
        FirelinkCore/src/FirelinkCore/src/Havok/HkaiTypes.cpp

Nested-type strategy
--------------------
Python files use flat module names (hkaiNavMeshFace) but the __real_name
field encodes the Havok type-system name ("hkaiNavMesh::Face").
The generated C++ struct keeps the *flat* Python class name as its identifier.
The dispatch table is registered under the *real* name so that TYPE section
lookups during tagfile unpacking resolve correctly.

POD vs Complex
--------------
A struct is "POD" if every member is a primitive / alias / enum type, an already-
known POD math vector/matrix, or a nested struct that is itself POD.  POD structs
can be bulk-memcpy'd from DATA.  Non-POD structs get a `Deser_Inline_*` helper
that reads each field individually.

Cross-folder references
-----------------------
Types imported from other folders (e.g. hkcdStaticAabbTree) appear as forward
declarations in the generated header.  The generated deserializer emits a
FollowObjectPtr + static_cast for refptrs to those types; you will need to link
in the corresponding generated .cpp.
"""

from __future__ import annotations

import ast
import argparse
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# Known primitive / math type table: python-name -> (cpp-type, byte-size, is-pod)
# ---------------------------------------------------------------------------

PRIMITIVE_MAP: dict[str, tuple[str, int, bool]] = {
    "hkInt8":           ("int8_t",   1,  True),
    "hkInt16":          ("int16_t",  2,  True),
    "hkInt32":          ("int32_t",  4,  True),
    "hkInt64":          ("int64_t",  8,  True),
    "hkUint8":          ("uint8_t",  1,  True),
    "hkUint16":         ("uint16_t", 2,  True),
    "hkUint32":         ("uint32_t", 4,  True),
    "hkUint64":         ("uint64_t", 8,  True),
    "hkReal":           ("float",    4,  True),
    "hkHalf16":         ("uint16_t", 2,  True),
    "hkBool":           ("bool",     1,  True),
    "hkUlong":          ("uint64_t", 8,  True),
    "_int":             ("int32_t",  4,  True),
    "_short":           ("int16_t",  2,  True),
    "_char":            ("int8_t",   1,  True),
    "_unsigned_char":   ("uint8_t",  1,  True),
    "_unsigned_short":  ("uint16_t", 2,  True),
    "_unsigned_int":    ("uint32_t", 4,  True),
    "_unsigned_long_long": ("uint64_t", 8, True),
    "_signed_char":     ("int8_t",   1,  True),
    # Math types (defined in Types.h)
    "hkVector4":        ("hkVector4",    16, True),
    "hkAabb":           ("hkAabb",       32, True),
    "hkMatrix4":        ("hkMatrix4",    64, True),
    "hkMatrix3":        ("hkMatrix3",    48, True),
    "hkRotation":       ("hkRotation",   48, True),
    "hkTransform":      ("hkTransform",  64, True),
    "hkQsTransform":    ("hkQsTransform",48, True),
    "hkQuaternion":     ("hkQuaternion", 16, True),
    # String pointer — non-POD
    "hkStringPtr":      ("std::string",  8,  False),
}

# Types whose C++ base is  HkReferencedObject
REFOBJ_BASE_NAMES = {"hkReferencedObject"}

# enum/flags storage map
STORAGE_MAP: dict[str, str] = {
    "hkUint8":  "uint8_t",  "hkUint16": "uint16_t",
    "hkUint32": "uint32_t", "hkUint64": "uint64_t",
    "hkInt8":   "int8_t",   "hkInt16":  "int16_t",
    "hkInt32":  "int32_t",
    "_unsigned_char":  "uint8_t",  "_unsigned_short": "uint16_t",
    "_unsigned_int":   "uint32_t", "_int": "int32_t",
    "_char":    "int8_t",
}


# ---------------------------------------------------------------------------
# Data model for a parsed member type expression
# ---------------------------------------------------------------------------

@dataclass
class TypeExpr:
    kind: str          # primitive | alias | enum | flags | string |
                       # array | relarray | refptr | viewptr |
                       # struct_pod | struct_complex | unknown
    cpp_type: str      # C++ field-declaration type (e.g. "std::vector<hkaiNavMeshFace>")
    inner: str         # element/target type name (for compound types)
    storage: str       # C++ storage type (for enum/flags)
    is_pod: bool       # true → field can be ReadPodAt<T>'d or bulk-copied


# ---------------------------------------------------------------------------
# Parsed member definition
# ---------------------------------------------------------------------------

@dataclass
class MemberDef:
    offset: int
    name: str
    texpr: TypeExpr
    not_serializable: bool = False


# ---------------------------------------------------------------------------
# Parsed type definition
# ---------------------------------------------------------------------------

@dataclass
class TypeDef:
    class_name: str    # Python class name == C++ struct name
    real_name:  str    # Havok dispatch name (real_name attr or class_name)
    base_class: str    # Python base class name
    alignment:  int
    byte_size:  int
    kind: str          # alias | enum | struct | refobj
    members: list[MemberDef] = field(default_factory=list)
    hsh: Optional[int] = None
    version: Optional[int] = None

    @property
    def dispatch_name(self) -> str:
        return self.real_name


# ---------------------------------------------------------------------------
# AST helpers
# ---------------------------------------------------------------------------

def _name(node) -> Optional[str]:
    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.Attribute):
        return node.attr
    return None


def _int_const(node) -> Optional[int]:
    if isinstance(node, ast.Constant) and isinstance(node.value, int):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        v = _int_const(node.operand)
        return -v if v is not None else None
    return None


def _str_const(node) -> Optional[str]:
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    return None


def _flags_contain_not_serializable(flags_node: ast.expr) -> bool:
    """Return True if the flags expression references MemberFlags.NotSerializable."""
    for n in ast.walk(flags_node):
        if isinstance(n, ast.Attribute) and n.attr == "NotSerializable":
            return True
    return False


# ---------------------------------------------------------------------------
# Type-expression parser
# ---------------------------------------------------------------------------

def _parse_texpr(
    node: ast.expr,
    local_types: dict[str, TypeDef],
    aliases: dict[str, str],
) -> TypeExpr:
    """Recursively parse a Havok type expression AST node into a TypeExpr."""

    # --- Simple names ---
    if isinstance(node, ast.Name):
        name = node.id

        if name in PRIMITIVE_MAP:
            cpp, _, is_pod = PRIMITIVE_MAP[name]
            kind = "primitive" if name != "hkStringPtr" else "string"
            return TypeExpr(kind, cpp, "", "", is_pod)

        if name in aliases:
            return TypeExpr("alias", aliases[name], name, "", True)

        if name in local_types:
            td = local_types[name]
            if td.kind in ("alias", "enum"):
                a_cpp = aliases.get(name, f"int32_t /*{name}*/")
                return TypeExpr("alias", a_cpp, name, "", True)
            is_pod = _is_pod(td, local_types)
            kind = "struct_pod" if is_pod else "struct_complex"
            return TypeExpr(kind, name, name, "", is_pod)

        # Unknown / cross-folder — treat as complex struct
        return TypeExpr("struct_complex", name, name, "", False)

    # --- Compound calls ---
    if isinstance(node, ast.Call):
        fname = _name(node.func)
        if fname is None:
            return TypeExpr("unknown", "void*", "", "", False)
        args = node.args

        if fname == "hkArray":
            if not args:
                return TypeExpr("array", "std::vector<uint8_t>", "uint8_t", "", False)
            inner_te = _parse_texpr(args[0], local_types, aliases)
            # If the inner type is a refobj (pointer-based), the vector holds unique_ptrs
            inner_td = local_types.get(inner_te.inner) if inner_te.inner else None
            if inner_td and inner_td.kind == "refobj":
                cpp_type = f"std::vector<std::shared_ptr<{inner_te.cpp_type}>>"
            else:
                cpp_type = f"std::vector<{inner_te.cpp_type}>"
            return TypeExpr("array", cpp_type, inner_te.cpp_type, "", False)

        if fname == "hkRelArray":
            if not args:
                return TypeExpr("relarray", "std::vector<uint8_t>", "uint8_t", "", False)
            inner_te = _parse_texpr(args[0], local_types, aliases)
            return TypeExpr("relarray", f"std::vector<{inner_te.cpp_type}>",
                            inner_te.cpp_type, "", False)

        if fname == "hkGenericStruct":
            # hkGenericStruct(ElementType, Count) — tightly-packed fixed-size in-place array.
            # Maps to std::array<T, N>.  Always POD.
            if len(args) < 2:
                return TypeExpr("fixed_array", "std::array<uint8_t, 1>", "uint8_t", "", True)
            inner_te = _parse_texpr(args[0], local_types, aliases)
            count_node = args[1]
            count = _int_const(count_node) or 1
            cpp = f"std::array<{inner_te.cpp_type}, {count}>"
            return TypeExpr("fixed_array", cpp, inner_te.cpp_type, "", True)

        if fname in ("hkRefPtr", "hkRefVariant", "Ptr_"):
            if not args:
                return TypeExpr("refptr", "std::shared_ptr<HkObject>", "HkObject", "", False)
            inner_te = _parse_texpr(args[0], local_types, aliases)
            return TypeExpr("refptr", f"std::shared_ptr<{inner_te.cpp_type}>",
                            inner_te.cpp_type, "", False)

        if fname == "hkViewPtr":
            if not args:
                return TypeExpr("viewptr", "HkObject*", "HkObject", "", False)
            inner_te = _parse_texpr(args[0], local_types, aliases)
            return TypeExpr("viewptr", f"{inner_te.cpp_type}*", inner_te.cpp_type, "", False)

        if fname == "hkEnum":
            storage = "int32_t"
            if len(args) >= 2:
                storage = STORAGE_MAP.get(_name(args[1]) or "", "int32_t")
            elif len(args) == 1:
                storage = STORAGE_MAP.get(_name(args[0]) or "", "int32_t")
            return TypeExpr("enum", storage, "", storage, True)

        if fname == "hkFlags":
            storage = "uint32_t"
            if args:
                storage = STORAGE_MAP.get(_name(args[0]) or "", "uint32_t")
            return TypeExpr("flags", storage, "", storage, True)

        # Fallback: unknown wrapper
        return TypeExpr("unknown", fname, fname, "", False)

    return TypeExpr("unknown", "void*", "", "", False)


# ---------------------------------------------------------------------------
# POD check (recursive)
# ---------------------------------------------------------------------------

def _is_pod(td: TypeDef, local_types: dict[str, TypeDef],
            _visited: frozenset[str] | None = None) -> bool:
    """A type is POD iff it and all its base classes have only POD members."""
    if _visited is None:
        _visited = frozenset()
    if td.class_name in _visited:
        return True  # cycle guard
    _visited = _visited | {td.class_name}

    if td.kind in ("alias", "enum"):
        return True

    # Check base class (inherited non-POD fields make this type non-POD)
    if td.base_class and td.base_class not in PRIMITIVE_MAP and td.base_class != "hk":
        base_td = local_types.get(td.base_class)
        if base_td and not _is_pod(base_td, local_types, _visited):
            return False

    for m in td.members:
        if m.not_serializable:
            continue
        te = m.texpr
        if te.kind in ("array", "relarray", "refptr", "viewptr", "string", "unknown"):
            return False
        if te.kind == "struct_complex":
            return False
        if te.kind == "fixed_array":
            pass  # always POD (primitive elements)
        if te.kind == "struct_pod":
            inner = local_types.get(te.inner)
            if inner and not _is_pod(inner, local_types, _visited):
                return False
    return True


def _collect_all_members(td: TypeDef, local_types: dict[str, TypeDef],
                         _visited: frozenset[str] | None = None) -> list[MemberDef]:
    """Return all serializable members in declaration order: base-class first, then local.
    Used by inline deserializers so that an empty-local-member derived type still
    reads the fields it inherits."""
    if _visited is None:
        _visited = frozenset()
    if td.class_name in _visited:
        return []
    _visited = _visited | {td.class_name}

    result: list[MemberDef] = []

    # Recurse into base if it is a known local struct (not a primitive alias / hkReferencedObject)
    if (td.base_class
            and td.base_class not in PRIMITIVE_MAP
            and td.base_class not in REFOBJ_BASE_NAMES
            and td.base_class != "hk"):
        base_td = local_types.get(td.base_class)
        if base_td is not None:
            result.extend(_collect_all_members(base_td, local_types, _visited))

    result.extend(td.members)
    return result


# ---------------------------------------------------------------------------
# Member tuple parser
# ---------------------------------------------------------------------------

def _parse_member(call: ast.Call, local_types: dict, aliases: dict) -> Optional[MemberDef]:
    """Parse a Member(offset, name, type_expr, flags?) AST call."""
    fname = _name(call.func)
    if fname != "Member":
        return None
    if len(call.args) < 3:
        return None

    offset = _int_const(call.args[0])
    name = _str_const(call.args[1])
    if offset is None or name is None:
        return None

    texpr = _parse_texpr(call.args[2], local_types, aliases)

    not_serial = False
    for i in range(3, len(call.args)):
        if _flags_contain_not_serializable(call.args[i]):
            not_serial = True
    for kw in call.keywords:
        if kw.arg == "flags" and _flags_contain_not_serializable(kw.value):
            not_serial = True

    return MemberDef(offset=offset, name=name, texpr=texpr, not_serializable=not_serial)


# ---------------------------------------------------------------------------
# Single Python file parser
# ---------------------------------------------------------------------------

def _target_name(node) -> Optional[str]:
    if isinstance(node, ast.Name):
        return node.id
    return None


def parse_py_file(
    path: Path,
    local_types: dict[str, TypeDef],
    aliases: dict[str, str],
) -> Optional[TypeDef]:
    """Parse one Python Havok type file and return a TypeDef (or None on failure)."""
    try:
        source = path.read_text(encoding="utf-8")
        tree = ast.parse(source)
    except Exception:
        return None

    for node in ast.walk(tree):
        if not isinstance(node, ast.ClassDef) or not node.bases:
            continue

        class_name = node.name
        base_class = _name(node.bases[0]) or "hk"

        real_name = class_name
        alignment = 8
        byte_size = 0
        hsh_val: Optional[int] = None
        ver_val: Optional[int] = None
        members: list[MemberDef] = []
        has_int_flag = False

        for stmt in node.body:
            if not isinstance(stmt, ast.Assign):
                continue
            for tgt in stmt.targets:
                tname = _target_name(tgt)
                if tname is None:
                    continue
                # Check for name-mangled dunders: __x in class body ->
                # stored as either __x (pre-compile) or _ClassName__x.
                # We match on both endings.
                clean = tname.lstrip("_")

                if tname == "alignment":
                    v = _int_const(stmt.value)
                    if v is not None:
                        alignment = v

                elif tname == "byte_size":
                    v = _int_const(stmt.value)
                    if v is not None:
                        byte_size = v

                elif clean == "real_name" or tname.endswith("__real_name"):
                    s = _str_const(stmt.value)
                    if s:
                        real_name = s

                elif clean == "hsh" or tname.endswith("__hsh"):
                    v = _int_const(stmt.value)
                    if v is not None:
                        hsh_val = v

                elif clean == "version" or tname.endswith("__version"):
                    v = _int_const(stmt.value)
                    if v is not None:
                        ver_val = v

                elif tname == "tag_type_flags":
                    # Detect integer-flag types (TagDataType.Int present in expression)
                    raw = ast.dump(stmt.value)
                    if "Int" in raw:
                        has_int_flag = True

                elif tname == "local_members":
                    val = stmt.value
                    if isinstance(val, ast.Tuple):
                        for elt in val.elts:
                            if isinstance(elt, ast.Call):
                                m = _parse_member(elt, local_types, aliases)
                                if m:
                                    members.append(m)
                    elif isinstance(val, ast.Call):
                        m = _parse_member(val, local_types, aliases)
                        if m:
                            members.append(m)

        # Classify kind
        is_primitive_ancestor = base_class in PRIMITIVE_MAP or base_class == "hk"
        if is_primitive_ancestor and not members:
            kind = "alias" if not has_int_flag else "enum"
        elif _is_refobj_base(base_class, local_types):
            kind = "refobj"
        else:
            kind = "struct"

        return TypeDef(
            class_name=class_name,
            real_name=real_name,
            base_class=base_class,
            alignment=alignment,
            byte_size=byte_size,
            kind=kind,
            members=members,
            hsh=hsh_val,
            version=ver_val,
        )

    return None


def _is_refobj_base(base: str, local_types: dict[str, TypeDef]) -> bool:
    """Check if `base` (transitively) inherits from hkReferencedObject."""
    visited: set[str] = set()
    current = base
    while current and current not in visited:
        visited.add(current)
        if current in REFOBJ_BASE_NAMES:
            return True
        td = local_types.get(current)
        if td is None:
            break
        current = td.base_class
    return False


# ---------------------------------------------------------------------------
# Topological sort
# ---------------------------------------------------------------------------

def topo_sort(types: dict[str, TypeDef]) -> list[str]:
    order: list[str] = []
    visited: set[str] = set()
    temp: set[str] = set()

    def visit(name: str):
        if name in visited or name not in types:
            return
        if name in temp:
            return  # cycle — skip, will still be added later
        temp.add(name)
        td = types[name]
        if td.base_class in types:
            visit(td.base_class)
        for m in td.members:
            if m.texpr.inner in types:
                visit(m.texpr.inner)
        temp.discard(name)
        visited.add(name)
        order.append(name)

    for name in list(types.keys()):
        visit(name)
    return order


# ---------------------------------------------------------------------------
# C++ code generation
# ---------------------------------------------------------------------------

I = "    "  # indent


def _cpp_base(td: TypeDef) -> str:
    if td.base_class == "hkReferencedObject":
        return "HkReferencedObject"
    if td.base_class in REFOBJ_BASE_NAMES:
        return "HkReferencedObject"
    if td.base_class in PRIMITIVE_MAP or td.base_class == "hk":
        return ""
    # Any other named base (local type or cross-folder forward decl) — pass through.
    return td.base_class


def _is_trivially_pod(td: TypeDef, local_types: dict[str, TypeDef]) -> bool:
    """Can we static_assert sizeof(T)?  Only if no std::vector/string/unique_ptr members."""
    if td.kind in ("alias", "enum"):
        return True
    if td.kind == "refobj":
        return False  # has vtable, std::vector etc.
    for m in td.members:
        te = m.texpr
        if te.kind in ("array", "relarray", "refptr", "viewptr", "string", "unknown"):
            return False
        if te.kind == "struct_complex":
            return False
        if te.kind == "struct_pod":
            inner = local_types.get(te.inner)
            if inner and not _is_trivially_pod(inner, local_types):
                return False
    return True


def gen_struct_decl(td: TypeDef, local_types: dict[str, TypeDef],
                    base_class_set: set[str]) -> str:
    lines: list[str] = []

    dispatch = td.dispatch_name
    comment = f'/// Havok type "{dispatch}"'
    if dispatch != td.class_name:
        comment += f"  (Python class: {td.class_name})"
    lines.append(comment)
    if td.byte_size:
        lines.append(f"/// byte_size={td.byte_size}, alignment={td.alignment}")

    # ---- alias / enum → using declaration (unless used as a base class) ----
    if td.kind in ("alias", "enum") and td.class_name not in base_class_set:
        base_cpp = PRIMITIVE_MAP.get(td.base_class, ("int32_t", 0, True))[0]
        suffix = "  // Havok enum/alias" if td.kind == "enum" else ""
        lines.append(f"using {td.class_name} = {base_cpp};{suffix}")
        lines.append("")
        return "\n".join(lines)

    # ---- alias/enum used as a base class → emit as empty struct ----
    if td.kind in ("alias", "enum") and td.class_name in base_class_set:
        lines.append(f"struct {td.class_name} {{}};  // empty base (Havok alias/enum used as base class)")
        lines.append("")
        return "\n".join(lines)

    # ---- struct / refobj ----
    base = _cpp_base(td)
    # NOTE: Not using 'alignas' because ideally it isn't needed.
    # alignas_str = f"alignas({td.alignment}) " if td.alignment > 8 else ""
    alignas_str = ""
    header = f"struct {alignas_str}{td.class_name}"
    if base:
        header += f" : {base}"
    lines.append(header)
    lines.append("{")

    if td.kind == "refobj":
        lines.append(f'{I}static constexpr std::string_view TypeName = "{dispatch}";')
        lines.append(f"{I}[[nodiscard]] std::string_view GetTypeName() const noexcept override {{ return TypeName; }}")
        lines.append("")

    for m in td.members:
        te = m.texpr
        comment = f"  // offset {m.offset}"
        if m.not_serializable:
            comment += " [not serialized]"
        default = ""
        # Give numeric/bool fields a zero-initializer
        if te.kind in ("primitive", "alias", "enum", "flags"):
            default = "{}"
        lines.append(f"{I}{te.cpp_type} {m.name}{default};{comment}")

    if _is_trivially_pod(td, local_types) and td.byte_size:
        lines.append("};")
        lines.append(f"static_assert(sizeof({td.class_name}) == {td.byte_size},")
        lines.append(f'              "Layout mismatch for {td.class_name}");')
    else:
        lines.append("};")

    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Deserializer code generation
# ---------------------------------------------------------------------------

def _deser_read_member(m: MemberDef, local_types: dict[str, TypeDef],
                       receiver: str = "r.") -> list[str]:
    """
    Generate lines that read member `m` into `receiver + m.name`.
    `receiver` is either "r." (for by-value locals) or "r->" (for unique_ptr).
    """
    te = m.texpr
    off = m.offset
    f = f"{receiver}{m.name}"
    lines: list[str] = []

    if te.kind in ("primitive", "alias", "enum", "flags"):
        lines.append(f"{I}{f} = u.ReadPodAt<{te.cpp_type}>(base + {off});")

    elif te.kind == "string":
        lines.append(f"{I}{f} = u.FollowStringPtr(u.ReadPodAt<uint64_t>(base + {off}));")

    elif te.kind == "array":
        inner = te.inner
        inner_td = local_types.get(inner)
        if inner_td is None or _is_pod(inner_td, local_types):
            # POD element → bulk copy
            lines.append(f"{I}{f} = u.ReadPodArray<{inner}>(base + {off});")
        elif inner_td.kind == "refobj":
            # Array of refobj items: each DATA entry is an 8-byte item index (pointer)
            lines.append(f"{I}{{")
            lines.append(f"{I}{I}const auto _arrIdx = static_cast<int>(u.ReadPodAt<uint64_t>(base + {off}));")
            lines.append(f"{I}{I}if (_arrIdx > 0 && _arrIdx < static_cast<int>(u.items.size())) {{")
            lines.append(f"{I}{I}{I}const auto& _arrItem = u.items[_arrIdx];")
            lines.append(f"{I}{I}{I}for (int _i = 0; _i < _arrItem.length; ++_i) {{")
            lines.append(f"{I}{I}{I}{I}const uint64_t _pIdx = u.ReadPodAt<uint64_t>(_arrItem.absoluteDataOffset + static_cast<size_t>(_i) * 8);")
            lines.append(f"{I}{I}{I}{I}auto _el = u.FollowObjectPtr(_pIdx);")
            lines.append(f"{I}{I}{I}{I}if (_el) {f}.emplace_back(std::static_pointer_cast<{inner}>(_el));")
            lines.append(f"{I}{I}{I}}}")
            lines.append(f"{I}{I}}}")
            lines.append(f"{I}}}")
        else:
            # Non-POD non-refobj element → element-by-element loop calling Deser_Inline_
            bs = inner_td.byte_size if inner_td else 0
            lines.append(f"{I}{{")
            lines.append(f"{I}{I}const auto _arrIdx = static_cast<int>(u.ReadPodAt<uint64_t>(base + {off}));")
            lines.append(f"{I}{I}if (_arrIdx > 0 && _arrIdx < static_cast<int>(u.items.size())) {{")
            lines.append(f"{I}{I}{I}const auto& _arrItem = u.items[_arrIdx];")
            lines.append(f"{I}{I}{I}{f}.resize(static_cast<size_t>(_arrItem.length));")
            lines.append(f"{I}{I}{I}for (int _i = 0; _i < _arrItem.length; ++_i)")
            lines.append(f"{I}{I}{I}{I}{f}[_i] = Deser_Inline_{inner}(u,"
                         f" _arrItem.absoluteDataOffset + static_cast<size_t>(_i) * {bs});")
            lines.append(f"{I}{I}}}")
            lines.append(f"{I}}}")

    elif te.kind == "refptr":
        inner = te.inner
        lines.append(f"{I}{{")
        lines.append(f"{I}{I}const uint64_t _idx = u.ReadPodAt<uint64_t>(base + {off});")
        lines.append(f"{I}{I}auto _obj = u.FollowObjectPtr(_idx);")
        lines.append(f"{I}{I}if (_obj) {f} = std::static_pointer_cast<{inner}>(_obj);")
        lines.append(f"{I}}}")

    elif te.kind == "viewptr":
        lines.append(f"{I}// viewptr '{m.name}' at +{off}: back-reference, skipped for now")

    elif te.kind in ("struct_pod", "struct_complex"):
        inner = te.inner
        inner_td = local_types.get(inner)
        if inner_td is None or _is_pod(inner_td, local_types):
            lines.append(f"{I}{f} = u.ReadPodAt<{inner}>(base + {off});")
        else:
            lines.append(f"{I}{f} = Deser_Inline_{inner}(u, base + {off});")

    elif te.kind == "fixed_array":
        # std::array<T, N> — always POD, bulk copy
        lines.append(f"{I}{f} = u.ReadPodAt<{te.cpp_type}>(base + {off});")

    elif te.kind == "relarray":
        # hkRelArray: 4-byte header (count uint16 + jump uint16), data follows inline
        inner = te.inner
        lines.append(f"{I}// relarray '{m.name}' at +{off}: TODO")

    else:
        lines.append(f"{I}// TODO: '{m.name}' at +{off} kind={te.kind}")

    return lines


def gen_inline_deser(td: TypeDef, local_types: dict[str, TypeDef]) -> Optional[str]:
    """
    Generate `Deser_Inline_T(u, base)` for non-POD struct types.
    Returns None if not needed (alias/enum/POD struct/refobj — the latter uses Deser_ instead).
    """
    if td.kind in ("alias", "enum", "refobj"):
        return None  # refobj types: use Deser_T(u, item) returning unique_ptr instead
    if _is_pod(td, local_types):
        return None  # POD → no inline deserializer needed

    lines: list[str] = []
    lines.append(f"static {td.class_name} Deser_Inline_{td.class_name}"
                 f"(TagFileUnpacker& u, size_t base)")
    lines.append("{")
    lines.append(f"{I}{td.class_name} r{{}};")
    all_members = _collect_all_members(td, local_types)
    for m in all_members:
        if m.not_serializable:
            continue
        lines.extend(_deser_read_member(m, local_types, "r."))
    lines.append(f"{I}return r;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def gen_refobj_deser(td: TypeDef, local_types: dict[str, TypeDef]) -> Optional[str]:
    """Generate `std::unique_ptr<HkObject> Deser_T(u, item)` for refobj types."""
    if td.kind != "refobj":
        return None

    lines: list[str] = []
    lines.append(f"std::shared_ptr<HkObject> Deser_{td.class_name}"
                 f"(TagFileUnpacker& u, const TagFileItem& item)")
    lines.append("{")
    lines.append(f"{I}auto r = std::make_shared<{td.class_name}>();")
    lines.append(f"{I}const size_t base = item.absoluteDataOffset;")
    lines.append("")
    all_members = _collect_all_members(td, local_types)
    for m in all_members:
        if m.not_serializable:
            continue
        lines.extend(_deser_read_member(m, local_types, "r->"))
    lines.append("")
    lines.append(f"{I}return r;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def gen_register_func(sorted_types: list[TypeDef], func_name: str, ns: str) -> str:
    lines: list[str] = []
    lines.append(f"void {func_name}(TagFileUnpacker& u)")
    lines.append("{")
    for td in sorted_types:
        if td.kind != "refobj":
            continue
        dn = td.dispatch_name
        fn = f"Deser_{td.class_name}"
        lines.append(f'{I}u.Register("{dn}",'
                     f" [](TagFileUnpacker& u_, const TagFileItem& i_)"
                     f" -> std::shared_ptr<HkObject>")
        lines.append(f"{I}{{")
        lines.append(f"{I}{I}return {fn}(u_, i_);")
        lines.append(f"{I}}});")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Forward-declaration collector
# ---------------------------------------------------------------------------

def collect_forward_decls(sorted_types: list[TypeDef], local_names: set[str]) -> list[str]:
    """Return forward-decl lines for types used but not defined in this folder."""
    # All C++ type names that need no forward declaration
    cpp_builtins = {
        "void", "bool", "char", "int", "float", "double",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "size_t", "ptrdiff_t",
        "hk", "hkReferencedObject", "HkObject", "HkReferencedObject", "TagFileUnpacker",
    }
    # add all Python primitive names and their C++ equivalents
    for py_name, (cpp_name, _, _) in PRIMITIVE_MAP.items():
        cpp_builtins.add(py_name)
        cpp_builtins.add(cpp_name)
    # add math / container prefixes
    math_types = {"hkVector4", "hkAabb", "hkMatrix4", "hkMatrix3", "hkRotation",
                  "hkTransform", "hkQsTransform", "hkQuaternion"}
    cpp_builtins |= math_types

    needed: set[str] = set()
    for td in sorted_types:
        if td.base_class not in local_names and td.base_class not in cpp_builtins:
            needed.add(td.base_class)
        for m in td.members:
            inner = m.texpr.inner
            if inner and inner not in local_names and inner not in cpp_builtins:
                needed.add(inner)

    result: list[str] = []
    for name in sorted(needed):
        if not name or name.startswith("void") or name.startswith("std::"):
            continue
        result.append(f"struct {name};  // cross-folder forward declaration")
    return result


# ---------------------------------------------------------------------------
# Main driver
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None):
    ap = argparse.ArgumentParser(description="Generate C++ Havok types from Python definitions")
    ap.add_argument("input_folder", help="Folder with *.py Havok type definitions")
    ap.add_argument("output_h",   help="Output C++ header (.h)")
    ap.add_argument("output_cpp", help="Output C++ source (.cpp)")
    ap.add_argument("--reg-func", default=None,
                    help="Name of the registration function (default: Register<Folder>Dispatch)")
    ap.add_argument("--ns", default="Firelink::Havok", help="C++ namespace")
    ap.add_argument("--tagfile-h", default="FirelinkCore/Havok/Tagfile.h",
                    help="TagFileUnpacker include path (used in the .cpp)")
    args = ap.parse_args(argv)

    input_dir = Path(args.input_folder)
    output_h   = Path(args.output_h)
    output_cpp = Path(args.output_cpp)

    # Derive registration function name from folder name
    reg_func = args.reg_func
    if reg_func is None:
        folder_stem = input_dir.name.lstrip("_")
        cap = "".join(w.capitalize() for w in re.split(r"[_\-]", folder_stem))
        reg_func = f"Register{cap}Dispatch"

    # ---- Pass 1: scan all files, build alias table for pass 2 ----
    py_files = sorted(f for f in input_dir.glob("*.py") if f.name != "__init__.py")

    # First, build only alias/enum info so later types can resolve those names
    local_types_pass1: dict[str, TypeDef] = {}
    aliases_pass1: dict[str, str] = {}

    for pyf in py_files:
        td = parse_py_file(pyf, local_types_pass1, aliases_pass1)
        if td:
            local_types_pass1[td.class_name] = td
            if td.kind in ("alias", "enum"):
                base_cpp = PRIMITIVE_MAP.get(td.base_class, (td.base_class, 0, True))[0]
                aliases_pass1[td.class_name] = base_cpp

    # ---- Pass 2: full parse with the alias table ----
    local_types: dict[str, TypeDef] = {}
    for pyf in py_files:
        td = parse_py_file(pyf, local_types, aliases_pass1)
        if td:
            local_types[td.class_name] = td
            if td.kind in ("alias", "enum"):
                base_cpp = PRIMITIVE_MAP.get(td.base_class, (td.base_class, 0, True))[0]
                aliases_pass1[td.class_name] = base_cpp  # accumulate

    # Fix up refobj kinds now that we have full inheritance chain
    for td in local_types.values():
        if td.kind == "struct" and _is_refobj_base(td.base_class, local_types):
            td.kind = "refobj"

    # Topo sort for definition order
    order = topo_sort(local_types)
    sorted_types = [local_types[n] for n in order if n in local_types]
    local_names = set(local_types.keys())

    # Types that are used as a C++ base class by at least one other type in this folder.
    # These must be structs (not `using` aliases) so that inheritance is legal.
    base_class_set: set[str] = {
        td.base_class for td in sorted_types if td.base_class in local_names
    }

    # ---- Generate header ----
    fwd_decls = collect_forward_decls(sorted_types, local_names)

    h_lines: list[str] = [
        "// AUTO-GENERATED by gen_hk_types.py — do not edit manually",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "#include <memory>",
        "#include <string>",
        "#include <vector>",
        "#include <FirelinkCore/Havok/Types.h>",
        "",
        f"namespace {args.ns}",
        "{",
        "",
    ]

    if fwd_decls:
        h_lines += fwd_decls
        h_lines.append("")

    for td in sorted_types:
        h_lines.append(gen_struct_decl(td, local_types, base_class_set))

    # Forward-declare the register function
    h_lines += [
        "// Registers all deserializers for this type set.",
        f"// Call after constructing TagFileUnpacker (or from TagFileUnpacker::RegisterDispatch).",
        "class TagFileUnpacker;",
        f"void {reg_func}(TagFileUnpacker& u);",
        "",
        f"}}  // namespace {args.ns}",
    ]

    output_h.parent.mkdir(parents=True, exist_ok=True)
    output_h.write_text("\n".join(h_lines), encoding="utf-8")
    print(f"[gen_hk_types] Wrote header:  {output_h}")

    # ---- Generate source ----
    cpp_lines: list[str] = [
        "// AUTO-GENERATED by gen_hk_types.py — do not edit manually",
        f"#include <FirelinkCore/Havok/{output_h.name}>",
        f"#include <{args.tagfile_h}>",
        "#include <cstring>",
        "",
        f"namespace {args.ns}",
        "{",
        "",
    ]

    for td in sorted_types:
        # Inline deserializer (for non-POD non-refobj structs used as array elements / values)
        inline_fn = gen_inline_deser(td, local_types)
        if inline_fn:
            cpp_lines.append(inline_fn)

        # Refobj deserializer
        refobj_fn = gen_refobj_deser(td, local_types)
        if refobj_fn:
            cpp_lines.append(refobj_fn)

    cpp_lines.append(gen_register_func(sorted_types, reg_func, args.ns))
    cpp_lines.append(f"}}  // namespace {args.ns}")

    output_cpp.parent.mkdir(parents=True, exist_ok=True)
    output_cpp.write_text("\n".join(cpp_lines), encoding="utf-8")
    print(f"[gen_hk_types] Wrote source:  {output_cpp}")


if __name__ == "__main__":
    main()

