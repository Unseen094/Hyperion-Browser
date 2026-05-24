#include <hjs/vm/vm.hpp>
#include <hjs/parser/parser.hpp>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <mutex>
#include <atomic>

namespace hyperion::js {

static std::atomic<uint64_t> g_symbol_counter(0);

class SymbolObject : public object {
public:
    std::wstring description;
    uint64_t unique_id;

    SymbolObject() : object{}, unique_id(0) {
        object_type = ObjectType::Symbol;
    }
};

class BigIntObject : public object {
public:
    std::string digits;
    bool is_negative;

    BigIntObject() : object{}, is_negative(false) {
        object_type = ObjectType::BigInt;
    }
};

value vm::create_symbol(const std::wstring& description) {
    auto* sym = m_gc.allocate<SymbolObject>();
    sym->description = description;
    sym->unique_id = g_symbol_counter.fetch_add(1);
    return value::object_ptr(sym);
}

value vm::symbol_for(const std::wstring& description) {
    auto it = m_symbol_registry.find(description);
    if (it != m_symbol_registry.end()) {
        return it->second;
    }

    auto sym = create_symbol(description);
    m_symbol_registry[description] = sym;
    return sym;
}

value vm::symbol_to_string(const value& sym) {
    if (!sym.is_object()) return value::undefined();
    auto* sym_obj = static_cast<SymbolObject*>(sym.as_object());
    return value::string(L"Symbol(" + sym_obj->description + L")");
}

value vm::symbol_value_of(const value& sym) {
    return sym;
}

value vm::create_bigint(const std::string& digits, bool negative) {
    auto* bi = m_gc.allocate<BigIntObject>();
    bi->digits = digits;
    bi->is_negative = negative;
    return value::object_ptr(bi);
}

value vm::bigint_from_int64(int64_t val) {
    bool negative = val < 0;
    std::string digits = std::to_string(val < 0 ? -val : val);
    return create_bigint(digits, negative);
}

value vm::bigint_to_int64(const value& bi) {
    if (!bi.is_object()) return value::number(0);
    auto* bi_obj = static_cast<BigIntObject*>(bi.as_object());

    int64_t result = 0;
    for (char c : bi_obj->digits) {
        result = result * 10 + (c - '0');
    }

    return value::number(static_cast<double>(bi_obj->is_negative ? -result : result));
}

value vm::bigint_to_string(const value& bi) {
    if (!bi.is_object()) return value::string(L"0");
    auto* bi_obj = static_cast<BigIntObject*>(bi.as_object());

    std::wstring result;
    if (bi_obj->is_negative) result = L"-";
    result += L"";

    for (size_t i = 0; i < bi_obj->digits.size(); ++i) {
        result += static_cast<wchar_t>(bi_obj->digits[i]);
    }

    return value::string(result);
}

value vm::bigint_add(const value& a, const value& b) {
    if (!a.is_object() || !b.is_object()) return value::undefined();
    auto* a_obj = static_cast<BigIntObject*>(a.as_object());
    auto* b_obj = static_cast<BigIntObject*>(b.as_object());

    std::string result;
    bool negative = false;

    if (a_obj->is_negative == b_obj->is_negative) {
        result = string_add(a_obj->digits, b_obj->digits);
        negative = a_obj->is_negative;
    } else {
        int cmp = string_compare(a_obj->digits, b_obj->digits);
        if (cmp > 0) {
            result = string_subtract(a_obj->digits, b_obj->digits);
            negative = a_obj->is_negative;
        } else if (cmp < 0) {
            result = string_subtract(b_obj->digits, a_obj->digits);
            negative = b_obj->is_negative;
        } else {
            return value::number(0);
        }
    }

    return create_bigint(result, negative);
}

value vm::bigint_multiply(const value& a, const value& b) {
    if (!a.is_object() || !b.is_object()) return value::undefined();
    auto* a_obj = static_cast<BigIntObject*>(a.as_object());
    auto* b_obj = static_cast<BigIntObject*>(b.as_object());

    std::string result = string_multiply(a_obj->digits, b_obj->digits);
    bool negative = a_obj->is_negative != b_obj->is_negative;

    return create_bigint(result, negative);
}

std::string vm::string_add(const std::string& a, const std::string& b) {
    std::string result;
    int i = a.size() - 1;
    int j = b.size() - 1;
    int carry = 0;

    while (i >= 0 || j >= 0 || carry) {
        int sum = carry;
        if (i >= 0) sum += a[i--] - '0';
        if (j >= 0) sum += b[j--] - '0';
        result.push_back('0' + (sum % 10));
        carry = sum / 10;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::string vm::string_subtract(const std::string& a, const std::string& b) {
    std::string result;
    int i = a.size() - 1;
    int j = b.size() - 1;
    int borrow = 0;

    while (i >= 0) {
        int diff = (a[i--] - '0') - borrow;
        if (j >= 0) diff -= b[j--] - '0';
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result.push_back('0' + diff);
    }

    while (result.size() > 1 && result.back() == '0') {
        result.pop_back();
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::string vm::string_multiply(const std::string& a, const std::string& b) {
    if (a == "0" || b == "0") return "0";

    std::vector<int> result(a.size() + b.size(), 0);

    for (int i = a.size() - 1; i >= 0; --i) {
        for (int j = b.size() - 1; j >= 0; --j) {
            int mul = (a[i] - '0') * (b[j] - '0');
            int p1 = i + j;
            int p2 = i + j + 1;
            int sum = mul + result[p2];
            result[p2] = sum % 10;
            result[p1] += sum / 10;
        }
    }

    std::string str;
    for (int n : result) {
        if (!(str.empty() && n == 0)) {
            str.push_back('0' + n);
        }
    }

    return str.empty() ? "0" : str;
}

int vm::string_compare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return a.size() < b.size() ? -1 : 1;
    }
    return a.compare(b);
}

class AtomicsObject : public object {
public:
    std::vector<std::atomic<int64_t>> shared_memory;
    std::mutex mutex;
    std::condition_variable cv;

    AtomicsObject() : object{} {
        object_type = ObjectType::Atomics;
    }
};

value vm::create_atomics(size_t size) {
    auto* atomics = m_gc.allocate<AtomicsObject>();
    atomics->shared_memory.resize(size);
    return value::object_ptr(atomics);
}

value vm::atomics_add(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t old = atomics->shared_memory[index].fetch_add(value);
    return value::number(static_cast<double>(old + value));
}

value vm::atomics_and(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t old = atomics->shared_memory[index].fetch_and(value);
    return value::number(static_cast<double>(old));
}

value vm::atomics_or(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t old = atomics->shared_memory[index].fetch_or(value);
    return value::number(static_cast<double>(old));
}

value vm::atomics_xor(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t old = atomics->shared_memory[index].fetch_xor(value);
    return value::number(static_cast<double>(old));
}

value vm::atomics_exchange(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t old = atomics->shared_memory[index].exchange(value);
    return value::number(static_cast<double>(old));
}

value vm::atomics_compare_exchange(const value& buffer, int index, int expected, int replacement) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }

    int64_t current = atomics->shared_memory[index].load();
    if (current == expected) {
        atomics->shared_memory[index].store(replacement);
        return value::number(static_cast<double>(current));
    }
    return value::number(static_cast<double>(current));
}

value vm::atomics_load(const value& buffer, int index) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    int64_t val = atomics->shared_memory[index].load();
    return value::number(static_cast<double>(val));
}

