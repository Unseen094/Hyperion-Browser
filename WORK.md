# 🌌 Hyperion Browser Engine — Project Manifest

Hyperion is a high-performance, independent browser engine built from scratch. It is designed to be modern, memory-safe, and modular, utilizing **C++23** for the core rendering engine and **Rust** for the security-critical parsing subsystems.

---

## 🏗️ Architecture Overview

Hyperion follows a multi-process architecture inspired by modern security standards:
- **Browser Process (C++23):** Manages UI, windows, and coordination.
- **Render Process (C++23/Rust):** Handles HTML/CSS parsing and Layout.
- **Scripting Engine (C++23):** HyperionJS, a custom high-speed JavaScript VM.
- **FFI Bridge:** Robust C-ABI layer connecting Rust parsers to the C++ layout engine.

---

## 📊 Master Project Status Table

| Phase | System / Module | Primary Language | Technical Detail | Status | Progress |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1-2** | **HyperionJS VM** | C++23 | Custom Bytecode VM + Mark-Sweep GC | ✅ | 100% |
| **3** | **HTML Tokenizer** | Rust | Zero-copy, speculative parsing, UTF-8 | ✅ | 100% |
| **4** | **CSS Parser** | Rust | Complex Selectors, At-Rules, FFI Bridge | ✅ | 100% |
| **5** | **DOM Engine** | C++23 | Node/Element Tree + C++/Rust Bridge | ✅ | 100% |
| **-** | **Browser Shell** | C++23 | Win32 "Day Mode" UI, Chrome-style Tabs | ✅ | 100% |
| **6** | **Layout Engine** | C++23 | Box Model, Reflow, Flexbox logic | ✅ | 100% |
| **7** | **Paint Engine** | C++23 | DirectX Rasterization, Layer Blitting | ✅ | 100% |
| **8** | **Networking Stack** | C++23 | Custom Winsock HTTP/1.1, DNS, Chunked | ✅ | 100% |
| **9** | **IPC & Sandbox** | C++23 | Multi-process separation & IPC Pipes | 🟡 | 40% |
| **10** | **Final Integration** | N/A | Stability pass & Beta Release | 🔴 | 0% |

---

## ✅ Completed Tasks

### Phase 1-2: Scripting & Foundations
- Built **HyperionJS**, a custom JavaScript interpreter.
- Implemented core memory management and garbage collection.

### Phase 3-4: The Parsing Pipeline (Rust)
- **HTML Tokenizer:** Zero-copy tokenizer handling entities, script data, and various states.
- **CSS Parser:** Production-grade parser handling complex selectors, at-rules, and FFI bridges.

### Phase 9: Browser Shell (Host UI)
- **Day Mode Theme:** Modern light-mode palette with optimized D2D1 rendering.
- **Chrome Tab Engine:** Dynamic shrinking, rounded trapezoidal shapes, and overflow handling.
- **Component Polish:** Multi-layer toolbar, inset Omnibox, and integrated Bookmarks bar.

### Phase 5: DOM Integration
- Linked Rust HTML output to C++ DOM tree construction.
- Implemented automatic stack unwinding for malformed HTML.

### Phase 6-7: Layout & Paint (The Visual Core)
- **Layout Engine:** Full Box Model support, reflow logic, and basic Flexbox.
- **Paint Engine:** GPU-accelerated rasterization via Direct2D/DirectWrite.
- **Compositor:** Layer-based blitting for smooth scrolling and transforms.

### Phase 8: Networking Stack
- **Winsock Engine:** Custom HTTP/1.1 client with DNS and chunked encoding.
- **HTTPS Foundation:** Integration point for SChannel/TLS security.

---

## 🚀 Pending Tasks & Roadmap

### Phase 9: Multiprocess & IPC (High Priority)
- [ ] Finalize **Named Pipe** message dispatcher.
- [ ] Implement **Sandbox** restrictions (Job Objects).
- [ ] Separate Browser Host from Renderer execution.

### Phase 10: Final Integration
- [ ] Stabilize the cross-language FFI bridges.
- [ ] **Installer:** Create Windows Installer using **NSIS** (Nullsoft).
- [ ] Performance audit and beta release.

---

## 🛠️ For Developers: How to Build

### Rust Subsystems (`/hyperion_css`, `/hyperion_html`)
- Requires `rustc` (2024 edition).
- Run `cargo build` to generate the static/dynamic libraries.
- FFI headers are located in `src/ffi.rs`.

### C++ Core Engine (`/engine`)
- Requires a **C++23** compliant compiler (MSVC 19.34+ or Clang 16+).
- The project uses CMake for the build system.
- Ensure the Rust `.lib` or `.a` files are in the linker path.

---

*Last Updated: 2026-05-16*
