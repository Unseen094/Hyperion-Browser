#include <hjs/runtime/extensions.hpp>
#include <hjs/vm/vm.hpp>
#include <hjs/runtime/object.hpp>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
inline std::string to_utf8(const std::wstring& w) {
    std::string r;
    for (auto c : w) {
        if (c < 0x80)       { r += (char)c; }
        else if (c < 0x800) { r += (char)(0xC0 | c >> 6); r += (char)(0x80 | (c & 0x3F)); }
        else                { r += (char)(0xE0 | c >> 12); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    }
    return r;
}

namespace hjs::runtime {

// ===== Symbol ==============================================================

static std::atomic<uint64_t> g_symbol_counter(0);

struct SymbolObject : JSObject {
    std::wstring description;
    uint64_t unique_id;
};

Value symbol_for(Value, int argc, Value* args) {
    if (argc < 1 || !args[0].is_string()) return Value();
    std::wstring desc = args[0].as_string();
    static std::map<std::wstring, Value> registry;
    auto it = registry.find(desc);
    if (it != registry.end()) return it->second;
    auto sym = std::make_shared<SymbolObject>();
    sym->description = desc;
    sym->unique_id = g_symbol_counter.fetch_add(1, std::memory_order_relaxed);
    Value result(sym);
    registry[desc] = result;
    return result;
}

Value symbol_keyFor(Value, int, Value*) {
    return Value();
}

// ===== BigInt ==============================================================

struct BigIntObject : JSObject {
    std::string digits;
    bool is_negative = false;
};

static std::string bigint_add_str(const std::string& a, const std::string& b) {
    std::string result;
    int i = (int)a.size() - 1, j = (int)b.size() - 1, carry = 0;
    while (i >= 0 || j >= 0 || carry) {
        int sum = carry;
        if (i >= 0) sum += a[i--] - '0';
        if (j >= 0) sum += b[j--] - '0';
        result.push_back(char('0' + (sum % 10)));
        carry = sum / 10;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

static std::string bigint_sub_str(const std::string& a, const std::string& b) {
    std::string result;
    int i = (int)a.size() - 1, j = (int)b.size() - 1, borrow = 0;
    while (i >= 0) {
        int diff = (a[i--] - '0') - borrow;
        if (j >= 0) diff -= b[j--] - '0';
        if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; }
        result.push_back(char('0' + diff));
    }
    while (result.size() > 1 && result.back() == '0') result.pop_back();
    std::reverse(result.begin(), result.end());
    return result;
}

static std::string bigint_mul_str(const std::string& a, const std::string& b) {
    if (a == "0" || b == "0") return "0";
    std::vector<int> res(a.size() + b.size(), 0);
    for (int i = (int)a.size() - 1; i >= 0; --i)
        for (int j = (int)b.size() - 1; j >= 0; --j) {
            int mul = (a[i] - '0') * (b[j] - '0');
            int p1 = i + j, p2 = i + j + 1, sum = mul + res[p2];
            res[p2] = sum % 10; res[p1] += sum / 10;
        }
    std::string str;
    for (int n : res) if (!(str.empty() && n == 0)) str.push_back(char('0' + n));
    return str.empty() ? "0" : str;
}

static std::shared_ptr<BigIntObject> get_bigint(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<BigIntObject>(v.as_object());
}

Value bigint_call(Value, int argc, Value* args) {
    if (argc == 0) return Value((int64_t)0);
    std::string digits = to_utf8(args[0].to_string());
    bool neg = false;
    if (!digits.empty() && digits[0] == '-') { neg = true; digits = digits.substr(1); }
    auto bi = std::make_shared<BigIntObject>();
    bi->digits = digits; bi->is_negative = neg;
    return Value(bi);
}

Value bigint_asUintN(Value, int argc, Value* args) {
    if (argc < 2) return Value((int64_t)0);
    auto bi = get_bigint(args[1]);
    if (!bi) return Value((int64_t)0);
    uint64_t bits = (uint64_t)args[0].as_number();
    auto res = std::make_shared<BigIntObject>();
    res->digits = bi->digits;
    while (res->digits.size() > bits / 4 + 1) res->digits = "0";
    return Value(res);
}

Value bigint_asIntN(Value, int argc, Value* args) {
    if (argc < 2) return Value((int64_t)0);
    return args[1];
}

Value bigint_toString(Value receiver, int, Value*) {
    auto bi = get_bigint(receiver);
    if (!bi) return Value(L"0");
    std::wstring result;
    if (bi->is_negative) result = L"-";
    for (char c : bi->digits) result += wchar_t(c);
    return Value(result);
}

Value bigint_valueOf(Value receiver, int, Value*) {
    return receiver;
}

// ===== Atomics =============================================================

struct AtomicsObject : JSObject {
    std::vector<std::atomic<int64_t>> shared_memory;
    std::mutex mtx;
    std::condition_variable cv;
};

static std::shared_ptr<AtomicsObject> get_atomics(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<AtomicsObject>(v.as_object());
}

Value atomics_add(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].fetch_add((int64_t)args[2].as_number()));
}

Value atomics_and(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].fetch_and((int64_t)args[2].as_number()));
}

