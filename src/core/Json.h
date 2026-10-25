// Hearizon Sync C.R.V. - minimal JSON
// 只实现配置与 API 需要的那一小部分：对象、数组、字符串、数字、布尔、null。
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hs {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool b);
    Json(double d);
    Json(int i);
    Json(const char* s);
    Json(const std::string& s);

    static Json array();
    static Json object();

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }

    // 读取：不存在时返回默认值，绝不抛异常
    bool asBool(bool def = false) const;
    double asNumber(double def = 0.0) const;
    int asInt(int def = 0) const;
    std::string asString(const std::string& def = "") const;

    // 对象访问
    Json& operator[](const std::string& key);
    const Json* find(const std::string& key) const;
    bool has(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& def = "") const;
    bool getBool(const std::string& key, bool def = false) const;
    int getInt(const std::string& key, int def = 0) const;
    double getNumber(const std::string& key, double def = 0.0) const;

    // 数组访问
    void push(const Json& v);
    size_t size() const;
    const Json* at(size_t i) const;

    // 遍历对象成员（按 key 有序）
    template <typename Fn>
    void forEachObject(Fn&& fn) const {
        for (const auto& kv : obj_) fn(kv.first, kv.second);
    }

    // 序列化
    std::string dump(int indent = -1) const;

    // 解析：失败返回 Null
    static Json parse(const std::string& text);

    // 便捷：对象写入
    void set(const std::string& key, const Json& v) { (*this)[key] = v; }
    void set(const std::string& key, const std::string& v) { (*this)[key] = Json(v); }
    void set(const std::string& key, const char* v) { (*this)[key] = Json(v); }
    void set(const std::string& key, bool v) { (*this)[key] = Json(v); }
    void set(const std::string& key, int v) { (*this)[key] = Json(v); }
    void set(const std::string& key, double v) { (*this)[key] = Json(v); }

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> arr_;
    std::map<std::string, Json> obj_;
};

}  // namespace hs
