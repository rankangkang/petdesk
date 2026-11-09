#!/usr/bin/env python3
"""Prepare, assemble, preview and export local PetDesk pet packages.

Requires Pillow. Image generation and visual judgments stay with the caller;
this tool never synthesizes pet artwork or declares a semantic review passed.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

try:
    from PIL import Image, ImageChops, ImageDraw
except ImportError:
    raise SystemExit("Pillow is required; install scripts/requirements.txt in a Python environment.")

CELL = (192, 208)
COUNTS = (6, 8, 8, 4, 5, 8, 6, 6, 6, 8, 8)
STATES = ("idle", "running-right", "running-left", "waving", "jumping",
          "failed", "waiting", "running", "review", "look-row-9", "look-row-10")
# Keep previews consistent with src/behavior.cpp, including PetDesk's slow idle.
TIMINGS = ((3200, 130, 160, 200, 180, 1630), (120,) * 7 + (220,),
           (120,) * 7 + (220,), (140, 140, 140, 280), (140,) * 4 + (280,),
           (140,) * 7 + (240,), (150,) * 5 + (260,), (120,) * 5 + (220,),
           (150,) * 5 + (280,), (180,) * 8, (180,) * 8)
DIRECTIONS = ("000", "022.5", "045", "067.5", "090", "112.5", "135", "157.5",
              "180", "202.5", "225", "247.5", "270", "292.5", "315", "337.5")
EXPECTED = ("up", "up-right", "up-right", "up-right", "right", "down-right",
            "down-right", "down-right", "down", "down-left", "down-left",
            "down-left", "left", "up-left", "up-left", "up-left")
PURPOSES = (
    "Calm breathing and blink; no walk, wave or new props. Frame 0 is also the resting pose.",
    "Walk facing screen-right, alternating leg/paw cadence. The host moves the window.",
    "Walk facing screen-left, alternating leg/paw cadence. Preserve asymmetric markings and props.",
    "Greeting through an attached hand, paw, wing or appendage: prepare, raise, wave, return.",
    "Anticipation, lift, peak, descent, settle. Actually leave the resting baseline; preserve size.",
    "A readable disappointed or deflated reaction, followed by recovery.",
    "An expectant asking or waiting pose, distinct from calm idle and inspection.",
    "Focused effort or thinking in place. This state means concentration, not foot-running.",
    "Inspect through eyes, head tilt or a lean; preserve existing props and identity.",
    "One coherent gaze family, clockwise: " + ", ".join(DIRECTIONS[:8]),
    "Continue the same anchored gaze family, clockwise: " + ", ".join(DIRECTIONS[8:]),
)
LIMIT = 20 * 1024 * 1024


def digest(path):
    return hashlib.sha256(Path(path).expanduser().read_bytes()).hexdigest()


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def external(path):
    """Keep private generation runs and assets out of the host's source tree."""
    path = Path(path).expanduser().resolve()
    root = next((parent for parent in Path(__file__).resolve().parents
                 if (parent / "src/petformat.cpp").is_file()), None)
    if root and (path == root or root in path.parents):
        raise ValueError("Generated assets must be outside the PetDesk code repository.")
    return path