Value atomics_or(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].fetch_or((int64_t)args[2].as_number()));
}

Value atomics_xor(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].fetch_xor((int64_t)args[2].as_number()));
}

Value atomics_exchange(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].exchange((int64_t)args[2].as_number()));
}

Value atomics_compareExchange(Value, int argc, Value* args) {
    if (argc < 4) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    int64_t expected = (int64_t)args[2].as_number();
    int64_t replacement = (int64_t)args[3].as_number();
    return Value((double)a->shared_memory[idx].compare_exchange_strong(expected, replacement) ? expected : a->shared_memory[idx].load());
}

Value atomics_load(Value, int argc, Value* args) {
    if (argc < 2) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    return Value((double)a->shared_memory[idx].load());
}

Value atomics_store(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto a = get_atomics(args[0]);
    if (!a) return Value();
    int idx = (int)args[1].as_number();
    if (idx < 0 || (size_t)idx >= a->shared_memory.size()) return Value();
    a->shared_memory[idx].store((int64_t)args[2].as_number());
    return Value((double)args[2].as_number());
}

Value atomics_isLockFree(Value, int, Value*) {
    return Value(true);
}

Value atomics_wait(Value, int, Value*) {
    return Value(L"not-equal");
}

Value atomics_notify(Value, int, Value*) {
    return Value(0.0);
}

// ===== Map =================================================================

struct MapEntry {
    Value key;
    Value value;
    bool deleted = false;
};

struct MapObject : JSObject {
    std::vector<MapEntry> entries;
    std::map<std::wstring, size_t> key_index;
    size_t map_size = 0;
};

static std::wstring value_to_key(const Value& v) {
    if (v.is_string()) return v.as_string();
    if (v.is_number()) {
        std::wostringstream ss; ss << v.as_number(); return ss.str();
    }
    if (v.is_boolean()) return v.as_boolean() ? L"true" : L"false";
    if (v.is_object()) return L"obj:" + std::to_wstring((uintptr_t)v.as_object().get());
    return L"";
}

static std::shared_ptr<MapObject> get_map(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<MapObject>(v.as_object());
}

Value map_get(Value, int argc, Value* args) {
    if (argc < 2) return Value();
    auto m = get_map(args[0]); if (!m) return Value();
    auto it = m->key_index.find(value_to_key(args[1]));
    if (it != m->key_index.end() && !m->entries[it->second].deleted)
        return m->entries[it->second].value;
    return Value();
}

Value map_set(Value, int argc, Value* args) {
    if (argc < 3) return Value();
    auto m = get_map(args[0]); if (!m) return Value();
    std::wstring ks = value_to_key(args[1]);
    auto it = m->key_index.find(ks);
    if (it != m->key_index.end() && !m->entries[it->second].deleted) {
        m->entries[it->second].value = args[2];
        return args[0];
    }
    MapEntry e; e.key = args[1]; e.value = args[2];
    m->key_index[ks] = m->entries.size();
    m->entries.push_back(e);
    m->map_size++;
    return args[0];
}

Value map_has(Value, int argc, Value* args) {
    if (argc < 2) return Value(false);
    auto m = get_map(args[0]); if (!m) return Value(false);
    auto it = m->key_index.find(value_to_key(args[1]));
    return Value(it != m->key_index.end() && !m->entries[it->second].deleted);
}

Value map_delete(Value, int argc, Value* args) {
    if (argc < 2) return Value(false);
    auto m = get_map(args[0]); if (!m) return Value(false);
    auto it = m->key_index.find(value_to_key(args[1]));
    if (it != m->key_index.end() && !m->entries[it->second].deleted) {
        m->entries[it->second].deleted = true;
        m->map_size--;
        return Value(true);
    }
    return Value(false);
}

