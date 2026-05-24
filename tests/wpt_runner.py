import subprocess
import json
import os
import sys
import tempfile
import time
import re


class WPTRunner:
    def __init__(self, browser_path: str, wpt_path: str):
        self.browser = browser_path
        self.wpt = wpt_path

    def run_test(self, test_path: str) -> dict:
        rel_path = os.path.relpath(test_path, self.wpt)
        result = {"test": rel_path, "status": "PASS", "message": ""}

        test_url = "file://" + os.path.abspath(test_path).replace("\\", "/")
        if os.path.exists(os.path.join(self.wpt, "config.json")):
            try:
                with open(os.path.join(self.wpt, "config.json")) as f:
                    cfg = json.load(f)
                if "host" in cfg and "ports" in cfg:
                    test_url = f"http://{cfg['host']}:{cfg['ports'].get('http', [8000])[0]}/{rel_path.replace(os.sep, '/')}"
            except (json.JSONDecodeError, KeyError):
                pass

        try:
            proc = subprocess.Popen(
                [self.browser, test_url, "--headless", "--no-sandbox", "--dump-dom"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=30
            )
            stdout, stderr = proc.communicate()

            if "TEST_UNEXPECTED_FAIL" in stdout or "FAIL" in stdout.upper():
                result["status"] = "FAIL"
                result["message"] = stderr[:500] if stderr else "Test failed"

            done_match = re.search(r'Harness:.*(?:done|complete)', stdout, re.I)
            if not done_match:
                result["status"] = "TIMEOUT"

        except subprocess.TimeoutExpired:
            result["status"] = "TIMEOUT"
            result["message"] = "Test timed out after 30s"
        except FileNotFoundError:
            result["status"] = "CRASH"
            result["message"] = f"Browser not found: {self.browser}"

        return result

    def run_all(self, test_dir: str) -> list:
        results = []
        total = 0
        for root, dirs, files in os.walk(test_dir):
            for f in files:
                if f.endswith(".html") or f.endswith(".htm"):
                    total += 1
                    path = os.path.join(root, f)
                    result = self.run_test(path)
                    results.append(result)
                    sys.stdout.write(f"\r  [{len(results)}/{total}] {result['test']}: {result['status']}    ")
                    sys.stdout.flush()

        print()
        return results

    def report(self, results: list):
        if not results:
            print("No tests ran.")
            return

        passed = sum(1 for r in results if r["status"] == "PASS")
        failed = sum(1 for r in results if r["status"] == "FAIL")
        timeout = sum(1 for r in results if r["status"] == "TIMEOUT")
        crashed = sum(1 for r in results if r["status"] == "CRASH")
        total = len(results)

        print(f"\n{'='*50}")
        print(f"WPT Results: {passed}/{total} passed ({passed*100//total}%)")
        print(f"  PASS:   {passed}")
        print(f"  FAIL:   {failed}")
        print(f"  TIMEOUT:{timeout}")
        print(f"  CRASH:  {crashed}")

        if failed > 0 or timeout > 0 or crashed > 0:
            print(f"\nFailed tests:")
            for r in results:
                if r["status"] != "PASS":
                    print(f"  [{r['status']}] {r['test']}")
                    if r["message"]:
                        print(f"    {r['message'][:200]}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python wpt_runner.py <browser_path> <wpt_path> [test_dir]")
        sys.exit(1)

    runner = WPTRunner(sys.argv[1], sys.argv[2])
    test_dir = sys.argv[3] if len(sys.argv) > 3 else os.path.join(sys.argv[2], "tests")
    results = runner.run_all(test_dir)
    runner.report(results)
