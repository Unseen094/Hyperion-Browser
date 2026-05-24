import subprocess
import sys
import os
import json
import time


class CIPipeline:
    STAGES = ["build", "unit", "integration", "wpt", "fuzz", "bench"]

    def __init__(self, build_dir="build"):
        self.build_dir = build_dir
        self.results = {}

    def run_stage(self, name, cmd, cwd=None):
        print(f"\n{'='*60}")
        print(f"STAGE: {name}")
        print(f"{'='*60}")

        start = time.time()
        try:
            proc = subprocess.run(
                cmd, cwd=cwd, capture_output=True, text=True, timeout=600
            )
            duration = time.time() - start
            success = proc.returncode == 0

            self.results[name] = {
                "success": success,
                "duration": duration,
                "returncode": proc.returncode,
                "stdout": proc.stdout[-2000:] if proc.stdout else "",
                "stderr": proc.stderr[-2000:] if proc.stderr else "",
            }

            print(f"Result: {'PASS' if success else 'FAIL'} ({duration:.1f}s)")
            if not success:
                print(f"Exit code: {proc.returncode}")
                if proc.stderr:
                    print(f"Stderr:\n{proc.stderr[:1000]}")
                if proc.stdout:
                    print(f"Stdout:\n{proc.stdout[:1000]}")

            return success

        except subprocess.TimeoutExpired:
            self.results[name] = {"success": False, "error": "timeout"}
            print(f"Result: TIMEOUT (>600s)")
            return False
        except FileNotFoundError as e:
            self.results[name] = {"success": False, "error": str(e)}
            print(f"Result: SKIP ({e})")
            return False

    def run(self, stages=None):
        if stages is None:
            stages = self.STAGES

        cmds = {
            "build": ["cmake", "--build", self.build_dir, "--config", "Release"],
            "unit": [os.path.join(self.build_dir, "bin", "Release", "hyperion_test.exe")],
            "integration": [os.path.join(self.build_dir, "bin", "Release", "hyperion.exe"), "--test"],
            "wpt": [sys.executable, "tests/wpt_runner.py",
                    os.path.join(self.build_dir, "bin", "Release", "hyperion.exe"), "wpt"],
            "fuzz": [os.path.join(self.build_dir, "bin", "Release", "hyperion_fuzz.exe")],
            "bench": [os.path.join(self.build_dir, "bin", "Release", "hyperion_bench.exe")],
        }

        overview = []
        fail_count = 0

        for stage in stages:
            if stage not in cmds:
                print(f"Unknown stage: {stage}")
                continue

            ok = self.run_stage(stage, cmds[stage])
            overview.append(f"  {stage}: {'PASS' if ok else 'FAIL'}")
            if not ok:
                fail_count += 1

        print(f"\n{'='*60}")
        print("PIPELINE SUMMARY")
        print(f"{'='*60}")
        for line in overview:
            print(line)
        print(f"\n{fail_count} stage(s) failed")

        return fail_count == 0


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="CI Pipeline Runner")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--stages", nargs="+", default=None)
    args = parser.parse_args()

    pipeline = CIPipeline(args.build_dir)
    success = pipeline.run(args.stages)
    sys.exit(0 if success else 1)