Value map_clear(Value, int argc, Value* args) {
    if (argc < 1) return Value();
    auto m = get_map(args[0]); if (!m) return Value();
    m->entries.clear(); m->key_index.clear(); m->map_size = 0;
    return Value();
}

Value map_get_size(Value, int argc, Value* args) {
    if (argc < 1) return Value(0.0);
    auto m = get_map(args[0]); if (!m) return Value(0.0);
    return Value((double)m->map_size);
}

// ===== Set =================================================================

struct SetObject : JSObject {
    std::set<std::wstring> keys;
    std::vector<Value> values;
    size_t set_size = 0;
};

static std::shared_ptr<SetObject> get_set(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<SetObject>(v.as_object());
}

Value set_add(Value, int argc, Value* args) {
    if (argc < 2) return Value();
    auto s = get_set(args[0]); if (!s) return Value();
    std::wstring ks = value_to_key(args[1]);
    if (s->keys.find(ks) == s->keys.end()) {
        s->keys.insert(ks);
        s->values.push_back(args[1]);
        s->set_size++;
    }
    return args[0];
}

Value set_has(Value, int argc, Value* args) {
    if (argc < 2) return Value(false);
    auto s = get_set(args[0]); if (!s) return Value(false);
    return Value(s->keys.find(value_to_key(args[1])) != s->keys.end());
}

Value set_delete(Value, int argc, Value* args) {
    if (argc < 2) return Value(false);
    auto s = get_set(args[0]); if (!s) return Value(false);
    std::wstring ks = value_to_key(args[1]);
    auto it = s->keys.find(ks);
    if (it != s->keys.end()) {
        s->keys.erase(it);
        s->set_size--;
        return Value(true);
    }
    return Value(false);
}

Value set_clear(Value, int argc, Value* args) {
    if (argc < 1) return Value();
    auto s = get_set(args[0]); if (!s) return Value();
    s->keys.clear(); s->values.clear(); s->set_size = 0;
    return Value();
}

Value set_get_size(Value, int argc, Value* args) {
    if (argc < 1) return Value(0.0);
    auto s = get_set(args[0]); if (!s) return Value(0.0);
    return Value((double)s->set_size);
}

// ===== WeakMap =============================================================

struct WeakMapObject : JSObject {
    std::vector<std::pair<JSObject*, Value>> entries;
};

static std::shared_ptr<WeakMapObject> get_weakmap(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<WeakMapObject>(v.as_object());
}

Value weakmap_get(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value();
    auto wm = get_weakmap(args[0]); if (!wm) return Value();
    auto* key = args[1].as_object().get();
    for (auto& e : wm->entries) if (e.first == key) return e.second;
    return Value();
}

Value weakmap_set(Value, int argc, Value* args) {
    if (argc < 3 || !args[1].is_object()) return Value();
    auto wm = get_weakmap(args[0]); if (!wm) return Value();
    auto* key = args[1].as_object().get();
    for (auto& e : wm->entries) if (e.first == key) { e.second = args[2]; return args[0]; }
    wm->entries.push_back({key, args[2]});
    return args[0];
}

Value weakmap_has(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value(false);
    auto wm = get_weakmap(args[0]); if (!wm) return Value(false);
    auto* key = args[1].as_object().get();
    for (auto& e : wm->entries) if (e.first == key) return Value(true);
    return Value(false);
}

Value weakmap_delete(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value(false);
    auto wm = get_weakmap(args[0]); if (!wm) return Value(false);
    auto* key = args[1].as_object().get();
    for (auto it = wm->entries.begin(); it != wm->entries.end(); ++it)
        if (it->first == key) { wm->entries.erase(it); return Value(true); }
    return Value(false);
}

// ===== WeakSet =============================================================

struct WeakSetObject : JSObject {
    std::vector<JSObject*> values;
};

static std::shared_ptr<WeakSetObject> get_weakset(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<WeakSetObject>(v.as_object());
}

Value weakset_add(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value();
    auto ws = get_weakset(args[0]); if (!ws) return Value();
    auto* key = args[1].as_object().get();
    for (auto* v : ws->values) if (v == key) return args[0];
    ws->values.push_back(key);
    return args[0];
}

