import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


EXTRACTOR = Path(__file__).resolve().parents[1] / "tools/extract-firmware.sh"


class FirmwareTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="tsv-firmware-test-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        (self.root / "tools").mkdir()
        (self.root / "config").mkdir()
        self.script = self.root / "tools/extract-firmware.sh"
        shutil.copyfile(EXTRACTOR, self.script)
        self.vendor = self.root / "vendor"
        self.vendor.mkdir()
        self.output = self.root / "extracted"
        self.names = ("adsp.mdt", "qcom/device/a506_zap.mdt")
        self.stock109 = {name: f"stock109:{name}".encode() for name in self.names}
        self.stock084 = {name: f"stock084:{name}".encode() for name in self.names}
        for manifest, files in (
            ("firmware-CD_18781Y.210228.109.sha256", self.stock109),
            ("firmware-CD_18781Y.200628.084.sha256", self.stock084),
        ):
            (self.root / "config" / manifest).write_text("".join(
                f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                for name, data in files.items()
            ))
        shim = self.root / "tools/debugfs"
        shim.write_text(
            "#!/usr/bin/env python3\n"
            "import sys\n"
            "from pathlib import Path\n"
            "name = sys.argv[2].removeprefix('cat /firmware/')\n"
            "path = Path(sys.argv[3]) / name\n"
            "if path.is_file():\n"
            "    sys.stdout.buffer.write(path.read_bytes())\n"
        )
        shim.chmod(0o755)
        self.environment = dict(os.environ, PATH=f"{self.root / 'tools'}:{os.environ['PATH']}")

    def populate(self, files):
        for name, data in files.items():
            path = self.vendor / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def extract(self, *flags):
        return subprocess.run(
            ["bash", str(self.script), *flags, "--android-vendor",
             str(self.vendor), str(self.output)],
            text=True, capture_output=True, env=self.environment, check=False,
        )

    def test_both_complete_known_sets_are_recognized(self):
        for profile, files in (
            ("firmware-CD_18781Y.210228.109.sha256", self.stock109),
            ("firmware-CD_18781Y.200628.084.sha256", self.stock084),
        ):
            with self.subTest(profile=profile):
                self.populate(files)
                result = self.extract()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(f"checksums match {profile}", result.stdout)
                for name, data in files.items():
                    self.assertEqual((self.output / name).read_bytes(), data)
                shutil.rmtree(self.output)

    def test_mixed_set_requires_explicit_override(self):
        files = {self.names[0]: self.stock109[self.names[0]],
                 self.names[1]: self.stock084[self.names[1]]}
        self.populate(files)
        result = self.extract()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--allow-unknown", result.stderr)
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.root.glob(".tsv-firmware.*")))

        result = self.extract("--allow-unknown")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("WARNING: Accepting an unknown firmware set", result.stderr)
        self.assertNotIn("checksums match", result.stdout)
        for name, data in files.items():
            self.assertEqual((self.output / name).read_bytes(), data)
            self.assertEqual((self.vendor / name).read_bytes(), data)

    def test_override_does_not_accept_missing_or_empty_files(self):
        for missing in (True, False):
            with self.subTest(missing=missing):
                self.populate(self.stock109)
                path = self.vendor / self.names[1]
                if missing:
                    path.unlink()
                else:
                    path.write_bytes(b"")
                result = self.extract("--allow-unknown")
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.output.exists())
                self.assertFalse(list(self.root.glob(".tsv-firmware.*")))


if __name__ == "__main__":
    unittest.main()
