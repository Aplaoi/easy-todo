#include "json.h"

namespace hf {

JsonValue JsonObject() {
    JsonValue v;
    v.type = JsonValue::Type::Object;
    return v;
}
JsonValue JsonArray() {
    JsonValue v;
    v.type = JsonValue::Type::Array;
    return v;
}
JsonValue JsonStr(const std::wstring& s) {
    JsonValue v;
    v.type = JsonValue::Type::String;
    v.string = s;
    return v;
}
JsonValue JsonNum(double n) {
    JsonValue v;
    v.type = JsonValue::Type::Number;
    v.number = n;
    return v;
}
JsonValue JsonBool(bool b) {
    JsonValue v;
    v.type = JsonValue::Type::Bool;
    v.boolean = b;
    return v;
}

namespace {

class Parser {
public:
    explicit Parser(const std::wstring& text) : s_(text) {}

    bool Parse(JsonValue& out) {
        SkipWs();
        if (!ParseValue(out, 0)) return false;
        SkipWs();
        if (i_ != s_.size()) return Fail(L"JSON 末尾存在多余内容");
        return true;
    }

    const std::wstring& error() const { return error_; }

private:
    const std::wstring& s_;
    size_t i_ = 0;
    std::wstring error_;

    bool Fail(const wchar_t* msg) {
        if (error_.empty()) error_ = Format(L"%s（位置 %zu）", msg, i_);
        return false;
    }

    void SkipWs() {
        while (i_ < s_.size()) {
            wchar_t c = s_[i_];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r')
                ++i_;
            else
                break;
        }
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > 32) return Fail(L"JSON 嵌套过深");
        if (i_ >= s_.size()) return Fail(L"JSON 意外结束");
        wchar_t c = s_[i_];
        if (c == L'{') return ParseObject(out, depth);
        if (c == L'[') return ParseArray(out, depth);
        if (c == L'"') {
            out.type = JsonValue::Type::String;
            return ParseString(out.string);
        }
        if (c == L't') {
            if (!ParseLiteral(L"true")) return false;
            out.type = JsonValue::Type::Bool;
            out.boolean = true;
            return true;
        }
        if (c == L'f') {
            if (!ParseLiteral(L"false")) return false;
            out.type = JsonValue::Type::Bool;
            out.boolean = false;
            return true;
        }
        if (c == L'n') {
            if (!ParseLiteral(L"null")) return false;
            out.type = JsonValue::Type::Null;
            return true;
        }
        if (c == L'-' || (c >= L'0' && c <= L'9')) return ParseNumber(out);
        return Fail(L"无法识别的 JSON 值");
    }

    bool ParseLiteral(const wchar_t* lit) {
        size_t n = wcslen(lit);
        if (i_ + n > s_.size() || s_.compare(i_, n, lit) != 0) return Fail(L"JSON 字面量错误");
        i_ += n;
        return true;
    }

    bool ParseObject(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Object;
        out.object.clear();
        ++i_;  // {
        SkipWs();
        if (i_ < s_.size() && s_[i_] == L'}') {
            ++i_;
            return true;
        }
        while (true) {
            SkipWs();
            if (i_ >= s_.size() || s_[i_] != L'"') return Fail(L"对象键必须是字符串");
            std::wstring key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (i_ >= s_.size() || s_[i_] != L':') return Fail(L"对象缺少冒号");
            ++i_;
            SkipWs();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.object[key] = std::move(value);
            SkipWs();
            if (i_ >= s_.size()) return Fail(L"对象未闭合");
            if (s_[i_] == L',') {
                ++i_;
                continue;
            }
            if (s_[i_] == L'}') {
                ++i_;
                return true;
            }
            return Fail(L"对象成员之间缺少逗号");
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Array;
        out.array.clear();
        ++i_;  // [
        SkipWs();
        if (i_ < s_.size() && s_[i_] == L']') {
            ++i_;
            return true;
        }
        while (true) {
            SkipWs();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.array.push_back(std::move(value));
            SkipWs();
            if (i_ >= s_.size()) return Fail(L"数组未闭合");
            if (s_[i_] == L',') {
                ++i_;
                continue;
            }
            if (s_[i_] == L']') {
                ++i_;
                return true;
            }
            return Fail(L"数组元素之间缺少逗号");
        }
    }

    static int HexDigit(wchar_t c) {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    }

    bool ParseHex4(unsigned& value) {
        if (i_ + 4 > s_.size()) return Fail(L"\\u 转义不完整");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            int d = HexDigit(s_[i_ + k]);
            if (d < 0) return Fail(L"\\u 转义包含非法字符");
            v = (v << 4) | (unsigned)d;
        }
        i_ += 4;
        value = v;
        return true;
    }

