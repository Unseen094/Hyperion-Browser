#include <hjs/runtime/stdlib.hpp>
#include <hjs/runtime/array_buffer.hpp>
#include <hjs/runtime/regexp.hpp>
#include <hjs/vm/vm.hpp>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace hjs::runtime {

std::shared_ptr<JSObject> StdLib::array_prototype  = nullptr;
std::shared_ptr<JSObject> StdLib::string_prototype = nullptr;
std::shared_ptr<JSObject> StdLib::math_object      = nullptr;

// ---- Array Methods -------------------------------------------------------

Value array_push(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    for (int i = 0; i < argc; i++) arr->elements.push_back(args[i]);
    double len = (double)arr->elements.size();
    arr->set_property(L"length", Value(len));
    return Value(len);
}

Value array_pop(Value receiver, int, Value*) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr || arr->elements.empty()) return Value();
    Value v = arr->elements.back();
    arr->elements.pop_back();
    arr->set_property(L"length", Value((double)arr->elements.size()));
    return v;
}

Value array_shift(Value receiver, int, Value*) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr || arr->elements.empty()) return Value();
    Value v = arr->elements.front();
    arr->elements.erase(arr->elements.begin());
    arr->set_property(L"length", Value((double)arr->elements.size()));
    return v;
}

Value array_join(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    std::wstring sep = (argc > 0 && args[0].is_string()) ? args[0].as_string() : L",";
    std::wstring result;
    for (size_t i = 0; i < arr->elements.size(); i++) {
        if (i) result += sep;
        result += arr->elements[i].to_string();
    }
    return Value(result);
}

Value array_reverse(Value receiver, int, Value*) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    std::reverse(arr->elements.begin(), arr->elements.end());
    return receiver;
}

Value array_indexOf(Value receiver, int argc, Value* args) {
    if (!receiver.is_object() || argc == 0) return Value(-1.0);
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value(-1.0);
    const auto& target = args[0];
    for (size_t i = 0; i < arr->elements.size(); i++) {
        bool eq = false;
        auto& el = arr->elements[i];
        if (el.is_number() && target.is_number()) eq = el.as_number() == target.as_number();
        else if (el.is_string() && target.is_string()) eq = el.as_string() == target.as_string();
        if (eq) return Value((double)i);
    }
    return Value(-1.0);
}

Value array_slice(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    int n = (int)arr->elements.size();
    int start = argc > 0 ? (int)args[0].as_number() : 0;
    int end_  = argc > 1 ? (int)args[1].as_number() : n;
    if (start < 0) start = std::max(0, n + start);
    if (end_  < 0) end_  = std::max(0, n + end_);
    start = std::min(start, n);
    end_  = std::min(end_, n);
    auto result = std::make_shared<JSArray>();
    for (int i = start; i < end_; i++) result->elements.push_back(arr->elements[i]);
    result->set_property(L"length", Value((double)result->elements.size()));
    if (StdLib::array_prototype) result->set_prototype(StdLib::array_prototype);
    return Value(result);
}

// ---- ES2023 Array Methods --------------------------------------------------

Value array_at(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    int n = (int)arr->elements.size();
    int idx = argc > 0 ? (int)args[0].as_number() : 0;
    if (idx < 0) idx = n + idx;
    if (idx < 0 || idx >= n) return Value();
    return arr->elements[idx];
}

