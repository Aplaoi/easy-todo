#pragma once

#include "common.h"

namespace hf {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::wstring string;
    std::vector<JsonValue> array;
    std::map<std::wstring, JsonValue> object;

    bool IsNull() const { return type == Type::Null; }
    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }

    const JsonValue* Find(const wchar_t* key) const {
        if (type != Type::Object) return nullptr;
        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }
    std::wstring GetString(const wchar_t* key, const std::wstring& def = L"") const {
        const JsonValue* v = Find(key);
        return v && v->type == Type::String ? v->string : def;
    }
    double GetNumber(const wchar_t* key, double def = 0.0) const {
        const JsonValue* v = Find(key);
        return v && v->type == Type::Number ? v->number : def;
    }
    bool GetBool(const wchar_t* key, bool def = false) const {
        const JsonValue* v = Find(key);
        return v && v->type == Type::Bool ? v->boolean : def;
    }
    void Set(const std::wstring& key, JsonValue value) { object[key] = std::move(value); }
};

JsonValue JsonObject();
JsonValue JsonArray();
JsonValue JsonStr(const std::wstring& s);
JsonValue JsonNum(double n);
JsonValue JsonBool(bool b);

bool ParseJson(const std::wstring& text, JsonValue& out, std::wstring& error);
std::wstring WriteJson(const JsonValue& value);

}  // namespace hf