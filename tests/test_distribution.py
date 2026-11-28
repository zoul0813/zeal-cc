from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import stage


class DistributionTest(unittest.TestCase):
    def test_pages_and_hardware_share_install_and_survive_missing_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dist = root / "dist/zeal-cc"
            host = root / "pages/hostfs"
            for folder, names in stage.FILES.items():
                (root / folder).mkdir(parents=True)
                for name in names:
                    (root / folder / name).write_text(name)
            assembler = root / "assembler"
            assembler.write_text("assembler")
            with patch.multiple(stage, ROOT=root, DIST=dist, HOSTFS=host), patch.dict(
                stage.os.environ, {"ZEALASM": str(assembler)}
            ):
                stage.stage_dist()
                (dist / "old").touch()
                stage.stage_dist()
                self.assertFalse((dist / "old").exists())
                stage.stage_hostfs()
                index = set(stage.json.loads((host / "index.json").read_text()))
                expected = {f"{folder}/{name}" for folder, names in stage.FILES.items() for name in names}
                self.assertEqual(index, expected | {"bin/zealasm"})
                for name in expected:
                    self.assertEqual((host / name).read_bytes(), (dist / name).read_bytes())
                assembler.unlink()
                with self.assertRaises(FileNotFoundError):
                    stage.stage_hostfs()
                self.assertEqual((host / "bin/zealasm").read_text(), "assembler")