Value array_findLast(Value receiver, int argc, Value* args) {
    if (argc == 0 || !args[0].is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    auto fn = std::dynamic_pointer_cast<NativeFunction>(args[0].as_object());
    if (!fn) return Value();
    for (int i = (int)arr->elements.size() - 1; i >= 0; i--) {
        Value cb_args[] = { arr->elements[i], Value((double)i), receiver };
        Value res = fn->function()(Value(), 3, cb_args);
        if (res.as_boolean()) return arr->elements[i];
    }
    return Value();
}

Value array_findLastIndex(Value receiver, int argc, Value* args) {
    if (argc == 0 || !args[0].is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    auto fn = std::dynamic_pointer_cast<NativeFunction>(args[0].as_object());
    if (!fn) return Value();
    for (int i = (int)arr->elements.size() - 1; i >= 0; i--) {
        Value cb_args[] = { arr->elements[i], Value((double)i), receiver };
        Value res = fn->function()(Value(), 3, cb_args);
        if (res.as_boolean()) return Value((double)i);
    }
    return Value(-1.0);
}

Value array_toReversed(Value receiver, int, Value*) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    auto result = std::make_shared<JSArray>();
    for (int i = (int)arr->elements.size() - 1; i >= 0; i--) result->elements.push_back(arr->elements[i]);
    result->set_property(L"length", Value((double)result->elements.size()));
    if (StdLib::array_prototype) result->set_prototype(StdLib::array_prototype);
    return Value(result);
}

Value array_toSorted(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    auto result = std::make_shared<JSArray>();
    result->elements = arr->elements;
    if (argc > 0 && args[0].is_object()) {
        auto cmp = std::dynamic_pointer_cast<NativeFunction>(args[0].as_object());
        if (cmp) {
            std::sort(result->elements.begin(), result->elements.end(),
                [&](const Value& a, const Value& b) {
                    Value cmp_args[] = { a, b };
                    Value r = cmp->function()(Value(), 2, cmp_args);
                    return r.as_number() < 0;
                });
        }
    } else {
        std::sort(result->elements.begin(), result->elements.end(),
            [](const Value& a, const Value& b) {
                return a.to_string() < b.to_string();
            });
    }
    result->set_property(L"length", Value((double)result->elements.size()));
    if (StdLib::array_prototype) result->set_prototype(StdLib::array_prototype);
    return Value(result);
}

Value array_toSpliced(Value receiver, int argc, Value* args) {
    if (!receiver.is_object()) return Value();
    auto arr = std::dynamic_pointer_cast<JSArray>(receiver.as_object());
    if (!arr) return Value();
    int n = (int)arr->elements.size();
    int start = argc > 0 ? (int)args[0].as_number() : 0;
    if (start < 0) start = std::max(0, n + start);
    int delete_count = argc > 1 ? (int)args[1].as_number() : n - start;
    auto result = std::make_shared<JSArray>();
    for (int i = 0; i < start; i++) result->elements.push_back(arr->elements[i]);
    for (int i = 2; i < argc; i++) result->elements.push_back(args[i]);
    for (int i = start + delete_count; i < n; i++) result->elements.push_back(arr->elements[i]);
    result->set_property(L"length", Value((double)result->elements.size()));
    if (StdLib::array_prototype) result->set_prototype(StdLib::array_prototype);
    return Value(result);
}

// ---- ES2023 String Methods -------------------------------------------------

Value string_isWellFormed(Value receiver, int, Value*) {
    std::wstring s = receiver.to_string();
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] & 0xFC00) == 0xD800) {
            if (i + 1 >= s.size() || (s[i + 1] & 0xFC00) != 0xDC00) return Value(false);
            i++;
        }
    }
    return Value(true);
}

Value string_toWellFormed(Value receiver, int, Value*) {
    std::wstring s = receiver.to_string();
    std::wstring result;
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] & 0xFC00) == 0xD800) {
            if (i + 1 < s.size() && (s[i + 1] & 0xFC00) == 0xDC00) {
                result += s[i]; result += s[i + 1]; i++;
            } else {
                result += L"\uFFFD";
            }
        } else {
            result += s[i];
        }
    }
    return Value(result);
}

// ---- String Methods -------------------------------------------------------

Value string_toUpperCase(Value receiver, int, Value*) {
    std::wstring s = receiver.to_string();
    std::transform(s.begin(), s.end(), s.begin(), ::towupper);
    return Value(s);
}

Value string_toLowerCase(Value receiver, int, Value*) {
    std::wstring s = receiver.to_string();
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return Value(s);
}

Value string_trim(Value receiver, int, Value*) {
    std::wstring s = receiver.to_string();
    size_t first = s.find_first_not_of(L" \t\n\r");
    if (first == std::wstring::npos) return Value(L"");
    size_t last = s.find_last_not_of(L" \t\n\r");
    return Value(s.substr(first, last - first + 1));
}

