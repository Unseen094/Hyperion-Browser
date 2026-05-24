#include <hjs/vm/vm.hpp>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <codecvt>
#include <locale>

namespace hyperion::js {

struct DateObject : public object {
    time_t timestamp;
    int milliseconds;

    DateObject() : object{}, timestamp(0), milliseconds(0) {
        object_type = ObjectType::Date;
    }
};

static std::wstring get_month_name(int month) {
    static const wchar_t* months[] = {
        L"January", L"February", L"March", L"April", L"May", L"June",
        L"July", L"August", L"September", L"October", L"November", L"December"
    };
    return months[month];
}

static std::wstring get_day_name(int day) {
    static const wchar_t* days[] = {
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
    };
    return days[day];
}

value vm::create_date() {
    auto* date = m_gc.allocate<DateObject>();
    date->timestamp = std::time(nullptr);
    date->milliseconds = 0;
    return value::object_ptr(date);
}

value vm::create_date_from_timestamp(double ms) {
    auto* date = m_gc.allocate<DateObject>();
    date->timestamp = static_cast<time_t>(ms / 1000.0);
    date->milliseconds = static_cast<int>(static_cast<long long>(ms) % 1000);
    return value::object_ptr(date);
}

value vm::date_now() {
    auto now = std::time(nullptr);
    auto ms = static_cast<double>(now) * 1000.0;
    return value::number(ms);
}

value vm::date_parse(const std::wstring& date_str) {
    struct tm tm = {};
    int year, month, day, hour, minute, second;
    wchar_t sep1, sep2, sep3, sep4, sep5;

    std::wistringstream iss(date_str);
    if (iss >> year >> sep1 >> month >> sep2 >> day >> sep3 >> hour >> sep4 >> minute >> sep5 >> second) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;

        time_t t = mktime(&tm);
        if (t != -1) {
            return value::number(static_cast<double>(t) * 1000.0);
        }
    }

    return value::number(0);
}

value vm::date_to_string(const value& date_val) {
    if (!date_val.is_object()) return value::string(L"Invalid Date");
    auto* date = static_cast<DateObject*>(date_val.as_object());

    struct tm tm;
    localtime_s(&tm, &date->timestamp);

    std::wostringstream oss;
    oss << get_day_name(tm.tm_wday) << L" "
        << get_month_name(tm.tm_mon) << L" "
        << tm.tm_mday << L" "
        << (tm.tm_year + 1900) << L" "
        << std::setfill(L'0') << std::setw(2) << tm.tm_hour << L":"
        << std::setfill(L'0') << std::setw(2) << tm.tm_min << L":"
        << std::setfill(L'0') << std::setw(2) << tm.tm_sec;

    return value::string(oss.str());
}

value vm::date_to_iso_string(const value& date_val) {
    if (!date_val.is_object()) return value::string(L"Invalid Date");
    auto* date = static_cast<DateObject*>(date_val.as_object());

    struct tm tm;
    gmtime_s(&tm, &date->timestamp);

    std::wostringstream oss;
    oss << (tm.tm_year + 1900) << L"-"
        << std::setfill(L'0') << std::setw(2) << (tm.tm_mon + 1) << L"-"
        << std::setfill(L'0') << std::setw(2) << tm.tm_mday << L"T"
        << std::setfill(L'0') << std::setw(2) << tm.tm_hour << L":"
        << std::setfill(L'0') << std::setw(2) << tm.tm_min << L":"
        << std::setfill(L'0') << std::setw(2) << tm.tm_sec << L"Z";

    return value::string(oss.str());
}

value vm::date_get_time(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    return value::number(static_cast<double>(date->timestamp) * 1000.0 + date->milliseconds);
}

value vm::date_get_year(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_year + 1900));
}

value vm::date_get_month(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_mon));
}

value vm::date_get_date(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_mday));
}

value vm::date_get_day(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_wday));
}

value vm::date_get_hours(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_hour));
}

value vm::date_get_minutes(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_min));
}

value vm::date_get_seconds(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    return value::number(static_cast<double>(tm.tm_sec));
}

value vm::date_get_milliseconds(const value& date_val) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    return value::number(static_cast<double>(date->milliseconds));
}

value vm::date_set_time(const value& date_val, double time) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    date->timestamp = static_cast<time_t>(time / 1000.0);
    date->milliseconds = static_cast<int>(static_cast<long long>(time) % 1000);
    return value::number(time);
}