Value weakset_has(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value(false);
    auto ws = get_weakset(args[0]); if (!ws) return Value(false);
    auto* key = args[1].as_object().get();
    for (auto* v : ws->values) if (v == key) return Value(true);
    return Value(false);
}

Value weakset_delete(Value, int argc, Value* args) {
    if (argc < 2 || !args[1].is_object()) return Value(false);
    auto ws = get_weakset(args[0]); if (!ws) return Value(false);
    auto* key = args[1].as_object().get();
    for (auto it = ws->values.begin(); it != ws->values.end(); ++it)
        if (*it == key) { ws->values.erase(it); return Value(true); }
    return Value(false);
}

// ===== Date ================================================================

struct DateObject : JSObject {
    time_t timestamp = 0;
    int milliseconds = 0;
};

static std::shared_ptr<DateObject> get_date(const Value& v) {
    if (!v.is_object()) return nullptr;
    return std::dynamic_pointer_cast<DateObject>(v.as_object());
}

static std::wstring month_name(int m) {
    static const wchar_t* names[] = { L"January", L"February", L"March", L"April", L"May", L"June",
        L"July", L"August", L"September", L"October", L"November", L"December" };
    return names[m % 12];
}

static std::wstring day_name(int d) {
    static const wchar_t* names[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    return names[d % 7];
}

Value date_now(Value, int, Value*) {
    return Value((double)std::time(nullptr) * 1000.0);
}

Value date_parse(Value, int argc, Value* args) {
    if (argc == 0) return Value(0.0);
    struct tm tm = {}; int y, m, d, h, mi, s;
    std::wistringstream iss(args[0].to_string());
    if (iss >> y >> m >> d >> h >> mi >> s) {
        tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d;
        tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = s;
        return Value((double)std::mktime(&tm) * 1000.0);
    }
    return Value(0.0);
}

Value date_create(Value, int argc, Value* args) {
    auto d = std::make_shared<DateObject>();
    if (argc == 0) {
        d->timestamp = std::time(nullptr);
    } else if (argc >= 3) {
        struct tm tm = {};
        tm.tm_year = (int)args[0].as_number() - 1900;
        tm.tm_mon = (int)args[1].as_number();
        tm.tm_mday = (int)args[2].as_number();
        tm.tm_hour = argc > 3 ? (int)args[3].as_number() : 0;
        tm.tm_min = argc > 4 ? (int)args[4].as_number() : 0;
        tm.tm_sec = argc > 5 ? (int)args[5].as_number() : 0;
        d->timestamp = std::mktime(&tm);
    } else {
        d->timestamp = (time_t)(args[0].as_number() / 1000.0);
        d->milliseconds = (int)((int64_t)args[0].as_number() % 1000);
    }
    return Value(d);
}

Value date_getTime(Value receiver, int, Value*) {
    auto d = get_date(receiver); if (!d) return Value(0.0);
    return Value((double)d->timestamp * 1000.0 + d->milliseconds);
}

#define DATE_GET_FIELD(field, tm_field) \
    Value date_get_##field(Value receiver, int, Value*) { \
        auto d = get_date(receiver); if (!d) return Value(0.0); \
        struct tm tm; localtime_s(&tm, &d->timestamp); \
        return Value((double)tm.tm_##tm_field); \
    }

DATE_GET_FIELD(year, year + 1900)
DATE_GET_FIELD(month, mon)
DATE_GET_FIELD(date, mday)
DATE_GET_FIELD(day, wday)
DATE_GET_FIELD(hours, hour)
DATE_GET_FIELD(minutes, min)
DATE_GET_FIELD(seconds, sec)

Value date_getMilliseconds(Value receiver, int, Value*) {
    auto d = get_date(receiver); if (!d) return Value(0.0);
    return Value((double)d->milliseconds);
}

Value date_setTime(Value, int argc, Value* args) {
    if (argc < 2) return Value(0.0);
    auto d = get_date(args[0]); if (!d) return Value(0.0);
    d->timestamp = (time_t)(args[1].as_number() / 1000.0);
    d->milliseconds = (int)((int64_t)args[1].as_number() % 1000);
    return args[1];
}

#define DATE_SET_FIELD(field, tm_member) \
    Value date_set_##field(Value, int argc, Value* args) { \
        if (argc < 2) return Value(0.0); \
        auto d = get_date(args[0]); if (!d) return Value(0.0); \
        struct tm tm; localtime_s(&tm, &d->timestamp); \
        tm.tm_##tm_member = (int)args[1].as_number(); \
        d->timestamp = std::mktime(&tm); \
        return Value((double)d->timestamp * 1000.0 + d->milliseconds); \
    }

