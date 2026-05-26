# rename_zstack.py
# Renames z-stack tif files from *_*_*.tif format to *_*.tif
# Original format: <part1>_<z_value>_<part2>.tif
# New format:      <part1><part2>_<increment>.tif
# Example: ABC_003_XYZ.tif -> ABCXYZ_003.tif

import os
import re
import sys
from pathlib import Path
from collections import defaultdict


def rename_zstack_files(directory: str, dry_run: bool = True):
    """
    Renames files from <part1>_<z>_<part2>.tif to <part1><part2>_<increment>.tif

    Args:
        directory: path to folder containing the tif files
        dry_run:   if True, just prints what would happen without renaming
    """

    dir_path = Path(directory)

    if not dir_path.exists() or not dir_path.is_dir():
        print(f"[ERROR] Invalid directory: {directory}")
        sys.exit(1)

    pattern = re.compile(r'^(.+)_(\d+)_(.+)\.tif$', re.IGNORECASE)

    # Group files by their combined identifier (part1 + part2)
    groups = defaultdict(list)

    for f in dir_path.iterdir():
        if not f.is_file():
            continue
        match = pattern.match(f.name)
        if match:
            part1  = match.group(1)
            z_val  = match.group(2)
            part2  = match.group(3)
            identifier = part1 + part2
            groups[identifier].append((int(z_val), f))
        else:
            print(f"[SKIP] Does not match expected pattern: {f.name}")

    if not groups:
        print("[ERROR] No matching files found.")
        print("  Expected format: <part1>_<z_value>_<part2>.tif")
        return

    print(f"[INFO] Found {sum(len(v) for v in groups.values())} file(s) across {len(groups)} group(s)")
    if dry_run:
        print("[INFO] DRY RUN — no files will be renamed. Pass --apply to rename.\n")

    for identifier, files in sorted(groups.items()):
        # Sort by z value to assign increments in correct order
        files.sort(key=lambda x: x[0])

        print(f"  Group: '{identifier}' ({len(files)} slices)")

        for increment, (z_val, old_path) in enumerate(files):
            new_name = f"{identifier}_{increment:03d}.tif"
            new_path = dir_path / new_name

            print(f"    {old_path.name}  ->  {new_name}")

            if not dry_run:
                if new_path.exists() and new_path != old_path:
                    print(f"    [WARNING] Target already exists, skipping: {new_name}")
                    continue
                old_path.rename(new_path)

    if not dry_run:
        print("\n[DONE] Files renamed successfully.")
    else:
        print("\n[DONE] Dry run complete. Run with dry_run=False to apply changes.")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Rename z-stack tif files from <p1>_<z>_<p2>.tif to <p1p2>_<increment>.tif"
    )
    parser.add_argument("directory", help="Path to folder containing tif files")
    parser.add_argument("--apply", action="store_true", help="Actually rename files (default is dry run)")

    args = parser.parse_args()
    rename_zstack_files(args.directory, dry_run=not args.apply)