value vm::date_set_year(const value& date_val, int year) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_year = year - 1900;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_month(const value& date_val, int month) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_mon = month;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_date(const value& date_val, int day) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_mday = day;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_hours(const value& date_val, int hours) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_hour = hours;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_minutes(const value& date_val, int minutes) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_min = minutes;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_seconds(const value& date_val, int seconds) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    struct tm tm;
    localtime_s(&tm, &date->timestamp);
    tm.tm_sec = seconds;
    date->timestamp = mktime(&tm);
    return date_get_time(date_val);
}

value vm::date_set_milliseconds(const value& date_val, int ms) {
    if (!date_val.is_object()) return value::number(0);
    auto* date = static_cast<DateObject*>(date_val.as_object());
    date->milliseconds = ms;
    return date_get_time(date_val);
}

value vm::date_utc(double year, double month, double day, double hour, double minute, double second) {
    struct tm tm = {};
    tm.tm_year = static_cast<int>(year) - 1900;
    tm.tm_mon = static_cast<int>(month);
    tm.tm_mday = static_cast<int>(day);
    tm.tm_hour = static_cast<int>(hour);
    tm.tm_min = static_cast<int>(minute);
    tm.tm_sec = static_cast<int>(second);

    time_t t = mktime(&tm);
    return value::number(static_cast<double>(t) * 1000.0);
}

void vm::init_date_prototype() {
    m_date_prototype = create_object();

    set_native_func(m_date_prototype, "toString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_string(args[0]);
    });

    set_native_func(m_date_prototype, "toISOString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_iso_string(args[0]);
    });

    set_native_func(m_date_prototype, "toUTCString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_iso_string(args[0]);
    });

    set_native_func(m_date_prototype, "toLocaleString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_string(args[0]);
    });

    set_native_func(m_date_prototype, "getTime", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_time(args[0]);
    });

    set_native_func(m_date_prototype, "getFullYear", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_year(args[0]);
    });

    set_native_func(m_date_prototype, "getMonth", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_month(args[0]);
    });

    set_native_func(m_date_prototype, "getDate", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_date(args[0]);
    });

    set_native_func(m_date_prototype, "getDay", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_day(args[0]);
    });

    set_native_func(m_date_prototype, "getHours", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_hours(args[0]);
    });

    set_native_func(m_date_prototype, "getMinutes", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_minutes(args[0]);
    });

    set_native_func(m_date_prototype, "getSeconds", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_seconds(args[0]);
    });

    set_native_func(m_date_prototype, "getMilliseconds", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_milliseconds(args[0]);
    });

    set_native_func(m_date_prototype, "setTime", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_time(args[0], args[1].as_number());
    });

    set_native_func(m_date_prototype, "setFullYear", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_year(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setMonth", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_month(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setDate", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_date(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setHours", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_hours(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setMinutes", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_minutes(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setSeconds", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_seconds(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "setMilliseconds", [this](const std::vector<value>& args) {
        if (args.size() < 2) return value::number(0);
        return date_set_milliseconds(args[0], static_cast<int>(args[1].as_number()));
    });

    set_native_func(m_date_prototype, "valueOf", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_get_time(args[0]);
    });

    set_native_func(m_date_prototype, "toJSON", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_iso_string(args[0]);
    });

    set_native_func(m_date_prototype, "toDateString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_string(args[0]);
    });

    set_native_func(m_date_prototype, "toTimeString", [this](const std::vector<value>& args) {
        if (args.empty()) return value::string(L"Invalid Date");
        return date_to_string(args[0]);
    });
}

void vm::init_date_constructor() {
    auto* date_fn = m_gc.allocate<object>();
    date_fn->object_type = ObjectType::Function;

    set_native_func(value::object_ptr(date_fn), "now", [this](const std::vector<value>& args) {
        return date_now();
    });

    set_native_func(value::object_ptr(date_fn), "parse", [this](const std::vector<value>& args) {
        if (args.empty()) return value::number(0);
        return date_parse(args[0].to_string());
    });

    set_native_func(value::object_ptr(date_fn), "UTC", [this](const std::vector<value>& args) {
        if (args.size() < 3) return value::number(0);
        double year = args[0].as_number();
        double month = args[1].as_number();
        double day = args[2].as_number();
        double hour = args.size() > 3 ? args[3].as_number() : 0;
        double minute = args.size() > 4 ? args[4].as_number() : 0;
        double second = args.size() > 5 ? args[5].as_number() : 0;
        return date_utc(year, month, day, hour, minute, second);
    });

    date_fn->properties[L"prototype"] = m_date_prototype;
    m_global_object.properties[L"Date"] = value::object_ptr(date_fn);
}

}