// The year setter adjusts by -1900 per JS spec (Date.setFullYear takes 4-digit year)
Value date_set_year(Value receiver, int argc, Value* args) {
    if (argc < 2) return Value(0.0);
    auto d = get_date(args[0]); if (!d) return Value(0.0);
    struct tm tm; localtime_s(&tm, &d->timestamp);
    tm.tm_year = (int)args[1].as_number() - 1900;
    d->timestamp = std::mktime(&tm);
    return Value((double)d->timestamp * 1000.0 + d->milliseconds);
}
DATE_SET_FIELD(month, mon)
DATE_SET_FIELD(date, mday)
DATE_SET_FIELD(hours, hour)
DATE_SET_FIELD(minutes, min)
DATE_SET_FIELD(seconds, sec)

Value date_setMilliseconds(Value, int argc, Value* args) {
    if (argc < 2) return Value(0.0);
    auto d = get_date(args[0]); if (!d) return Value(0.0);
    d->milliseconds = (int)args[1].as_number();
    return Value((double)d->timestamp * 1000.0 + d->milliseconds);
}

Value date_toString(Value receiver, int, Value*) {
    auto d = get_date(receiver); if (!d) return Value(L"Invalid Date");
    struct tm tm; localtime_s(&tm, &d->timestamp);
    std::wostringstream oss;
    oss << day_name(tm.tm_wday) << L" " << month_name(tm.tm_mon) << L" " << tm.tm_mday
        << L" " << (tm.tm_year + 1900) << L" " << std::setfill(L'0') << std::setw(2) << tm.tm_hour
        << L":" << std::setfill(L'0') << std::setw(2) << tm.tm_min
        << L":" << std::setfill(L'0') << std::setw(2) << tm.tm_sec;
    return Value(oss.str());
}

Value date_toISOString(Value receiver, int, Value*) {
    auto d = get_date(receiver); if (!d) return Value(L"Invalid Date");
    struct tm tm; gmtime_s(&tm, &d->timestamp);
    std::wostringstream oss;
    oss << (tm.tm_year + 1900) << L"-" << std::setfill(L'0') << std::setw(2) << (tm.tm_mon + 1)
        << L"-" << std::setfill(L'0') << std::setw(2) << tm.tm_mday
        << L"T" << std::setfill(L'0') << std::setw(2) << tm.tm_hour
        << L":" << std::setfill(L'0') << std::setw(2) << tm.tm_min
        << L":" << std::setfill(L'0') << std::setw(2) << tm.tm_sec << L"Z";
    return Value(oss.str());
}

Value date_toJSON(Value receiver, int, Value*) {
    return date_toISOString(receiver, 0, nullptr);
}

Value date_valueOf(Value receiver, int, Value*) {
    auto d = get_date(receiver); if (!d) return Value(0.0);
    return Value((double)d->timestamp * 1000.0 + d->milliseconds);
}

Value date_UTC(Value, int argc, Value* args) {
    if (argc < 3) return Value(0.0);
    struct tm tm = {};
    tm.tm_year = (int)args[0].as_number() - 1900;
    tm.tm_mon = (int)args[1].as_number();
    tm.tm_mday = (int)args[2].as_number();
    tm.tm_hour = argc > 3 ? (int)args[3].as_number() : 0;
    tm.tm_min = argc > 4 ? (int)args[4].as_number() : 0;
    tm.tm_sec = argc > 5 ? (int)args[5].as_number() : 0;
    return Value((double)std::mktime(&tm) * 1000.0);
}

// ===== setup_extensions ====================================================

