"""Behavioral checks for the local pipeline; synthetic pixels are test fixtures only."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image, ImageDraw

import petdesk_pet as pet


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="petdesk-skill-test-")
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def atlas(self, version=2):
        image = Image.new("RGBA", (1536, pet.row_count(version) * 208))
        for row in range(pet.row_count(version)):
            for column in range(pet.COUNTS[row]):
                lift = (0, 12, 24, 12, 0)[column] if row == 4 else 0
                draw = ImageDraw.Draw(image)
                left, bottom = column * 192 + 70, row * 208 + 195 - lift
                draw.rectangle((left, bottom - 69, left + 49, bottom), fill=(60 + column * 20, 90 + row * 10, 130, 255))
        path = self.root / f"atlas-v{version}.png"
        image.save(path)
        return path

    def preview_args(self, atlas):
        return argparse.Namespace(atlas=atlas, output=self.root / "previews")

    def export_args(self, atlas):
        pet.preview(self.preview_args(atlas))
        template = self.root / "visual-review.json"
        pet.review_template(argparse.Namespace(atlas=atlas, output=template))
        return argparse.Namespace(atlas=atlas, id="test-pet", name="测试桌宠", description="Test fixture",
                                  review=template, previews=self.root / "previews", existing=False,
                                  validator=Path(__file__), output=self.root / "delivery")

    def fixture_review(self, args):
        # These verdicts concern synthetic unit-test shapes, never generated pet art.
        review = json.loads(args.review.read_text())
        checks = [review["appearance"], review["motion"], *review["states"].values(), *review["directions"].values()]
        for check in checks:
            check.update(verdict="pass", evidence="Synthetic rectangle fixture, for testing the export contract only.")
            if "expected" in check:
                check["observed"] = check["expected"]
        args.review.write_text(json.dumps(review), encoding="utf-8")

    def test_init_preserves_reference_and_job_dependencies(self):
        reference = self.root / "photo.jpg"
        Image.new("RGB", (32, 40), "orange").save(reference)
        output = self.root / "run"
        args = argparse.Namespace(id="new-pet", name="新宠物", description="Description", notes="Keep left ear mark",
                                  style="pixel", version=2, reference=[reference], output=output)
        pet.init_run(args)
        jobs = json.loads((output / "jobs.json").read_text())["jobs"]
        self.assertEqual(len(jobs), 13)
        self.assertEqual(jobs[-1]["depends_on"], ["base", "cardinals", "look-row-9"])
        self.assertEqual((output / "source/reference-01.jpg").read_bytes(), reference.read_bytes())
        with self.assertRaisesRegex(ValueError, "already exists"):
            pet.init_run(args)

    def test_cli_help_is_usable_for_every_command(self):
        for command in ([], ["init"], ["extract"], ["assemble"], ["validate"], ["preview"], ["review-template"], ["export"]):
            with self.subTest(command=command):
                result = subprocess.run([sys.executable, "-B", pet.__file__, *command, "--help"],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("usage:", result.stdout)

    def test_assemble_round_trip_preserves_exact_cells_and_unused_alpha(self):
        source = Image.open(self.atlas())
        frames = self.root / "frames"
        for row, state in enumerate(pet.STATES):
            (frames / state).mkdir(parents=True)
            for column in range(pet.COUNTS[row]):
                pet.frame(source, row, column).save(frames / state / f"{column:02}.png")
        output = self.root / "assembled.png"
        pet.assemble(argparse.Namespace(frames=frames, version=2, output=output))
        self.assertEqual(Image.open(output).tobytes(), source.tobytes())
        self.assertTrue(pet.atlas_report(output, creation=True)["ok"])
        (frames / "waving/04.png").write_bytes((frames / "waving/03.png").read_bytes())
        with self.assertRaisesRegex(ValueError, "expected exactly"):
            pet.assemble(argparse.Namespace(frames=frames, version=2, output=self.root / "extra.png"))

    def test_extraction_preserves_jump_height_with_shared_transform(self):
        strip = Image.new("RGBA", (950, 300))
        draw = ImageDraw.Draw(strip)
        for left, lift in zip((20, 160, 340, 570, 810), (0, 12, 24, 12, 0)):
            draw.rectangle((left, 100 - lift, left + 59, 199 - lift), fill="#bd5533")
        path = self.root / "jump.png"
        strip.save(path)
        args = argparse.Namespace(strip=path, state="jumping", boxes=None, anchor=None, reference=None,
                                  registration=None, height=100, resample="nearest", output=self.root / "jump-frames")
        pet.extract_strip(args)
        bottoms = [pet.visible(pet.read_image(args.output / f"{index:02}.png")).getbbox()[3] for index in range(5)]
        self.assertEqual(bottoms, [196, 184, 172, 184, 196])
        self.assertEqual(len(list(args.output.glob("[0-9][0-9].png"))), 5)

    def test_ambiguous_extraction_stops_and_reviewed_boxes_preserve_all_pixels(self):
        image = Image.new("RGBA", (600, 240))
        draw = ImageDraw.Draw(image)
        for index in range(4):
            left = index * 150 + 40
            draw.rectangle((left, 80, left + 30, 180), fill="black")
            draw.rectangle((left + 40, 100, left + 43, 106), fill="orange")
        path = self.root / "wave.png"
        image.save(path)
        args = argparse.Namespace(strip=path, state="waving", boxes=None, anchor=None, reference=None,
                                  registration=None, height=100, resample="nearest", output=self.root / "wave-frames")
        with self.assertRaisesRegex(ValueError, "complete poses"):
            pet.extract_strip(args)
        self.assertFalse(args.output.exists())
        boxes = self.root / "boxes.json"
        boxes.write_text(json.dumps([[index * 150, 0, (index + 1) * 150, 240] for index in range(4)]))
        args.boxes = boxes
        pet.extract_strip(args)
        self.assertEqual(len(list(args.output.glob("[0-9][0-9].png"))), 4)

    def test_validator_checks_unused_alpha_even_when_only_one(self):
        atlas = self.atlas()
        image = Image.open(atlas)
        image.putpixel((7 * 192 + 100, 100), (120, 100, 90, 1))
        image.save(atlas)
        report = pet.atlas_report(atlas)
        self.assertFalse(report["ok"])
        self.assertTrue(any("Unused cell" in error for error in report["errors"]))

    def test_validator_rejects_opaque_and_animated_files(self):
        opaque = self.root / "opaque.png"
        Image.new("RGB", (1536, 1872), "white").save(opaque)
        self.assertFalse(pet.atlas_report(opaque)["ok"])
        animated = self.root / "animated.webp"
        Image.new("RGBA", (32, 32), (255, 0, 0, 120)).save(
            animated, save_all=True, append_images=[Image.new("RGBA", (32, 32), (0, 0, 255, 120))], duration=100)
        self.assertFalse(pet.atlas_report(animated)["ok"])

    def test_edge_risk_blocks_new_art_but_keeps_existing_art_previewable(self):
        path = self.atlas()
        image = Image.open(path)
        image.putpixel((2 * 192 + 80, 4 * 208), (100, 80, 60, 255))
        image.save(path)
        self.assertTrue(pet.atlas_report(path)["ok"])
        self.assertFalse(pet.atlas_report(path, creation=True)["ok"])
        args = self.preview_args(path)
        self.assertTrue(pet.preview(args)["ok"])
        self.assertTrue((args.output / "jumping.gif").is_file())

    def test_creation_gate_rejects_static_idle_and_fake_jump(self):
        path = self.atlas()
        image = Image.open(path)
        idle = pet.frame(image, 0, 0)
        for column in range(6):
            image.paste(idle, (column * 192, 0))
        for column in range(5):
            image.paste(idle, (column * 192, 4 * 208))
        image.save(path)
        self.assertTrue(pet.atlas_report(path)["ok"])
        report = pet.atlas_report(path, creation=True)
        self.assertFalse(report["ok"])
        self.assertTrue(any("leave the idle baseline" in error for error in report["errors"]))

    def test_previews_match_native_size_half_size_and_runtime_timing(self):
        path = self.atlas()
        args = self.preview_args(path)
        pet.preview(args)
        with Image.open(args.output / "idle.gif") as gif:
            total = 0
            for index in range(gif.n_frames):
                gif.seek(index)
                total += gif.info["duration"]
            self.assertEqual(gif.size, pet.CELL)
            self.assertEqual(total, 5500)
        with Image.open(args.output / "all-states-50.gif") as gif:
            self.assertEqual(gif.size, (96, 104))
        self.assertTrue((args.output / "directions.png").is_file())
        self.assertEqual(json.loads((args.output / "manifest.json").read_text())["atlas_sha256"], pet.digest(path))

    def test_v1_stays_v1_without_invented_look_frames(self):
        path = self.atlas(version=1)
        args = self.preview_args(path)
        pet.preview(args)
        self.assertFalse((args.output / "look-loop.gif").exists())
        self.assertEqual(pet.atlas_report(path)["required_frames"], 57)

    def test_export_blocks_pending_or_stale_visual_review(self):
        args = self.export_args(self.atlas())
        with self.assertRaisesRegex(ValueError, "direction evidence"):
            pet.export(args)
        self.assertFalse(args.output.exists())
        self.fixture_review(args)
        review = json.loads(args.review.read_text())
        review["atlas_sha256"] = "0" * 64
        args.review.write_text(json.dumps(review))
        with self.assertRaisesRegex(ValueError, "different atlas bytes"):
            pet.export(args)
        self.assertFalse(args.output.exists())

    def test_failed_host_validation_does_not_publish_partial_package(self):
        args = self.export_args(self.atlas())
        self.fixture_review(args)
        failure = subprocess.CompletedProcess([], 1, stdout="", stderr="Host rejected fixture")
        with patch.object(pet.subprocess, "run", return_value=failure) as run:
            with self.assertRaisesRegex(ValueError, "Host rejected fixture"):
                pet.export(args)
            self.assertEqual(run.call_args.args[0][1], "--validate")
        self.assertFalse(args.output.exists())
        self.assertEqual(list(self.root.glob(".delivery-*")), [])

    @unittest.skipUnless(os.environ.get("PETDESK_TEST_EXECUTABLE"), "Optional real PetDesk binary not specified")
    def test_export_with_real_petdesk_keeps_atlas_bytes_and_metadata(self):
        args = self.export_args(self.atlas())
        self.fixture_review(args)
        args.validator = Path(os.environ["PETDESK_TEST_EXECUTABLE"])
        result = pet.export(args)
        self.assertTrue(result["petdesk_validated"])
        self.assertEqual((args.output / "spritesheet.png").read_bytes(), Path(args.atlas).read_bytes())
        self.assertEqual(json.loads((args.output / "pet.json").read_text())["displayName"], "测试桌宠")
        self.assertTrue(json.loads((args.output / "qa/petdesk-validation.json").read_text())["ok"])


if __name__ == "__main__":
    unittest.main()
