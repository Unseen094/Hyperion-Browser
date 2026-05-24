import subprocess
import sys
import os
import random
import string
import json
import tempfile
import time


class SecurityFuzzer:
    def __init__(self, binary_path: str, corpus_dir: str = None):
        self.binary = binary_path
        self.corpus_dir = corpus_dir or os.path.join(tempfile.gettempdir(), "hyperion_fuzz_corpus")
        os.makedirs(self.corpus_dir, exist_ok=True)
        self.crashes = []

    def _random_string(self, min_len=1, max_len=4096):
        length = random.randint(min_len, max_len)
        return ''.join(random.choice(string.printable) for _ in range(length))

    def _random_html(self):
        tags = ["div", "span", "p", "a", "script", "style", "img", "input", "form", "table"]
        attrs = ["id", "class", "style", "href", "src", "onclick", "onerror", "onload"]

        depth = random.randint(1, 5)
        parts = []
        for _ in range(random.randint(1, 10)):
            tag = random.choice(tags)
            attr_str = ""
            for _ in range(random.randint(0, 3)):
                attr = random.choice(attrs)
                val = self._random_string(1, 50)
                attr_str += f' {attr}="{val}"'

            if random.random() < 0.5:
                parts.append(f"<{tag}{attr_str}>")
                if random.random() < 0.3:
                    parts.append(self._random_string(1, 100))
                parts.append(f"</{tag}>")
            else:
                parts.append(f"<{tag}{attr_str} />")

        return "<html><body>" + "".join(parts) + "</body></html>"

    def _random_css(self):
        props = ["color", "background", "font-size", "margin", "padding", "width",
                 "height", "display", "position", "overflow", "z-index", "transform"]
        selectors = [".cls", "#id", "div", "*", "a:hover", "input:checked"]
        parts = []
        for _ in range(random.randint(1, 20)):
            sel = random.choice(selectors)
            parts.append(f"{sel} {{")
            for _ in range(random.randint(1, 5)):
                prop = random.choice(props)
                val = self._random_string(1, 50)
                parts.append(f"  {prop}: {val};")
            parts.append("}")
        return "\n".join(parts)

    def _random_url(self):
        schemes = ["http", "https", "file", "data", "javascript", "about"]
        return f"{random.choice(schemes)}://{self._random_string(1, 100)}/{self._random_string(1, 50)}"

    def _create_test_case(self, fuzz_type):
        if fuzz_type == "html":
            return self._random_html()
        elif fuzz_type == "css":
            return "<html><head><style>" + self._random_css() + "</style></head><body></body></html>"
        elif fuzz_type == "url":
            return f'<html><body><a href="{self._random_url()}">link</a><img src="{self._random_url()}" /></body></html>'
        elif fuzz_type == "script":
            return f'<html><body><script>{self._random_string(1, 2000)}</script></body></html>'
        else:
            return self._random_html()

    def run_test_case(self, content: str, test_name: str = "fuzz") -> bool:
        fd, path = tempfile.mkstemp(suffix=".html")
        try:
            with os.fdopen(fd, "w", encoding="utf-8", errors="replace") as f:
                f.write(content)

            proc = subprocess.Popen(
                [self.binary, path, "--headless", "--no-sandbox"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True, timeout=10
            )
            stdout, stderr = proc.communicate()

            if proc.returncode < 0 or proc.returncode > 1:
                crash_info = {
                    "test": test_name,
                    "returncode": proc.returncode,
                    "stderr": stderr[:500],
                    "path": path
                }
                self.crashes.append(crash_info)
                return False

            return True

        except subprocess.TimeoutExpired:
            crash_info = {"test": test_name, "error": "timeout", "path": path}
            self.crashes.append(crash_info)
            return False
        except FileNotFoundError:
            print(f"Binary not found: {self.binary}", file=sys.stderr)
            return False

    def fuzz(self, iterations=1000, fuzz_types=None):
        if fuzz_types is None:
            fuzz_types = ["html", "css", "url", "script", "mixed"]

        start = time.time()
        passed = 0
        failed = 0

        print(f"Security Fuzzer: {iterations} iterations")
        print(f"  Binary: {self.binary}")
        print(f"  Types: {', '.join(fuzz_types)}")
        print()

        for i in range(iterations):
            fuzz_type = random.choice(fuzz_types)
            if fuzz_type == "mixed":
                content = self._random_html() + "\n" + self._random_css() + "\n" + self._random_url()
            else:
                content = self._create_test_case(fuzz_type)

            ok = self.run_test_case(content, f"fuzz_{i}_{fuzz_type}")
            if ok:
                passed += 1
            else:
                failed += 1

            if (i + 1) % 100 == 0:
                elapsed = time.time() - start
                rate = (i + 1) / elapsed
                print(f"  [{i+1}/{iterations}] passed={passed} failed={failed} crashes={len(self.crashes)} ({rate:.0f} it/s)")

        duration = time.time() - start
        print(f"\n{'='*50}")
        print(f"Results: {passed} passed, {failed} failed, {len(self.crashes)} crashes")
        print(f"Duration: {duration:.1f}s")

        if self.crashes:
            print(f"\nCrashes:")
            for c in self.crashes[:10]:
                print(f"  {c['test']}: rc={c.get('returncode', '?')} {c.get('error', '')}")
                if 'path' in c:
                    print(f"    Path: {c['path']}")

        return len(self.crashes) == 0

    def run_corpus(self):
        for fname in os.listdir(self.corpus_dir):
            path = os.path.join(self.corpus_dir, fname)
            if os.path.isfile(path):
                with open(path, "rb") as f:
                    content = f.read()
                try:
                    self.run_test_case(content.decode("utf-8", errors="replace"), fname)
                except Exception:
                    pass


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Hyperion Security Fuzzer")
    parser.add_argument("binary", help="Path to hyperion.exe")
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--types", nargs="+", default=None)
    parser.add_argument("--corpus", default=None)
    args = parser.parse_args()

    fuzzer = SecurityFuzzer(args.binary, args.corpus)
    success = fuzzer.fuzz(args.iterations, args.types)
    sys.exit(0 if success else 1)
