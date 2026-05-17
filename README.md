# Hyperion Browser

A commercial-grade native web browser written in C++ with a custom Rust HTML tokenizer and JavaScript engine.

## Version: v0.1.0 Alpha

## Features

- **Native Windows Rendering** - Built on Direct2D/Direct3D for high-performance graphics
- **Custom JavaScript Engine (HyperionJS)** - Full-featured JS runtime with bytecode VM, parser, and lexer
- **Rust HTML Tokenizer** - HTML5-compliant tokenizer written in Rust with C FFI bridge
- **Rust CSS Parser** - Production-grade CSS parsing with complex selectors
- **CSS Layout Engine** - Box model and flexbox layout support
- **Tabbed Browsing** - Multi-tab interface with toolbar, navigation, and bookmarks
- **DOM Scripting** - Execute JavaScript in pages with DOM bindings
- **Sandboxed Rendering** - Multi-process architecture with Windows Job Object sandboxing
- **Named Pipe IPC** - Fast inter-process communication between browser and renderer

## System Requirements

- Windows 10/11 (x64)
- Visual C++ Runtime (VS 2022 redistributable)

## Quick Start

1. Run `hyperion.exe`
2. Browser opens with demo content
3. Enter URLs in the toolbar (supports HTTP URLs)

## Architecture

```
hyperion/
├── apps/hyperion        - Main entry point
├── browser/             - Application, tabs, tab management
├── engine/              - Hyperion Rendering Engine (HRE)
│   ├── parser/          - HTML parser (uses Rust tokenizer)
│   ├── css/             - CSS parser
│   ├── style/           - Style engine (computed styles)
│   ├── layout/          - Layout engine (box model, flexbox)
│   ├── render/          - Painter (D2D rendering)
│   ├── script/          - Script engine (JS execution)
│   └── net/             - Network requests
├── ui/                  - UI components
│   ├── renderer/       - Direct2D renderer
│   ├── toolbar/        - Tab strip, navigation, omnibox
│   └── bookmarks/      - Bookmarks bar
├── platform/            - Platform layer
│   ├── window/         - Win32 window management
│   ├── logging/        - Logging system
│   └── ipc/            - Named pipe communication
├── HyperionJS/          - JavaScript engine
│   ├── lexer/          - Tokenizer
│   ├── parser/         - AST parser
│   ├── vm/              - Bytecode compiler & VM
│   └── runtime/         - JS objects, environment
├── hyperion_html_tokenizer/ - Rust HTML tokenizer (FFI)
└── hyperion_css/        - Rust CSS parser (FFI)
```

## Building from Source

### Prerequisites

- Windows 10/11
- Visual Studio 2022 or later
- Rust (latest stable) with cargo
- CMake 3.20+

### Build Commands

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

The executable will be at: `build/bin/Debug/hyperion.exe`

## JavaScript Capabilities

HyperionJS supports:
- Variables, functions, objects, arrays
- Arithmetic operators (+, -, *, /, %)
- Comparison operators (==, !=, <, >, <=, >=)
- Strict equality (===, !==)
- Logical operators (&&, ||, !)
- Bitwise operators (&, |, ^)
- Control flow (if/else, while, for, functions)
- Exception handling (try/catch/throw)
- Array methods (push, pop, join, reverse, slice, indexOf)
- String methods (toUpperCase, toLowerCase, trim, split, indexOf, etc.)
- Math object (floor, ceil, round, abs, sqrt, pow, max, min, random)

## Limitations (Alpha)

- No HTTPS/TLS support
- No fetch(), async/await, Promises
- No localStorage, cookies
- No CSS Grid, advanced layouts
- No WebGL, Canvas API

## Project Stats

- **Total Lines**: ~7,500+
- **C++**: ~6,000 lines
- **Rust**: ~1,500 lines

## License

Proprietary - All rights reserved

## Credits

Built with:
- C++23 (MSVC)
- Rust
- Direct2D/DirectWrite
- Windows API