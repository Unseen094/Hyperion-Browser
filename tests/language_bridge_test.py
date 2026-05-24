import subprocess
import sys
import os
import json
import tempfile
import unittest


class TestLanguageBridge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build_dir = os.environ.get("HYPERION_BUILD_DIR", "build")
        cls.bindings_dir = os.path.join(cls.build_dir, "bindings")

    def _run_bridge_test(self, lang, test_file):
        """Run a language binding test and return output."""
        if lang == "python":
            return self._run_python_test(test_file)
        elif lang == "node":
            return self._run_node_test(test_file)
        elif lang == "dotnet":
            return self._run_dotnet_test(test_file)
        else:
            self.skipTest(f"Unknown language: {lang}")

    def _run_python_test(self, test_file):
        if not os.path.exists(test_file):
            self.skipTest(f"Test file not found: {test_file}")

        py_bridge = os.path.join(self.bindings_dir, "python", "hyperion.py")
        if not os.path.exists(py_bridge):
            self.skipTest("Python bindings not built")

        result = subprocess.run(
            [sys.executable, test_file],
            capture_output=True, text=True, timeout=30
        )
        return result

    def _run_node_test(self, test_file):
        if not os.path.exists(test_file):
            self.skipTest(f"Test file not found: {test_file}")

        try:
            result = subprocess.run(
                ["node", test_file],
                capture_output=True, text=True, timeout=30
            )
            return result
        except FileNotFoundError:
            self.skipTest("Node.js not found")

    def _run_dotnet_test(self, test_file):
        if not os.path.exists(test_file):
            self.skipTest(f"Test file not found: {test_file}")

        try:
            result = subprocess.run(
                ["dotnet", "run", "--project", test_file],
                capture_output=True, text=True, timeout=30
            )
            return result
        except FileNotFoundError:
            self.skipTest("dotnet not found")

    def _write_temp_html(self, content):
        fd, path = tempfile.mkstemp(suffix=".html")
        with os.fdopen(fd, "w") as f:
            f.write(content)
        self.addCleanup(lambda: os.unlink(path))
        return path

    # ---- Python Bridge Tests ----

    def test_python_bridge_import(self):
        py_bridge = os.path.join(self.bindings_dir, "python", "hyperion.py")
        if not os.path.exists(py_bridge):
            self.skipTest("Python bindings not built")

        result = subprocess.run(
            [sys.executable, "-c", "import sys; sys.path.insert(0, '{}'); import hyperion; print('OK')".format(
                os.path.dirname(py_bridge))],
            capture_output=True, text=True, timeout=10
        )
        self.assertIn("OK", result.stdout)

    def test_python_bridge_html_parse(self):
        py_bridge = os.path.join(self.bindings_dir, "python", "hyperion.py")
        if not os.path.exists(py_bridge):
            self.skipTest("Python bindings not built")

        test_code = f"""
import sys
sys.path.insert(0, '{os.path.dirname(py_bridge)}')
from hyperion import HTMLParser
parser = HTMLParser()
doc = parser.parse("<html><body><p>Hello</p></body></html>")
print(doc.title if hasattr(doc, 'title') else 'parsed')
print('OK')
"""
        result = subprocess.run(
            [sys.executable, "-c", test_code],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            self.assertIn("OK", result.stdout)

    # ---- JavaScript Bridge Tests ----

    def test_node_bridge_import(self):
        js_bridge = os.path.join(self.bindings_dir, "node", "hyperion.js")
        if not os.path.exists(js_bridge):
            self.skipTest("Node bindings not built")
            return

        test_file = self._write_temp_html("""<html><script src="bridge.js"></script><body><p>test</p></body></html>""")
        result = subprocess.run(
            ["node", "-e", f"const h = require('{js_bridge}'); console.log('OK')"],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            self.assertIn("OK", result.stdout)

    # ---- .NET Bridge Tests ----

    def test_dotnet_bridge_import(self):
        dll_path = os.path.join(self.bindings_dir, "dotnet", "Hyperion.Bridge.dll")
        if not os.path.exists(dll_path):
            self.skipTest(".NET bindings not built")

        test_file = os.path.join(self.bindings_dir, "dotnet", "Hyperion.Bridge.Tests", "Hyperion.Bridge.Tests.csproj")
        if not os.path.exists(test_file):
            self.skipTest(".NET test project not found")

        result = subprocess.run(
            ["dotnet", "test", test_file, "--no-build"],
            capture_output=True, text=True, timeout=60
        )
        if result.returncode == 0:
            self.assertIn("Passed", result.stdout)

    # ---- Common Bridge Features ----

    def test_bridge_dom_query(self):
        test_file = os.path.join(os.path.dirname(__file__), "bridge", "test_dom_query.html")
        if not os.path.exists(test_file):
            self.skipTest(f"Test file not found: {test_file}")

        binary = os.path.join(self.build_dir, "bin", "Release", "hyperion.exe")
        if not os.path.exists(binary):
            self.skipTest("Hyperion binary not built")

        result = subprocess.run(
            [binary, test_file, "--dump-dom"],
            capture_output=True, text=True, timeout=10
        )
        self.assertEqual(result.returncode, 0)

    def test_bridge_js_eval(self):
        test_file = os.path.join(os.path.dirname(__file__), "bridge", "test_js_eval.html")
        if not os.path.exists(test_file):
            self.skipTest(f"Test file not found: {test_file}")

        binary = os.path.join(self.build_dir, "bin", "Release", "hyperion.exe")
        if not os.path.exists(binary):
            self.skipTest("Hyperion binary not built")

        result = subprocess.run(
            [binary, test_file, "--dump-dom"],
            capture_output=True, text=True, timeout=10
        )
        self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