    bool ParseString(std::wstring& out) {
        if (i_ >= s_.size() || s_[i_] != L'"') return Fail(L"字符串缺少引号");
        ++i_;
        out.clear();
        while (i_ < s_.size()) {
            wchar_t c = s_[i_++];
            if (c == L'"') return true;
            if (static_cast<unsigned>(c) < 0x20) return Fail(L"字符串包含控制字符");
            if (c != L'\\') {
                out.push_back(c);
                continue;
            }
            if (i_ >= s_.size()) return Fail(L"字符串转义意外结束");
            wchar_t e = s_[i_++];
            switch (e) {
                case L'"': out.push_back(L'"'); break;
                case L'\\': out.push_back(L'\\'); break;
                case L'/': out.push_back(L'/'); break;
                case L'b': out.push_back(L'\b'); break;
                case L'f': out.push_back(L'\f'); break;
                case L'n': out.push_back(L'\n'); break;
                case L'r': out.push_back(L'\r'); break;
                case L't': out.push_back(L'\t'); break;
                case L'u': {
                    unsigned cp = 0;
                    if (!ParseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (i_ + 1 < s_.size() && s_[i_] == L'\\' && s_[i_ + 1] == L'u') {
                            i_ += 2;
                            unsigned low = 0;
                            if (!ParseHex4(low)) return false;
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            } else {
                                out.push_back(static_cast<wchar_t>(cp));
                                out.push_back(static_cast<wchar_t>(low));
                                break;
                            }
                        }
                    }
                    if (cp <= 0xFFFF) {
                        out.push_back(static_cast<wchar_t>(cp));
                    } else {
                        cp -= 0x10000;
                        out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
                        out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
                    }
                    break;
                }
                default:
                    return Fail(L"未知的字符串转义");
            }
        }
        return Fail(L"字符串未闭合");
    }

    bool ParseNumber(JsonValue& out) {
        size_t start = i_;
        if (i_ < s_.size() && s_[i_] == L'-') ++i_;
        if (i_ >= s_.size()) return Fail(L"数字不完整");
        if (s_[i_] == L'0') {
            ++i_;
        } else if (s_[i_] >= L'1' && s_[i_] <= L'9') {
            while (i_ < s_.size() && s_[i_] >= L'0' && s_[i_] <= L'9') ++i_;
        } else {
            return Fail(L"数字格式错误");
        }
        if (i_ < s_.size() && s_[i_] == L'.') {
            ++i_;
            if (i_ >= s_.size() || s_[i_] < L'0' || s_[i_] > L'9') return Fail(L"小数部分缺失");
            while (i_ < s_.size() && s_[i_] >= L'0' && s_[i_] <= L'9') ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == L'e' || s_[i_] == L'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == L'+' || s_[i_] == L'-')) ++i_;
            if (i_ >= s_.size() || s_[i_] < L'0' || s_[i_] > L'9') return Fail(L"指数部分缺失");
            while (i_ < s_.size() && s_[i_] >= L'0' && s_[i_] <= L'9') ++i_;
        }
        std::wstring token = s_.substr(start, i_ - start);
        wchar_t* end = nullptr;
        double value = wcstod(token.c_str(), &end);
        if (!end || *end != L'\0') return Fail(L"数字解析失败");
        out.type = JsonValue::Type::Number;
        out.number = value;
        return true;
    }
};

void EscapeJsonString(const std::wstring& s, std::wstring& out) {
    out.push_back(L'"');
    for (wchar_t c : s) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\b': out += L"\\b"; break;
            case L'\f': out += L"\\f"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (static_cast<unsigned>(c) < 0x20) {
                    out += Format(L"\\u%04x", (unsigned)c);
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back(L'"');
}

void DumpValue(const JsonValue& v, std::wstring& out, int depth) {
    auto indent = [&](int d) {
        out.push_back(L'\n');
        out.append((size_t)d * 2, L' ');
    };
    switch (v.type) {
        case JsonValue::Type::Null: out += L"null"; break;
        case JsonValue::Type::Bool: out += v.boolean ? L"true" : L"false"; break;
        case JsonValue::Type::Number: {
            if (std::isfinite(v.number)) {
                if (v.number == (double)(long long)v.number)
                    out += Format(L"%lld", (long long)v.number);
                else
                    out += Format(L"%.10g", v.number);
            } else {
                out += L"0";
            }
            break;
        }
        case JsonValue::Type::String: EscapeJsonString(v.string, out); break;
        case JsonValue::Type::Array: {
            out.push_back(L'[');
            for (size_t i = 0; i < v.array.size(); ++i) {
                if (i) out.push_back(L',');
                indent(depth + 1);
                DumpValue(v.array[i], out, depth + 1);
            }
            if (!v.array.empty()) indent(depth);
            out.push_back(L']');
            break;
        }
        case JsonValue::Type::Object: {
            out.push_back(L'{');
            bool first = true;
            for (const auto& kv : v.object) {
                if (!first) out.push_back(L',');
                first = false;
                indent(depth + 1);
                EscapeJsonString(kv.first, out);
                out += L": ";
                DumpValue(kv.second, out, depth + 1);
            }
            if (!v.object.empty()) indent(depth);
            out.push_back(L'}');
            break;
        }
    }
}

}  // namespace

bool ParseJson(const std::wstring& text, JsonValue& out, std::wstring& error) {
    Parser parser(text);
    out = JsonValue{};
    if (!parser.Parse(out)) {
        error = parser.error();
        return false;
    }
    return true;
}

std::wstring WriteJson(const JsonValue& value) {
    std::wstring out;
    DumpValue(value, out, 0);
    out.push_back(L'\n');
    return out;
}

}  // namespace hf