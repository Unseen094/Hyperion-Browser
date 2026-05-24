#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace hre::render {

// ---- Edge profile ----

struct pgo_edge {
    uint64_t from_block;
    uint64_t to_block;
    uint64_t hit_count;
};

struct pgo_function_info {
    uint64_t address;
    std::string name;
    uint64_t total_hits;
};

static std::vector<pgo_edge> g_pgo_edges;
static std::unordered_map<uint64_t, pgo_function_info> g_pgo_functions;

// ---- Basic block counters ---

#ifdef _WIN32
static std::vector<uint64_t> g_block_counters(1024);
static uint32_t g_next_block_id = 0;
#endif

uint32_t allocate_block_counter() {
#ifdef _WIN32
    uint32_t id = g_next_block_id++;
    if (id >= g_block_counters.size()) {
        g_block_counters.resize(g_block_counters.size() * 2);
    }
    g_block_counters[id] = 0;
    return id;
#else
    return 0;
#endif
}

void increment_block_counter(uint32_t id) {
#ifdef _WIN32
    if (id < g_block_counters.size()) {
        ++g_block_counters[id];
    }
#endif
}

void record_edge(uint64_t from, uint64_t to) {
    auto it = std::find_if(g_pgo_edges.begin(), g_pgo_edges.end(),
        [from, to](const pgo_edge& e) { return e.from_block == from && e.to_block == to; });
    if (it != g_pgo_edges.end()) {
        ++it->hit_count;
    } else {
        g_pgo_edges.push_back({from, to, 1});
    }
}

// ---- PGO trace collection callback ----

void pgo_trace_callback(uint64_t function_addr, const char* function_name) {
    auto& info = g_pgo_functions[function_addr];
    info.address = function_addr;
    info.name = function_name ? function_name : "unknown";
    ++info.total_hits;
}

// ---- Profile serialization ----

bool write_pgo_profile(const std::string& path) {
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "wb");
#else
    f = fopen(path.c_str(), "wb");
#endif
    if (!f) return false;

    // Header
    uint32_t magic = 0x50474F48; // "HPGP"
    fwrite(&magic, sizeof(magic), 1, f);

    uint32_t num_edges = static_cast<uint32_t>(g_pgo_edges.size());
    fwrite(&num_edges, sizeof(num_edges), 1, f);

    for (const auto& e : g_pgo_edges) {
        fwrite(&e.from_block, sizeof(e.from_block), 1, f);
        fwrite(&e.to_block, sizeof(e.to_block), 1, f);
        fwrite(&e.hit_count, sizeof(e.hit_count), 1, f);
    }

    uint32_t num_funcs = static_cast<uint32_t>(g_pgo_functions.size());
    fwrite(&num_funcs, sizeof(num_funcs), 1, f);

    for (const auto& [addr, info] : g_pgo_functions) {
        fwrite(&addr, sizeof(addr), 1, f);
        uint32_t name_len = static_cast<uint32_t>(info.name.size());
        fwrite(&name_len, sizeof(name_len), 1, f);
        fwrite(info.name.data(), 1, name_len, f);
        fwrite(&info.total_hits, sizeof(info.total_hits), 1, f);
    }

    fclose(f);
    return true;
}

// ---- Reset ----

void reset_pgo_data() {
    g_pgo_edges.clear();
    g_pgo_functions.clear();
#ifdef _WIN32
    std::fill(g_block_counters.begin(), g_block_counters.end(), 0);
#endif
}

} // namespace hre::render
