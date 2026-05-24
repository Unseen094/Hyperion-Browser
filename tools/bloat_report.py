import subprocess
import re
import sys
import os
import json


class BloatAnalyzer:
    def __init__(self, lib_path: str):
        self.lib_path = lib_path

    def analyze(self):
        ext = os.path.splitext(self.lib_path)[1].lower()

        if ext in (".lib", ".obj", ".exe", ".dll"):
            return self._analyze_coff()
        elif ext in (".a", ".o", ".so"):
            return self._analyze_elf()
        else:
            return self._analyze_coff()

    def _analyze_coff(self):
        data = []
        try:
            result = subprocess.run(
                ["dumpbin", "/SYMBOLS", self.lib_path],
                capture_output=True, text=True, timeout=30
            )
            for line in result.stdout.splitlines():
                m = re.search(r'^\d{3,}\s+[0-9A-F]+\s+[0-9A-F]+\s+\S+\s+\S+\s+\S+\s+(\S+)\s+\((\d+)\)', line)
                if m:
                    name = m.group(1)
                    size = int(m.group(2))
                    data.append((name, size))
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass

        if not data:
            try:
                result = subprocess.run(
                    ["llvm-nm", "--print-size", "--size-sort", self.lib_path],
                    capture_output=True, text=True, timeout=30
                )
                for line in result.stdout.splitlines():
                    parts = line.strip().split()
                    if len(parts) >= 3:
                        try:
                            size = int(parts[1], 16)
                            name = parts[2] if len(parts) > 2 else "unknown"
                            data.append((name, size))
                        except ValueError:
                            pass
            except (subprocess.TimeoutExpired, FileNotFoundError):
                pass

        return data

    def _analyze_elf(self):
        data = []
        try:
            result = subprocess.run(
                ["nm", "--print-size", "--size-sort", "--radix=d", self.lib_path],
                capture_output=True, text=True, timeout=30
            )
            for line in result.stdout.splitlines():
                parts = line.strip().split()
                if len(parts) >= 4 and parts[1] != "0":
                    try:
                        size = int(parts[1])
                        name = parts[3]
                        data.append((name, size))
                    except ValueError:
                        pass
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        return data

    def report(self):
        data = self.analyze()
        if not data:
            print("No symbol data found.")
            return

        total = sum(v for _, v in data)
        print(f"Total size: {total} bytes ({total/1024:.1f} KB)")
        print(f"Total symbols: {len(data)}")
        print()
        print(f"{'Size':>10}  {'Symbol':<50}")
        print(f"{'-'*10}  {'-'*50}")

        for name, size in sorted(data, key=lambda x: -x[1])[:20]:
            pct = size * 100.0 / total if total > 0 else 0
            print(f"{size:>8} ({pct:4.1f}%)  {name}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python bloat_report.py <path to .lib/.obj/.a/.so>")
        sys.exit(1)
    BloatAnalyzer(sys.argv[1]).report()
