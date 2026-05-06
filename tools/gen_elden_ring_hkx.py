from pathlib import Path
from gen_hk_types import main

REPO_ROOT = Path(__file__).parent.parent
SOULSTRUCT_DIR = REPO_ROOT.parent / "soulstruct-havok-git/src/soulstruct/havok/types/hk2018"

MODULES = {
    "hcl": ("core", "base"),
    "hka": ("core", "base"),
    "hkai": ("core", "base"),
    "hkb": ("core", "base"),
    "hkcd": ("core", "base"),
    "hknp": ("core", "base"),
    "hkp": ("core", "base"),
    "hkx": ("core", "base"),
}

def elden_ring_main():

    # Core types.
    main([
        str(SOULSTRUCT_DIR),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/include/FirelinkERHavok/Types/core.h"),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/src/Types/core.cpp"),
        "--file-glob", "core.py",
        "--reg-func", "RegisterCoreDispatch",
        "--ns", "Firelink::Havok::EldenRing",
        "--self-include", "FirelinkERHavok/Types/core.h",
    ])

    # Base types.
    main([
        str(SOULSTRUCT_DIR),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/include/FirelinkERHavok/Types/base.h"),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/src/Types/base.cpp"),
        "--file-glob", "hk*.py",  # not core, not Custom*
        "--reg-func", "RegisterBaseDispatch",
        "--dep-header", *[f"FirelinkERHavok/Types/{m}.h" for m in ("core",)],
        "--ns", "Firelink::Havok::EldenRing",
        "--self-include", "FirelinkERHavok/Types/base.h",
    ])

    # Custom types.
    main([
        str(SOULSTRUCT_DIR),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/include/FirelinkERHavok/Types/custom.h"),
        str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/src/Types/custom.cpp"),
        "--file-glob", "Custom*.py",
        "--reg-func", "RegisterCustomDispatch",
        "--dep-header", *[f"FirelinkERHavok/Types/{m}.h" for m in ("core", "base", "hkb")],
        "--ns", "Firelink::Havok::EldenRing",
        "--self-include", "FirelinkERHavok/Types/custom.h",
    ])

    for mod, deps in MODULES.items():
        main([
            str(SOULSTRUCT_DIR / f"_{mod}"),
            str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/include/FirelinkERHavok/Types/{mod}.h"),
            str(REPO_ROOT / f"FirelinkER/src/FirelinkERHavok/src/Types/{mod}.cpp"),
            "--reg-func", f"Register{mod.capitalize()}Dispatch",
            "--ns", "Firelink::Havok::EldenRing",
            "--dep-header", *[f"FirelinkERHavok/Types/{m}.h" for m in deps],
            "--self-include", f"FirelinkERHavok/Types/{mod}.h",
        ])


if __name__ == '__main__':
    elden_ring_main()