void setup_extensions() {
    // ---- Symbol ----
    auto symbol_fn = std::make_shared<JSObject>();
    symbol_fn->set_property(L"for", Value(std::make_shared<NativeFunction>(symbol_for)));
    symbol_fn->set_property(L"keyFor", Value(std::make_shared<NativeFunction>(symbol_keyFor)));

    auto symbol_proto = std::make_shared<JSObject>();
    symbol_proto->set_property(L"toString", Value(std::make_shared<NativeFunction>(bigint_toString))); // placeholder
    symbol_proto->set_property(L"valueOf", Value(std::make_shared<NativeFunction>(bigint_valueOf)));
    symbol_fn->set_property(L"prototype", Value(symbol_proto));

    hjs::vm::VM::m_globals.define(L"Symbol", Value(symbol_fn));

    // ---- BigInt ----
    auto bigint_fn = std::make_shared<JSObject>();
    bigint_fn->set_property(L"asUintN", Value(std::make_shared<NativeFunction>(bigint_asUintN)));
    bigint_fn->set_property(L"asIntN", Value(std::make_shared<NativeFunction>(bigint_asIntN)));

    auto bigint_proto = std::make_shared<JSObject>();
    bigint_proto->set_property(L"toString", Value(std::make_shared<NativeFunction>(bigint_toString)));
    bigint_proto->set_property(L"valueOf", Value(std::make_shared<NativeFunction>(bigint_valueOf)));
    bigint_proto->set_property(L"toLocaleString", Value(std::make_shared<NativeFunction>(bigint_toString)));
    bigint_fn->set_property(L"prototype", Value(bigint_proto));

    hjs::vm::VM::m_globals.define(L"BigInt", Value(bigint_fn));

    // ---- Atomics ----
    auto atomics_obj = std::make_shared<JSObject>();
    atomics_obj->set_property(L"add", Value(std::make_shared<NativeFunction>(atomics_add)));
    atomics_obj->set_property(L"and", Value(std::make_shared<NativeFunction>(atomics_and)));
    atomics_obj->set_property(L"or", Value(std::make_shared<NativeFunction>(atomics_or)));
    atomics_obj->set_property(L"xor", Value(std::make_shared<NativeFunction>(atomics_xor)));
    atomics_obj->set_property(L"exchange", Value(std::make_shared<NativeFunction>(atomics_exchange)));
    atomics_obj->set_property(L"compareExchange", Value(std::make_shared<NativeFunction>(atomics_compareExchange)));
    atomics_obj->set_property(L"load", Value(std::make_shared<NativeFunction>(atomics_load)));
    atomics_obj->set_property(L"store", Value(std::make_shared<NativeFunction>(atomics_store)));
    atomics_obj->set_property(L"wait", Value(std::make_shared<NativeFunction>(atomics_wait)));
    atomics_obj->set_property(L"notify", Value(std::make_shared<NativeFunction>(atomics_notify)));
    atomics_obj->set_property(L"isLockFree", Value(std::make_shared<NativeFunction>(atomics_isLockFree)));
    hjs::vm::VM::m_globals.define(L"Atomics", Value(atomics_obj));

    // ---- Map ----
    auto map_proto = std::make_shared<JSObject>();
    map_proto->set_property(L"get", Value(std::make_shared<NativeFunction>(map_get)));
    map_proto->set_property(L"set", Value(std::make_shared<NativeFunction>(map_set)));
    map_proto->set_property(L"has", Value(std::make_shared<NativeFunction>(map_has)));
    map_proto->set_property(L"delete", Value(std::make_shared<NativeFunction>(map_delete)));
    map_proto->set_property(L"clear", Value(std::make_shared<NativeFunction>(map_clear)));
    map_proto->set_property(L"size", Value(std::make_shared<NativeFunction>(map_get_size)));

    auto map_fn = std::make_shared<JSObject>();
    map_fn->set_property(L"prototype", Value(map_proto));
    hjs::vm::VM::m_globals.define(L"Map", Value(map_fn));

    // ---- Set ----
    auto set_proto = std::make_shared<JSObject>();
    set_proto->set_property(L"add", Value(std::make_shared<NativeFunction>(set_add)));
    set_proto->set_property(L"has", Value(std::make_shared<NativeFunction>(set_has)));
    set_proto->set_property(L"delete", Value(std::make_shared<NativeFunction>(set_delete)));
    set_proto->set_property(L"clear", Value(std::make_shared<NativeFunction>(set_clear)));
    set_proto->set_property(L"size", Value(std::make_shared<NativeFunction>(set_get_size)));

    auto set_fn = std::make_shared<JSObject>();
    set_fn->set_property(L"prototype", Value(set_proto));
    hjs::vm::VM::m_globals.define(L"Set", Value(set_fn));

    // ---- WeakMap ----
    auto wm_proto = std::make_shared<JSObject>();
    wm_proto->set_property(L"get", Value(std::make_shared<NativeFunction>(weakmap_get)));
    wm_proto->set_property(L"set", Value(std::make_shared<NativeFunction>(weakmap_set)));
    wm_proto->set_property(L"has", Value(std::make_shared<NativeFunction>(weakmap_has)));
    wm_proto->set_property(L"delete", Value(std::make_shared<NativeFunction>(weakmap_delete)));

    auto wm_fn = std::make_shared<JSObject>();
    wm_fn->set_property(L"prototype", Value(wm_proto));
    hjs::vm::VM::m_globals.define(L"WeakMap", Value(wm_fn));

    // ---- WeakSet ----
    auto ws_proto = std::make_shared<JSObject>();
    ws_proto->set_property(L"add", Value(std::make_shared<NativeFunction>(weakset_add)));
    ws_proto->set_property(L"has", Value(std::make_shared<NativeFunction>(weakset_has)));
    ws_proto->set_property(L"delete", Value(std::make_shared<NativeFunction>(weakset_delete)));

    auto ws_fn = std::make_shared<JSObject>();
    ws_fn->set_property(L"prototype", Value(ws_proto));
    hjs::vm::VM::m_globals.define(L"WeakSet", Value(ws_fn));

    // ---- Date ----
    auto date_proto = std::make_shared<JSObject>();
    date_proto->set_property(L"toString", Value(std::make_shared<NativeFunction>(date_toString)));
    date_proto->set_property(L"toISOString", Value(std::make_shared<NativeFunction>(date_toISOString)));
    date_proto->set_property(L"toUTCString", Value(std::make_shared<NativeFunction>(date_toISOString)));
    date_proto->set_property(L"toJSON", Value(std::make_shared<NativeFunction>(date_toJSON)));
    date_proto->set_property(L"valueOf", Value(std::make_shared<NativeFunction>(date_valueOf)));
    date_proto->set_property(L"getTime", Value(std::make_shared<NativeFunction>(date_getTime)));
    date_proto->set_property(L"getFullYear", Value(std::make_shared<NativeFunction>(date_get_year)));
    date_proto->set_property(L"getMonth", Value(std::make_shared<NativeFunction>(date_get_month)));
    date_proto->set_property(L"getDate", Value(std::make_shared<NativeFunction>(date_get_date)));
    date_proto->set_property(L"getDay", Value(std::make_shared<NativeFunction>(date_get_day)));
    date_proto->set_property(L"getHours", Value(std::make_shared<NativeFunction>(date_get_hours)));
    date_proto->set_property(L"getMinutes", Value(std::make_shared<NativeFunction>(date_get_minutes)));
    date_proto->set_property(L"getSeconds", Value(std::make_shared<NativeFunction>(date_get_seconds)));
    date_proto->set_property(L"getMilliseconds", Value(std::make_shared<NativeFunction>(date_getMilliseconds)));
    date_proto->set_property(L"setTime", Value(std::make_shared<NativeFunction>(date_setTime)));
    date_proto->set_property(L"setFullYear", Value(std::make_shared<NativeFunction>(date_set_year)));
    date_proto->set_property(L"setMonth", Value(std::make_shared<NativeFunction>(date_set_month)));
    date_proto->set_property(L"setDate", Value(std::make_shared<NativeFunction>(date_set_date)));
    date_proto->set_property(L"setHours", Value(std::make_shared<NativeFunction>(date_set_hours)));
    date_proto->set_property(L"setMinutes", Value(std::make_shared<NativeFunction>(date_set_minutes)));
    date_proto->set_property(L"setSeconds", Value(std::make_shared<NativeFunction>(date_set_seconds)));
    date_proto->set_property(L"setMilliseconds", Value(std::make_shared<NativeFunction>(date_setMilliseconds)));

    auto date_fn = std::make_shared<JSObject>();
    date_fn->set_property(L"now", Value(std::make_shared<NativeFunction>(date_now)));
    date_fn->set_property(L"parse", Value(std::make_shared<NativeFunction>(date_parse)));
    date_fn->set_property(L"UTC", Value(std::make_shared<NativeFunction>(date_UTC)));
    date_fn->set_property(L"prototype", Value(date_proto));

    hjs::vm::VM::m_globals.define(L"Date", Value(date_fn));
}

} // namespace hjs::runtime
