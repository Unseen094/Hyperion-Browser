import subprocess
import json
import time
import os
import sys
import tempfile


class HyperionProfiler:
    def __init__(self, binary="build/bin/Debug/hyperion.exe"):
        self.binary = binary

    def start_trace(self, output="trace.json"):
        if sys.platform == "win32":
            return self._trace_etw(output)
        else:
            return self._trace_perf(output)

    def _trace_etw(self, output):
        try:
            subprocess.run(
                ["xperf", "-on", "Microsoft-Windows-Kernel-Process+Microsoft-Windows-TCPIP"],
                check=False, capture_output=True
            )
            return True
        except FileNotFoundError:
            return False

    def _trace_perf(self, output):
        pid = None
        try:
            proc = subprocess.Popen(
                ["perf", "record", "-o", output, "-g", "--", self.binary],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE
            )
            pid = proc.pid
            return True
        except FileNotFoundError:
            return False

    def stop_trace(self):
        if sys.platform == "win32":
            subprocess.run(["xperf", "-stop"], check=False, capture_output=True)
        else:
            subprocess.run(["perf", "record", "--stop"], check=False, capture_output=True)

    def generate_flamegraph(self, trace_file="trace.json", output="flamegraph.svg"):
        if sys.platform == "win32":
            folded = trace_file.replace(".json", ".folded")
            subprocess.run(
                ["python", "-m", "flamegraph", "--input", trace_file, "--output", folded],
                check=False, capture_output=True
            )
        else:
            folded = trace_file.replace(".data", ".folded")
            subprocess.run(
                ["perf", "script", "-i", trace_file, "--no-inline"],
                check=False, capture_output=True, stdout=open(folded, "w")
            )

        if os.path.exists(folded):
            subprocess.run(
                [sys.executable, "-m", "flamegraph", folded, "--output", output],
                check=False, capture_output=True
            )
        return output if os.path.exists(output) else None

    def profile_page_load(self, url, duration=10):
        print(f"Profiling page load: {url} (duration={duration}s)")

        start = time.time()
        trace_file = f"trace_{int(start)}.json"
        self.start_trace(trace_file)

        proc = subprocess.Popen(
            [self.binary, url],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        try:
            proc.wait(timeout=duration)
        except subprocess.TimeoutExpired:
            proc.kill()

        self.stop_trace()

        flamegraph = self.generate_flamegraph(trace_file)
        if flamegraph:
            print(f"Flamegraph: {flamegraph}")

        print("Done.")


if __name__ == "__main__":
    url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8080"
    HyperionProfiler().profile_page_load(url)
