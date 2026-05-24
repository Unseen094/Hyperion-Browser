# Hyperion Browser: Chrome Extension Compatibility Roadmap

## Executive Summary

This document outlines the plan to enable Chrome extensions in Hyperion, allowing users to install and use most Chrome Web Store extensions directly in Hyperion.

**Target:** Achieve 90%+ compatibility with Chrome extensions

---

## Current Implementation (What's Done)

| Component | Status | Language |
|-----------|--------|----------|
| Manifest V3 Parser | Basic | C++ |
| Extension Loader | Basic | C++ |
| Extension Manager | Skeleton | C++ |
| Chrome API Stub | Placeholder | C++ |

**What's Working:** Can parse manifest.json, load extension folders, basic enable/disable

---

## What's Needed for Chrome Extension Support

### 1. Complete Chrome API Implementation (Critical)

We need to implement these Chrome APIs (in priority order):

#### Tier 1: Essential APIs
| API | Description | Language |
|-----|-------------|-----------|
| chrome.runtime | Extension lifecycle, message passing | C++ |
| chrome.storage | Local/sync storage for extensions | C++ |
| chrome.tabs | Tab management, querying | C++ |
| chrome.windows | Window management | C++ |

#### Tier 2: Common APIs
| API | Description | Language |
|-----|-------------|-----------|
| chrome.bookmarks | Bookmark management | C++ |
| chrome.history | Browsing history | C++ |
| chrome.downloads | Download management | C++ |
| chrome.contextMenus | Context menu items | C++ |
| chrome.alarms | Scheduled tasks | C++ |
| chrome.notifications | Desktop notifications | C |

#### Tier 3: Advanced APIs
| API | Description | Language |
|-----|-------------|-----------|
| chrome.webRequest | Network request modification | C++ |
| chrome.proxy | Proxy settings | C++ |
| chrome.identity | OAuth2 support | C++ |
| chrome.gcm | Google Cloud Messaging | C++ |

---

### 2. Service Worker Support (Critical)

Manifest V3 uses service workers instead of background pages.

| Task | Language | Description |
|------|----------|-------------|
| Service Worker Runtime | C++ | Implement chrome.runtime API in service worker context |
| Event Handling | C++ | Background sync, alarm, push events |
| Lifecycle Management | C++ | Install, update, activate states |
| Message Routing | C++ | Connect service worker to extensions |

---

### 3. Content Script Injection

| Task | Language | Description |
|------|----------|-------------|
| Script Injector | C++ | Inject scripts into web pages |
| World Isolation | C++ | Separate isolated world for extensions |
| CSS Injection | C++ | Inject user stylesheets |
| Match Patterns | C++ | URL pattern matching for injection |

---

### 4. Message Passing System

| Component | Language | Description |
|-----------|----------|-------------|
| Native Messaging | C++ | Communication between extension components |
| Port Management | C++ | Long-lived message channels |
| External Messaging | C++ | Native messaging host (for extensions to communicate with OS) |

---

### 5. Extension UI Components

| Component | Language | Description |
|-----------|----------|-------------|
| Popup Renderer | C++ | Browser action popups |
| Options Page | C++ | Extension settings UI |
| Tab Strip | C++ | Browser action in toolbar |

---

## Implementation Phases

### Phase E1: Core APIs (Weeks 1-4)
**Goal:** Make simple extensions work

| Week | Task | Language |
|------|------|----------|
| 1-2 | chrome.runtime implementation | C++ |
| 3 | chrome.storage implementation | C++ |
| 4 | chrome.tabs/windows basic | C++ |

### Phase E2: Service Workers (Weeks 5-6)
**Goal:** Support Manifest V3 extensions

| Week | Task | Language |
|------|------|----------|
| 5 | Service Worker runtime | C++ |
| 6 | Background event handling | C++ |

### Phase E3: Content Scripts (Weeks 7-8)
**Goal:** Allow extensions to interact with pages

| Week | Task | Language |
|------|------|----------|
| 7 | Script injection system | C++ |
| 8 | Match pattern parser | C++ |

### Phase E4: Complete APIs (Weeks 9-14)
**Goal:** Most extensions work

| Week | Task |
|------|------|
| 9-10 | Bookmarks, history, downloads |
| 11-12 | Context menus, alarms, notifications |
| 13-14 | webRequest, proxy, identity |

### Phase E5: Polish (Week 15-16)
**Goal:** Edge cases and final integration

| Week | Task |
|------|------|
| 15 | Extension loading, updates |
| 16 | Final integration |

---

## Technical Architecture

### Extension Process Model

```
Main Browser Process
├── Extension Manager
├── Chrome API Handler
├── Service Worker Pool (for MV3 extensions)
├── Content Script Injector
└── Message Router
    ├── Extension → Page
    ├── Extension → Background
    └── Extension → Native Apps
```

### Chrome API Implementation Pattern

```cpp
class ChromeRuntimeAPI {
public:
    // chrome.runtime.id
    std::wstring getId() const;
    
    // chrome.runtime.getManifest()
    ExtensionManifest getManifest() const;
    
    // chrome.runtime.sendMessage()
    void sendMessage(const std::wstring& extensionId, 
                     const std::wstring& message,
                     ResponseCallback callback);
    
    // chrome.runtime.onMessage
    Event<std::function<void(Message)>> onMessage;
};
```

---

## Language Distribution

| Language | Usage |
|----------|-------|
| C++ | 70% (Core engine, APIs, injection) |
| Rust | 15% (Security-critical parts) |
| C | 10% (System calls) |
| Python | 5% (Build tools) |

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Complex API surface | High | Medium | Prioritize essential APIs first |
| Service Worker complexity | Medium | High | Use Chrome's spec as reference |
| Security vulnerabilities | Medium | High | Careful message validation |

---

## Conclusion

Implementing Chrome extension support will make Hyperion significantly more useful, as users can leverage the millions of extensions in the Chrome Web Store.

**Estimated Timeline:** 16 weeks  
**Final Capability:** ~90% of Chrome extensions will work in Hyperion

This complements our Phase 2 roadmap (User Experience) and brings Hyperion closer to true Chromium rival.

---

**Document Version:** 1.0  
**Last Updated:** 2026-05-19  
**Status:** Ready for Implementation