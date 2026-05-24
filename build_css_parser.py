#!/usr/bin/env python3
"""
Build script for Hyperion CSS Parser (Rust)
Compiles the Rust CSS parser and prepares it for linking with C++ code.
"""

import os
import subprocess
import sys
import shutil
from pathlib import Path

def build_css_parser():
    """Build the Rust CSS parser crate."""
    print("🔨 Building Hyperion CSS Parser (Rust)...")
    
    # Get the project root directory
    script_dir = Path(__file__).parent
    css_parser_dir = script_dir / "hyperion_css_parser"
    
    if not css_parser_dir.exists():
        print(f"❌ Error: CSS parser directory not found: {css_parser_dir}")
        return False
    
    # Build the Rust crate in release mode
    try:
        print(f"📂 Working directory: {css_parser_dir}")
        result = subprocess.run(
            ["cargo", "build", "--release"],
            cwd=css_parser_dir,
            check=True,
            capture_output=True,
            text=True
        )
        print("✅ Rust build successful!")
        print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"❌ Rust build failed!")
        print(f"Error output: {e.stderr}")
        return False
    except FileNotFoundError:
        print("❌ Error: cargo not found. Make sure Rust is installed.")
        return False
    
    # Copy the built library to the engine directory
    try:
        # Determine the output filename based on platform
        if sys.platform == 'win32':
            rust_lib_name = "hyperion_css_parser.dll"
            rust_lib_import = "hyperion_css_parser.lib"
            rust_lib_static = "hyperion_css_parser.a"
        else:
            rust_lib_name = f"libhyperion_css_parser.so"
            rust_lib_import = f"libhyperion_css_parser.a"
            rust_lib_static = f"libhyperion_css_parser.a"
        
        # Source paths
        target_dir = css_parser_dir / "target" / "release"
        rust_dll_path = target_dir / rust_lib_name
        rust_lib_path = target_dir / rust_lib_import if (target_dir / rust_lib_import).exists() else target_dir / rust_lib_static
        
        # Destination paths
        engine_dir = script_dir / "engine"
        engine_lib_dir = engine_dir / "lib"
        engine_lib_dir.mkdir(exist_ok=True)
        
        dest_dll = engine_lib_dir / rust_lib_name
        dest_lib = engine_lib_dir / rust_lib_import
        
        # Copy files
        if rust_dll_path.exists():
            shutil.copy2(rust_dll_path, dest_dll)
            print(f"✅ Copied {rust_lib_name} to {dest_dll}")
        else:
            print(f"⚠️  Warning: {rust_dll_name} not found in {target_dir}")
            
        if rust_lib_path.exists():
            shutil.copy2(rust_lib_path, dest_lib)
            print(f"✅ Copied {rust_lib_import} to {dest_lib}")
        else:
            print(f"⚠️  Warning: {rust_lib_import} not found in {target_dir}")
            
    except Exception as e:
        print(f"❌ Error copying library files: {e}")
        return False
    
    print("🎉 CSS Parser build completed successfully!")
    return True

if __name__ == "__main__":
    success = build_css_parser()
    sys.exit(0 if success else 1)