@contextmanager
def new_directory(path):
    path = external(path)
    if path.exists():
        raise ValueError(f"Output already exists; choose a new revision: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=f".{path.name}-", dir=path.parent))
    try:
        yield stage
        if path.exists():
            raise ValueError(f"Output appeared during preparation: {path}")
        os.rename(stage, path)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def read_image(path):
    path = Path(path).expanduser()
    if not 0 < path.stat().st_size <= 64 * 1024 * 1024:
        raise ValueError(f"Image is empty or too large: {path}")
    with Image.open(path) as opened:
        if getattr(opened, "n_frames", 1) != 1:
            raise ValueError(f"Use a static image, not an animated file: {path}")
        if opened.width * opened.height > 40_000_000:
            raise ValueError(f"Image has too many pixels: {path}")
        alpha = "A" in opened.getbands() or "transparency" in opened.info
        if not alpha:
            raise ValueError(f"Image needs real transparency; regenerate with transparent background: {path}")
        image = opened.convert("RGBA")
    if image.getchannel("A").getextrema()[0] != 0:
        raise ValueError(f"No fully transparent background found: {path}")
    return image


def visible(image):
    return image.getchannel("A").point(lambda value: 255 if value > 16 else 0)


def frame(atlas, row, column):
    x, y = column * CELL[0], row * CELL[1]
    return atlas.crop((x, y, x + CELL[0], y + CELL[1]))


def row_count(version):
    return 11 if version == 2 else 9


def require_identity(pet_id, name):
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", pet_id):
        raise ValueError("id must use 1-64 lowercase letters, digits, hyphens or underscores.")
    if not name.strip() or len(name) > 100:
        raise ValueError("name must contain 1-100 characters.")


def init_run(args):
    require_identity(args.id, args.name)
    if len(args.description) > 1000:
        raise ValueError("description must not exceed 1000 characters.")
    with new_directory(args.output) as stage:
        (stage / "source").mkdir()
        (stage / "frames").mkdir()
        references = []
        for index, source in enumerate(args.reference):
            source = Path(source).expanduser().resolve()
            with Image.open(source) as opened:
                if opened.format not in ("PNG", "JPEG", "WEBP"):
                    raise ValueError(f"Unsupported reference image: {source}")
                suffix = {"PNG": ".png", "JPEG": ".jpg", "WEBP": ".webp"}[opened.format]
            relative = f"source/reference-{index + 1:02}{suffix}"
            shutil.copyfile(source, stage / relative)
            references.append(relative)
        brief = {"id": args.id, "displayName": args.name, "description": args.description,
                 "style": args.style, "notes": args.notes, "version": args.version,
                 "references": references}
        write_json(stage / "brief.json", brief)
        identity = (f"Pet: {args.name}. {args.description} Style: {args.style}. {args.notes} "
                    "Keep the reference identity, proportions, face, material, markings and props. "
                    "Transparent background. No text, grid, borders, scenery, ground shadow or detached effects. ")
        jobs = [{"id": "base", "depends_on": [], "references": references,
                 "output": "source/base.png", "prompt": identity +
                 "One readable whole-body resting character, centered with generous clear margins, suitable for 192x208 display."}]
        for index in range(row_count(args.version)):
            state = STATES[index]
            refs = ["source/base.png"] + references
            dependencies = ["base"]
            if index >= 9:
                refs += ["source/cardinals.png", "qa/standard/contact-light.png"]
                dependencies += ["cardinals"]
            if index == 10:
                refs.append("source/look-row-9.png")
                dependencies.append("look-row-9")
            jobs.append({"id": state, "depends_on": dependencies, "references": refs,
                         "output": f"source/{state}.png", "prompt": identity +
                         f"Exactly {COUNTS[index]} complete poses in ONE horizontal strip, left to right in temporal order. "
                         "Leave clear transparent gaps; same drawing scale and stable ground registration throughout. " + PURPOSES[index] +
                         (" Viewer coordinates: 000 is up, 090 screen-right, 180 down, 270 screen-left. "
                          "Keep the torso/base anchored; change gaze/head naturally, not a whole-body turntable." if index >= 9 else "")})
        if args.version == 2:
            jobs.insert(10, {"id": "cardinals", "depends_on": list(STATES[:9]),
                            "references": ["source/base.png", "qa/standard/contact-light.png"] + references,
                            "output": "source/cardinals.png", "prompt": identity +
                            "Exactly four complete poses in one horizontal strip: look up, screen-right, down, screen-left. "
                            "Use natural head/eye mechanics with torso/base anchored and unmistakable direction."})
        write_json(stage / "jobs.json", {"paths_relative_to": "run directory", "jobs": jobs})
    return {"ok": True, "run": str(external(args.output)), "jobs": len(jobs)}


def extract_strip(args):
    image = read_image(Path(args.strip).expanduser())
    mask = visible(image)
    count = 4 if args.state == "cardinals" else COUNTS[STATES.index(args.state)]
    if args.boxes:
        boxes = json.loads(Path(args.boxes).read_text(encoding="utf-8"))
    else:
        # Separate entire pose groups at actual transparent gaps, never equal slices.
        spans, start = [], None
        for x in range(image.width + 1):
            occupied = x < image.width and mask.crop((x, 0, x + 1, image.height)).getbbox() is not None
            if occupied and start is None:
                start = x
            if not occupied and start is not None:
                spans.append([start, 0, x, image.height])
                start = None
        boxes = spans
    if not isinstance(boxes, list) or len(boxes) != count:
        raise ValueError(f"{args.state} needs {count} complete poses; found {len(boxes) if isinstance(boxes, list) else 'invalid boxes'}. "
                         "Supply --boxes with reviewed [left,top,right,bottom] slots, or regenerate the strip.")
    covered = Image.new("L", image.size)
    crops, bounds = [], []
    for box in boxes:
        if (not isinstance(box, list) or len(box) != 4 or any(type(value) is not int for value in box)
                or not (0 <= box[0] < box[2] <= image.width and 0 <= box[1] < box[3] <= image.height)):
            raise ValueError("A crop box is invalid or outside the strip.")
        if covered.crop(tuple(box)).getbbox():
            raise ValueError("Crop boxes must not overlap.")
        ImageDraw.Draw(covered).rectangle((box[0], box[1], box[2] - 1, box[3] - 1), fill=255)
        crop = image.crop(tuple(box))
        bound = visible(crop).getbbox()
        if bound is None:
            raise ValueError("A crop box contains no visible pose.")
        crops.append(crop)
        bounds.append([bound[0] + box[0], bound[1] + box[1], bound[2] + box[0], bound[3] + box[1]])
    if ImageChops.subtract(mask, covered).getbbox():
        raise ValueError("Visible pixels outside crop boxes would be discarded; fix boxes or source artwork.")
    anchor = args.anchor if args.anchor is not None else (count - 1 if args.state == "jumping" else 0)
    if not 0 <= anchor < count:
        raise ValueError("anchor is a zero-based pose index within this strip.")
    if args.registration:
        registration = json.loads(Path(args.registration).read_text(encoding="utf-8"))
        if registration["source_height"] != image.height:
            raise ValueError("Look rows must share source height when reusing registration.")
        scale, offset = registration["scale"], registration["y_offset"]
    else:
        target_height, baseline = args.height, 196
        if args.reference:
            reference = read_image(args.reference)
            if reference.size != CELL:
                raise ValueError("reference must be an approved 192x208 resting frame.")
            ref_bound = visible(reference).getbbox()
            if not ref_bound:
                raise ValueError("reference is empty.")
            target_height, baseline = ref_bound[3] - ref_bound[1], ref_bound[3]
        scale = target_height / (bounds[anchor][3] - bounds[anchor][1])
        if not args.reference:
            scale = min(scale, 168 / max(box[2] - box[0] for box in bounds))
        offset = baseline - round(bounds[anchor][3] * scale)
        registration = {"source_height": image.height, "scale": scale, "y_offset": offset,
                        "anchor": anchor, "target_baseline": baseline, "resample": args.resample}
    if not math.isfinite(scale) or scale <= 0 or not math.isfinite(offset):
        raise ValueError("Invalid registration transform.")
    mode = Image.Resampling.NEAREST if registration["resample"] == "nearest" else Image.Resampling.LANCZOS
    normalized = []
    for crop, box, bound in zip(crops, boxes, bounds):
        # One scale and vertical transform for the whole row preserve jump displacement.
        width, height = max(1, round(crop.width * scale)), max(1, round(crop.height * scale))
        x, y = (CELL[0] - width) // 2, round(box[1] * scale + offset)
        left = x + (bound[0] - box[0]) * scale
        right = x + (bound[2] - box[0]) * scale
        top, bottom = bound[1] * scale + offset, bound[3] * scale + offset
        if left < 2 or right > CELL[0] - 2 or top < 2 or bottom > CELL[1] - 2:
            raise ValueError("A pose would clip. Repair source registration or lower the common idle height, then re-extract all affected rows.")
        cell = Image.new("RGBA", CELL)
        cell.alpha_composite(crop.resize((width, height), mode), (x, y))
        normalized.append(cell)
    with new_directory(args.output) as stage:
        for index, cell in enumerate(normalized):
            cell.save(stage / f"{index:02}.png")
        registration.update({"state": args.state, "boxes": boxes, "source_sha256": digest(args.strip)})
        write_json(stage / "registration.json", registration)
        render_gif(normalized, [180] * count, stage / "preview.gif")
        sheet = Image.new("RGB", (CELL[0] * count, CELL[1]), "#f6f8fa")
        for index, cell in enumerate(normalized):
            sheet.paste(cell, (CELL[0] * index, 0), cell)
        sheet.save(stage / "contact-sheet.png")
    return {"ok": True, "state": args.state, "frames": count, "output": str(external(args.output))}


def assemble(args):
    rows = row_count(args.version)
    atlas = Image.new("RGBA", (1536, rows * CELL[1]))
    for row in range(rows):
        folder = Path(args.frames) / STATES[row]
        expected = {f"{index:02}.png" for index in range(COUNTS[row])}
        actual = {path.name for path in folder.glob("[0-9][0-9].png")}
        if actual != expected:
            raise ValueError(f"{STATES[row]}: expected exactly {sorted(expected)}; found {sorted(actual)}")
        for column in range(COUNTS[row]):
            cell = read_image(folder / f"{column:02}.png")
            if cell.size != CELL:
                raise ValueError(f"{STATES[row]}/{column:02}: frames must already be 192x208; no per-frame resizing.")
            atlas.paste(cell, (column * CELL[0], row * CELL[1]))
    output = external(args.output)
    if output.suffix.lower() != ".png" or output.exists():
        raise ValueError("Choose a new .png atlas output.")
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(output, format="PNG")
    return {"ok": True, "atlas": str(output), "sha256": digest(output)}


def atlas_report(path, creation=False):
    path = Path(path).expanduser().resolve()
    errors, warnings, cells, metrics = [], [], [], {}
    report = {"ok": False, "sha256": digest(path), "errors": errors, "warnings": warnings,
              "cells": cells, "metrics": metrics, "creation_checks": creation}
    if not 0 < path.stat().st_size <= LIMIT:
        errors.append("Atlas size must be 1 byte to 20 MiB.")
        return report
    try:
        with Image.open(path) as source:
            format_name = source.format
        atlas = read_image(path)
    except (ValueError, OSError) as error:
        errors.append(str(error))
        return report
    if format_name not in ("PNG", "WEBP") or path.suffix.lower() != "." + format_name.lower():
        errors.append("Atlas encoding and extension must agree: .png or .webp.")
    version = { (1536, 1872): 1, (1536, 2288): 2 }.get(atlas.size)
    if version is None:
        errors.append(f"Expected 1536x1872 (v1) or 1536x2288 (v2); got {atlas.size}.")
        return report
    report.update({"version": version, "format": format_name.lower(), "width": atlas.width,
                   "height": atlas.height, "required_frames": sum(COUNTS[:row_count(version)])})
    bounds_by_row = []
    quality_issues = []
    for row in range(row_count(version)):
        hashes, bounds = set(), []
        for column in range(8):
            cell = frame(atlas, row, column)
            alpha = cell.getchannel("A")
            nonzero = sum(alpha.histogram()[1:])
            bound = visible(cell).getbbox()
            if column >= COUNTS[row]:
                if nonzero:
                    errors.append(f"Unused cell row={row} frame={column} must be fully transparent.")
                continue
            pixels = sum(visible(cell).histogram()[1:])
            cells.append({"row": row, "frame": column, "pixels": pixels, "bounds": bound})
            bounds.append(bound)
            hashes.add(hashlib.sha256(cell.tobytes()).hexdigest())
            if pixels < 50:
                errors.append(f"Missing or tiny pose row={row} frame={column}.")
            if bound and (bound[0] < 1 or bound[1] < 1 or bound[2] > CELL[0] - 1 or bound[3] > CELL[1] - 1):
                quality_issues.append(f"Pose touches cell edge row={row} frame={column}; check clipping or bleed.")
            if nonzero > CELL[0] * CELL[1] * .95:
                errors.append(f"Near-opaque background in row={row} frame={column}.")
        bounds_by_row.append(bounds)
        metrics[STATES[row]] = {"distinct_frames": len(hashes)}
        if row < 9 and len(hashes) < 2:
            quality_issues.append(f"{STATES[row]} is stationary; repeated pixels do not provide the required animation.")
    if bounds_by_row[0][0] and all(bounds_by_row[4]):
        baseline = bounds_by_row[0][0][3]
        bottoms = [box[3] for box in bounds_by_row[4]]
        metrics["jumping"].update({"idle_baseline": baseline, "frame_bottoms": bottoms})
        if baseline - min(bottoms) < 4:
            quality_issues.append("jumping does not visibly leave the idle baseline.")
        if abs(bottoms[-1] - baseline) > 8:
            quality_issues.append("jumping does not settle near the idle baseline.")
    warnings.extend(quality_issues)
    if creation:
        errors.extend(quality_issues)
    report["ok"] = not errors
    return report


def validate(args):
    report = atlas_report(args.atlas, args.creation)
    if args.output:
        write_json(external(args.output), report)
    return report


def render_gif(frames, durations, output, scale=1):
    panels = []
    for cell in frames:
        if scale != 1:
            cell = cell.resize((round(CELL[0] * scale), round(CELL[1] * scale)), Image.Resampling.NEAREST)
        panel = Image.new("RGB", cell.size, "#f6f8fa")
        panel.paste(cell, (0, 0), cell)
        panels.append(panel)
    panels[0].save(output, format="GIF", save_all=True, append_images=panels[1:],
                   duration=durations, loop=0, disposal=2, optimize=False)


def preview(args):
    report = atlas_report(args.atlas)
    if not report["ok"]:
        return report
    atlas = read_image(args.atlas)
    with new_directory(args.output) as stage:
        for theme, background, ink in (("light", "#f6f8fa", "#24292f"), ("dark", "#24292f", "#f6f8fa")):
            sheet = Image.new("RGB", (1536, 236 * row_count(report["version"])), background)
            draw = ImageDraw.Draw(sheet)
            for row in range(row_count(report["version"])):
                for column in range(COUNTS[row]):
                    label = DIRECTIONS[(row - 9) * 8 + column] if row >= 9 else f"{STATES[row]} {column}"
                    draw.text((column * 192 + 4, row * 236 + 4), label, fill=ink)
                    cell = frame(atlas, row, column)
                    sheet.paste(cell, (column * 192, row * 236 + 24), cell)
            sheet.save(stage / f"contact-{theme}.png")
        all_frames, durations, rows = [], [], []
        for row in range(9):
            cells = [frame(atlas, row, column) for column in range(COUNTS[row])]
            rows.append(cells)
            render_gif(cells, list(TIMINGS[row]), stage / f"{STATES[row]}.gif")
            all_frames.extend(cells)
            durations.extend(TIMINGS[row])
        render_gif(all_frames, durations, stage / "all-states.gif")
        render_gif(all_frames, durations, stage / "all-states-50.gif", .5)
        render_gif(rows[0] + rows[4] + rows[0], list(TIMINGS[0] + TIMINGS[4] + TIMINGS[0]), stage / "idle-jump-idle.gif")
        if report["version"] == 2:
            looks = [frame(atlas, 9 + index // 8, index % 8) for index in range(16)]
            render_gif(looks, [180] * 16, stage / "look-loop.gif")
            sheet = Image.new("RGB", (5 * 192, 4 * 236), "#f6f8fa")
            draw = ImageDraw.Draw(sheet)
            for index in range(20):
                x, y = index % 5 * 192, index // 5 * 236
                cell = frame(atlas, 0, 0) if index % 5 == 0 else looks[index - index // 5 - 1]
                label = "neutral" if index % 5 == 0 else DIRECTIONS[index - index // 5 - 1]
                draw.text((x + 4, y + 4), label, fill="#24292f")
                sheet.paste(cell, (x, y + 24), cell)
            sheet.save(stage / "directions.png")
        write_json(stage / "manifest.json", {"atlas_sha256": report["sha256"], "version": report["version"],
                                            "timings_source": "src/behavior.cpp", "idle_cycle_ms": sum(TIMINGS[0]),
                                            "files": sorted(path.name for path in stage.iterdir())})
    return {"ok": True, "previews": str(external(args.output)), "atlas_sha256": report["sha256"]}


def review_template(args):
    report = atlas_report(args.atlas)
    if not report["ok"]:
        return report
    pending = lambda: {"verdict": "pending", "evidence": ""}
    review = {"atlas_sha256": report["sha256"], "appearance": pending(), "motion": pending(),
              "states": {state: pending() for state in STATES[:9]}, "directions": {}}
    if report["version"] == 2:
        review["directions"] = {label: {"expected": expected, "observed": "", **pending()}
                                for label, expected in zip(DIRECTIONS, EXPECTED)}
    write_json(external(args.output), review)
    return {"ok": True, "review_template": str(external(args.output)), "completed": False}


def verify_review(review, report):
    if review.get("atlas_sha256") != report["sha256"]:
        raise ValueError("Visual review is for different atlas bytes; inspect the final previews again.")
    checks = [review.get("appearance"), review.get("motion")]
    checks += [review.get("states", {}).get(state) for state in STATES[:9]]
    if report["version"] == 2:
        for label, expected in zip(DIRECTIONS, EXPECTED):
            check = review.get("directions", {}).get(label)
            if not isinstance(check, dict) or check.get("expected") != expected or not check.get("observed", "").strip():
                raise ValueError(f"Missing direction evidence for {label}.")
            checks.append(check)
    for check in checks:
        if not isinstance(check, dict) or check.get("verdict") != "pass" or not check.get("evidence", "").strip():
            raise ValueError("Every visual check needs an actual pass verdict and concrete observed evidence.")


def export(args):
    require_identity(args.id, args.name)
    if len(args.description) > 1000:
        raise ValueError("description must not exceed 1000 characters.")
    atlas_path = Path(args.atlas).expanduser().resolve()
    report = atlas_report(atlas_path, creation=not args.existing)
    if not report["ok"]:
        return report
    review = json.loads(Path(args.review).read_text(encoding="utf-8"))
    verify_review(review, report)
    previews = Path(args.previews).expanduser().resolve()
    preview_manifest = json.loads((previews / "manifest.json").read_text(encoding="utf-8"))
    if preview_manifest.get("atlas_sha256") != report["sha256"]:
        raise ValueError("Previews are for different atlas bytes; render them again.")
    validator = Path(args.validator).expanduser().resolve()
    if not validator.is_file():
        raise ValueError("Pass --validator with the local PetDesk executable, not its .app directory.")
    with new_directory(args.output) as stage:
        asset = f"spritesheet.{report['format']}"
        shutil.copyfile(atlas_path, stage / asset)
        if digest(stage / asset) != report["sha256"]:
            raise ValueError("Atlas bytes changed during export; validate again.")
        write_json(stage / "pet.json", {"id": args.id, "displayName": args.name, "description": args.description,
                                        "spritesheetPath": asset, "spriteVersionNumber": report["version"],
                                        "sha256": report["sha256"]})
        frame(read_image(stage / asset), 0, 0).save(stage / "preview.png")
        write_json(stage / "qa/atlas-validation.json", report)
        write_json(stage / "qa/visual-review.json", review)
        (stage / "previews").mkdir()
        for filename in preview_manifest["files"] + ["manifest.json"]:
            source = previews / filename
            if Path(filename).name != filename or not source.is_file() or source.is_symlink():
                raise ValueError("Preview manifest must refer to local regular files.")
            shutil.copyfile(source, stage / "previews" / filename)
        result = subprocess.run([str(validator), "--validate", str(stage)], capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=30)
        if result.returncode:
            raise ValueError(f"PetDesk rejected the exported package: {result.stderr.strip()}")
        host_report = json.loads(result.stdout)
        if (host_report.get("ok") is not True or host_report.get("sha256") != report["sha256"]
                or host_report.get("version") != report["version"] or host_report.get("name") != args.name):
            raise ValueError("PetDesk validation did not confirm this package's name, version and exact atlas bytes.")
        # The validator sees staging; report the delivered relative asset instead.
        host_report["atlasPath"] = asset
        write_json(stage / "qa/petdesk-validation.json", host_report)
    return {"ok": True, "directory": str(external(args.output)), "name": args.name,
            "version": report["version"], "sha256": report["sha256"], "petdesk_validated": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init", help="Create an external run with grounded image-generation jobs.")
    for flag in ("id", "name", "output"):
        init.add_argument(f"--{flag}", required=True)
    init.add_argument("--description", default="")
    init.add_argument("--style", default="infer from the supplied reference")
    init.add_argument("--notes", default="")
    init.add_argument("--reference", action="append", default=[])
    init.add_argument("--version", type=int, choices=(1, 2), default=2)
    init.set_defaults(run=init_run)
    extract = commands.add_parser("extract", help="Extract complete transparent poses with a shared row transform.")
    extract.add_argument("--strip", required=True)
    extract.add_argument("--state", choices=STATES + ("cardinals",), required=True)
    extract.add_argument("--output", required=True)
    extract.add_argument("--boxes", help="JSON list of reviewed crop slots in source pixel coordinates.")
    extract.add_argument("--anchor", type=int, help="Zero-based neutral pose; defaults to last for jumping, first otherwise.")
    extract.add_argument("--height", type=int, default=160, choices=range(32, 185), metavar="32..184")
    transform = extract.add_mutually_exclusive_group()
    transform.add_argument("--reference", help="Approved 192x208 idle frame for common neutral size and baseline.")
    transform.add_argument("--registration", help="Reuse the approved look-row-9 registration for look-row-10.")
    extract.add_argument("--resample", choices=("smooth", "nearest"), default="smooth")
    extract.set_defaults(run=extract_strip)
    compose = commands.add_parser("assemble", help="Assemble registered frames without inventing or resizing cells.")
    compose.add_argument("--frames", required=True)
    compose.add_argument("--version", type=int, choices=(1, 2), default=2)
    compose.add_argument("--output", required=True)
    compose.set_defaults(run=assemble)
    valid = commands.add_parser("validate", help="Check geometry, alpha, occupancy and motion metrics.")
    valid.add_argument("--atlas", required=True)
    valid.add_argument("--creation", action="store_true", help="Also reject edge contact, stationary animation rows and absent jumps.")
    valid.add_argument("--output")
    valid.set_defaults(run=validate)
    previews = commands.add_parser("preview", help="Render contact sheets, nine motions, half-size playback and gaze.")
    previews.add_argument("--atlas", required=True)
    previews.add_argument("--output", required=True)
    previews.set_defaults(run=preview)
    template = commands.add_parser("review-template", help="Create pending checks bound to the exact atlas bytes.")
    template.add_argument("--atlas", required=True)
    template.add_argument("--output", required=True)
    template.set_defaults(run=review_template)
    package = commands.add_parser("export", help="Publish a new package after visual and real PetDesk validation.")
    for flag in ("atlas", "id", "name", "review", "previews", "validator", "output"):
        package.add_argument(f"--{flag}", required=True)
    package.add_argument("--description", default="")
    package.add_argument("--existing", action="store_true", help="Preserve an existing pet; report motion issues without imposing new-art gates.")
    package.set_defaults(run=export)
    args = parser.parse_args()
    try:
        result = args.run(args)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(json.dumps({"ok": False, "error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