value vm::atomics_store(const value& buffer, int index, int value) {
    if (!buffer.is_object()) return value::undefined();
    auto* atomics = static_cast<AtomicsObject*>(buffer.as_object());
    if (index < 0 || static_cast<size_t>(index) >= atomics->shared_memory.size()) {
        return value::undefined();
    }
    atomics->shared_memory[index].store(value);
    return value::number(static_cast<double>(value));
}

void vm::init_symbol_bigint_atomics() {
    auto* symbol_fn = m_gc.allocate<object>();
    symbol_fn->object_type = ObjectType::Function;

    set_native_func(value::object_ptr(symbol_fn), "for", [this](const std::vector<value>& args) {
        if (args.empty()) return create_symbol(L"");
        return symbol_for(args[0].to_string());
    });

    set_native_func(value::object_ptr(symbol_fn), "keyFor", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::undefined();
        return value::undefined();
    });

    m_global_object.properties[L"Symbol"] = value::object_ptr(symbol_fn);

    m_symbol_prototype = create_object();

    set_native_func(m_symbol_prototype, "toString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::undefined();
        return symbol_to_string(args[0]);
    });

    set_native_func(m_symbol_prototype, "valueOf", [this](const std::vector<value>& args) {
        if (args.empty()) return value::undefined();
        return symbol_value_of(args[0]);
    });

    set_native_func(m_symbol_prototype, "description", [this](const std::vector<value>& args) {
        if (args.empty() || !args[0].is_object()) return value::undefined();
        auto* sym = static_cast<SymbolObject*>(args[0].as_object());
        return value::string(sym->description);
    });

    symbol_fn->properties[L"prototype"] = m_symbol_prototype;

    auto* bigint_fn = m_gc.allocate<object>();
    bigint_fn->object_type = ObjectType::Function;

    set_native_func(value::object_ptr(bigint_fn), "call", [this](const std::vector<value>& args) {
        if (args.empty()) return create_bigint("0", false);
        std::string digits = args[0].to_string();
        bool negative = false;
        if (!digits.empty() && digits[0] == '-') {
            negative = true;
            digits = digits.substr(1);
        }
        return create_bigint(digits, negative);
    });

    set_native_func(value::object_ptr(bigint_fn), "asUintN", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        double bits = args[0].as_number();
        if (!args[1].is_object()) return value::number(0);
        auto* bi = static_cast<BigIntObject*>(args[1].as_object());

        std::string result = bi->digits;
        while (result.size() < static_cast<size_t>(bits / 4)) {
            result = "0" + result;
        }

        return create_bigint(result, false);
    });

    set_native_func(value::object_ptr(bigint_fn), "asIntN", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        double bits = args[0].as_number();
        if (!args[1].is_object()) return value::number(0);
        return args[1];
    });

    m_global_object.properties[L"BigInt"] = value::object_ptr(bigint_fn);

    m_bigint_prototype = create_object();

    set_native_func(m_bigint_prototype, "toString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"0");
        return bigint_to_string(args[0]);
    });

    set_native_func(m_bigint_prototype, "valueOf", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return bigint_to_int64(args[0]);
    });

    set_native_func(m_bigint_prototype, "toLocaleString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"0");
        return bigint_to_string(args[0]);
    });

    bigint_fn->properties[L"prototype"] = m_bigint_prototype;

    auto* atomics_obj = create_object();
    m_global_object.properties[L"Atomics"] = atomics_obj;

    set_native_func(atomics_obj, "add", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_add(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "and", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_and(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "or", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_or(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "xor", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_xor(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "exchange", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_exchange(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "compareExchange", [this](const std::vector<value>& args) {
        if (args.size() < 4) return value::undefined();
        return atomics_compare_exchange(args[0], static_cast<int>(args[1].as_number()),
                                        static_cast<int>(args[2].as_number()), static_cast<int>(args[3].as_number()));
    });

    set_native_func(atomics_obj, "load", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::undefined();
        return atomics_load(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(atomics_obj, "store", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::undefined();
        return atomics_store(args[0], static_cast<int>(args[1].as_number()), static_cast<int>(args[2].as_number()));
    });

    set_native_func(atomics_obj, "wait", [this](const std::vector<value>& args) {
        return value::string(L"not-equal");
    });

    set_native_func(atomics_obj, "notify", [this](const std::vector<value>& args) {
        return value::number(0);
    });

    set_native_func(atomics_obj, "isLockFree", [this](const std::vector<value>& args) {
        return value::boolean(true);
    });
}

}