Value string_split(Value receiver, int argc, Value* args) {
    std::wstring s = receiver.to_string();
    std::wstring sep = argc > 0 && args[0].is_string() ? args[0].as_string() : L"";
    auto arr = std::make_shared<JSArray>();
    if (sep.empty()) {
        for (wchar_t c : s) arr->elements.push_back(Value(std::wstring(1, c)));
    } else {
        size_t pos = 0, last = 0;
        while ((pos = s.find(sep, last)) != std::wstring::npos) {
            arr->elements.push_back(Value(s.substr(last, pos - last)));
            last = pos + sep.size();
        }
        arr->elements.push_back(Value(s.substr(last)));
    }
    arr->set_property(L"length", Value((double)arr->elements.size()));
    if (StdLib::array_prototype) arr->set_prototype(StdLib::array_prototype);
    return Value(arr);
}

Value string_indexOf(Value receiver, int argc, Value* args) {
    if (argc == 0) return Value(-1.0);
    std::wstring s   = receiver.to_string();
    std::wstring sub = args[0].to_string();
    auto pos = s.find(sub);
    return Value(pos == std::wstring::npos ? -1.0 : (double)pos);
}

Value string_includes(Value receiver, int argc, Value* args) {
    if (argc == 0) return Value(false);
    std::wstring s = receiver.to_string();
    return Value(s.find(args[0].to_string()) != std::wstring::npos);
}

Value string_startsWith(Value receiver, int argc, Value* args) {
    if (argc == 0) return Value(false);
    std::wstring s = receiver.to_string();
    std::wstring p = args[0].to_string();
    return Value(s.size() >= p.size() && s.substr(0, p.size()) == p);
}

Value string_endsWith(Value receiver, int argc, Value* args) {
    if (argc == 0) return Value(false);
    std::wstring s = receiver.to_string();
    std::wstring p = args[0].to_string();
    return Value(s.size() >= p.size() && s.substr(s.size() - p.size()) == p);
}

Value string_substring(Value receiver, int argc, Value* args) {
    std::wstring s = receiver.to_string();
    int start = argc > 0 ? (int)args[0].as_number() : 0;
    int end_  = argc > 1 ? (int)args[1].as_number() : (int)s.size();
    start = std::max(0, std::min(start, (int)s.size()));
    end_  = std::max(0, std::min(end_, (int)s.size()));
    if (start > end_) std::swap(start, end_);
    return Value(s.substr(start, end_ - start));
}

Value string_replace(Value receiver, int argc, Value* args) {
    if (argc < 2) return receiver;
    std::wstring s = receiver.to_string();
    std::wstring from = args[0].to_string();
    std::wstring to   = args[1].to_string();
    auto pos = s.find(from);
    if (pos != std::wstring::npos) s.replace(pos, from.size(), to);
    return Value(s);
}

Value string_repeat(Value receiver, int argc, Value* args) {
    std::wstring s = receiver.to_string();
    int times = argc > 0 ? (int)args[0].as_number() : 0;
    std::wstring result;
    for (int i = 0; i < times; i++) result += s;
    return Value(result);
}

// ---- Math Methods -------------------------------------------------------

Value math_floor(Value, int argc, Value* args)  { return argc > 0 ? Value(std::floor(args[0].as_number())) : Value(); }
Value math_ceil(Value, int argc, Value* args)   { return argc > 0 ? Value(std::ceil(args[0].as_number()))  : Value(); }
Value math_round(Value, int argc, Value* args)  { return argc > 0 ? Value(std::round(args[0].as_number())) : Value(); }
Value math_abs(Value, int argc, Value* args)    { return argc > 0 ? Value(std::abs(args[0].as_number()))   : Value(); }
Value math_sqrt(Value, int argc, Value* args)   { return argc > 0 ? Value(std::sqrt(args[0].as_number()))  : Value(); }
Value math_pow(Value, int argc, Value* args)    { return argc > 1 ? Value(std::pow(args[0].as_number(), args[1].as_number())) : Value(); }
Value math_max(Value, int argc, Value* args)    {
    if (argc == 0) return Value(-std::numeric_limits<double>::infinity());
    double m = args[0].as_number();
    for (int i = 1; i < argc; i++) m = std::max(m, args[i].as_number());
    return Value(m);
}
Value math_min(Value, int argc, Value* args) {
    if (argc == 0) return Value(std::numeric_limits<double>::infinity());
    double m = args[0].as_number();
    for (int i = 1; i < argc; i++) m = std::min(m, args[i].as_number());
    return Value(m);
}
Value math_random(Value, int, Value*) { return Value((double)rand() / RAND_MAX); }
Value math_log(Value, int argc, Value* args)  { return argc > 0 ? Value(std::log(args[0].as_number()))  : Value(); }
Value math_log2(Value, int argc, Value* args) { return argc > 0 ? Value(std::log2(args[0].as_number())) : Value(); }

