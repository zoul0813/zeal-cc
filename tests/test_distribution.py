from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import stage


class DistributionTest(unittest.TestCase):
    def test_compiler_install_and_optional_example(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dist = root / "dist/zeal-cc"
            host = root / "pages/hostfs"
            expected = {"bin/cc", "bin/cc_parse", "bin/cc_semantic", "bin/cc_codegen",
                        "bin/ast_dump", "lib/crt0.asm", "lib/zeal8bit.asm",
                        "lib/math_8.asm", "lib/math_16.asm"}
            for name in expected:
                source = root / name.replace("lib/", "runtime/")
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_text(name)
            with patch.multiple(stage, ROOT=root, DIST=dist, HOSTFS=host):
                stage.stage_dist()
                (dist / "old").touch()
                stage.stage_dist()
                self.assertEqual({p.relative_to(dist).as_posix() for p in dist.rglob("*")
                                  if p.is_file()}, expected)
                for name in expected:
                    self.assertEqual((dist / name).read_text(), name)
                (root / "runtime/crt0.asm").unlink()
                with self.assertRaises(FileNotFoundError):
                    stage.stage_dist()
                self.assertEqual((dist / "lib/crt0.asm").read_text(), "lib/crt0.asm")

                example = root / "examples/hexdump"
                example.mkdir(parents=True)
                for name in ("hexdump.c", "sample.dat"):
                    (example / name).write_text(name)
                stage.stage_hostfs()
                self.assertEqual(stage.json.loads((host / "index.json").read_text()),
                                 ["examples/hexdump/hexdump.c", "examples/hexdump/sample.dat"])

    @unittest.skipUnless(shutil.which("zdt"), "TF image integration requires zdt")
    def test_private_tf_inventory_and_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            install = work / "install"
            assembler = work / "assembler"
            assembler.write_bytes(b"assembler")
            expected = {"bin/zealasm": assembler}
            for folder, names in stage.FILES.items():
                (install / folder).mkdir(parents=True)
                for name in names:
                    source = install / folder / name
                    source.write_text(name)
                    expected[f"{folder}/{name}"] = source
            image = work / "card.img"
            with patch.dict(stage.os.environ, {"ZEALASM": str(assembler)}):
                stage.stage_tf(image, install)
            def names_at(path):
                result = subprocess.check_output(["zdt", "--json", "-i", image, "ls", path])
                return {entry["name"] for entry in stage.json.loads(result)["data"]}
            self.assertEqual(names_at("/"), {"bin", "lib"})
            for folder in ("bin", "lib"):
                self.assertEqual({f"{folder}/{name}" for name in names_at("/" + folder)},
                                 {path for path in expected if path.startswith(folder + "/")})
            for path, source in expected.items():
                target = work / Path(path).name
                stage.disk(image, "export", "/" + path, target)
                self.assertEqual(target.read_bytes(), source.read_bytes())
