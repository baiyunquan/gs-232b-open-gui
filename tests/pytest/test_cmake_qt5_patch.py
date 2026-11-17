"""Exercise the Qt 6 to Qt 5 patch against a tiny CMake fixture."""

import pathlib
import subprocess
import tempfile
import unittest


PATCH_SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "cmake/PatchAnytermQt5.cmake"


class Qt5PatchTests(unittest.TestCase):
    def test_patch_converts_qtpyte_cmake_and_mouse_events(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            qtpyte = root / "qtpyte"
            source = qtpyte / "src/selection.cpp"
            source.parent.mkdir(parents=True)
            (qtpyte / "CMakeLists.txt").write_text(
                "find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)\n"
                "target_link_libraries(example Qt6::Widgets)\n",
                encoding="utf-8",
            )
            source.write_text(
                "\n".join(["event->position().toPoint();"] * 3) + "\n",
                encoding="utf-8",
            )

            result = subprocess.run(
                [
                    "cmake",
                    "-DQT_VERSION_MAJOR=5",
                    f"-Danytermqt_SOURCE_DIR={root.as_posix()}",
                    "-P",
                    PATCH_SCRIPT.as_posix(),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            patched_cmake = (qtpyte / "CMakeLists.txt").read_text(encoding="utf-8")
            patched_source = source.read_text(encoding="utf-8")
            self.assertIn("find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)", patched_cmake)
            self.assertIn("Qt5::Widgets", patched_cmake)
            self.assertEqual(patched_source.count("event->pos()"), 3)
            self.assertNotIn("event->position()", patched_source)


if __name__ == "__main__":
    unittest.main()