// ---- Initialization -----------------------------------------------------

void StdLib::initialize() {
    // Array prototype
    array_prototype = std::make_shared<JSObject>();
    auto ap = [](NativeFn f){ return Value(std::make_shared<NativeFunction>(f)); };
    array_prototype->set_property(L"push",    ap(array_push));
    array_prototype->set_property(L"pop",     ap(array_pop));
    array_prototype->set_property(L"shift",   ap(array_shift));
    array_prototype->set_property(L"join",    ap(array_join));
    array_prototype->set_property(L"reverse", ap(array_reverse));
    array_prototype->set_property(L"indexOf", ap(array_indexOf));
    array_prototype->set_property(L"slice",   ap(array_slice));
    array_prototype->set_property(L"at",      ap(array_at));
    array_prototype->set_property(L"findLast", ap(array_findLast));
    array_prototype->set_property(L"findLastIndex", ap(array_findLastIndex));
    array_prototype->set_property(L"toReversed", ap(array_toReversed));
    array_prototype->set_property(L"toSorted", ap(array_toSorted));
    array_prototype->set_property(L"toSpliced", ap(array_toSpliced));

    // String prototype
    string_prototype = std::make_shared<JSObject>();
    auto sp = [](NativeFn f){ return Value(std::make_shared<NativeFunction>(f)); };
    string_prototype->set_property(L"toUpperCase",  sp(string_toUpperCase));
    string_prototype->set_property(L"toLowerCase",  sp(string_toLowerCase));
    string_prototype->set_property(L"trim",         sp(string_trim));
    string_prototype->set_property(L"split",        sp(string_split));
    string_prototype->set_property(L"indexOf",      sp(string_indexOf));
    string_prototype->set_property(L"includes",     sp(string_includes));
    string_prototype->set_property(L"startsWith",   sp(string_startsWith));
    string_prototype->set_property(L"endsWith",     sp(string_endsWith));
    string_prototype->set_property(L"substring",    sp(string_substring));
    string_prototype->set_property(L"replace",      sp(string_replace));
    string_prototype->set_property(L"repeat",       sp(string_repeat));
    string_prototype->set_property(L"isWellFormed", sp(string_isWellFormed));
    string_prototype->set_property(L"toWellFormed", sp(string_toWellFormed));

    // Math object
    math_object = std::make_shared<JSObject>();
    auto mp = [](NativeFn f){ return Value(std::make_shared<NativeFunction>(f)); };
    math_object->set_property(L"floor",   mp(math_floor));
    math_object->set_property(L"ceil",    mp(math_ceil));
    math_object->set_property(L"round",   mp(math_round));
    math_object->set_property(L"abs",     mp(math_abs));
    math_object->set_property(L"sqrt",    mp(math_sqrt));
    math_object->set_property(L"pow",     mp(math_pow));
    math_object->set_property(L"max",     mp(math_max));
    math_object->set_property(L"min",     mp(math_min));
    math_object->set_property(L"random",  mp(math_random));
    math_object->set_property(L"log",     mp(math_log));
    math_object->set_property(L"log2",    mp(math_log2));
    math_object->set_property(L"PI",      Value(3.141592653589793));
    math_object->set_property(L"E",       Value(2.718281828459045));

    hjs::vm::VM::m_globals.define(L"Math", Value(math_object));

    // Register ArrayBuffer
    auto array_buffer_ctor = std::make_shared<NativeFunction>(ArrayBuffer::create);
    hjs::vm::VM::m_globals.define(L"ArrayBuffer", Value(array_buffer_ctor));

    // Register SharedArrayBuffer
    auto shared_buffer_ctor = std::make_shared<NativeFunction>(ArrayBuffer::create_shared);
    hjs::vm::VM::m_globals.define(L"SharedArrayBuffer", Value(shared_buffer_ctor));

    // Register RegExp
    auto regexp_ctor = std::make_shared<NativeFunction>(RegExp::create);
    hjs::vm::VM::m_globals.define(L"RegExp", Value(regexp_ctor));
}

} // namespace hjs::runtime
