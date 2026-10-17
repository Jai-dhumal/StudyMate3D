#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace sm_json {

enum Type {
    NUL,
    BOOL,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
};

class Value {
public:
    Type type;
    bool bool_val;
    double num_val;
    std::string str_val;
    std::vector<Value> arr_val;
    std::map<std::string, Value> obj_val;

    Value() : type(NUL), bool_val(false), num_val(0.0) {}
    Value(bool b) : type(BOOL), bool_val(b), num_val(0.0) {}
    Value(int n) : type(NUMBER), bool_val(false), num_val(n) {}
    Value(long n) : type(NUMBER), bool_val(false), num_val((double)n) {}
    Value(long long n) : type(NUMBER), bool_val(false), num_val((double)n) {}
    Value(double n) : type(NUMBER), bool_val(false), num_val(n) {}
    Value(const char* s) : type(STRING), bool_val(false), num_val(0.0), str_val(s ? s : "") {}
    Value(const std::string& s) : type(STRING), bool_val(false), num_val(0.0), str_val(s) {}

    static Value make_object() {
        Value v;
        v.type = OBJECT;
        return v;
    }

    static Value make_array() {
        Value v;
        v.type = ARRAY;
        return v;
    }

    bool is_null() const { return type == NUL; }
    bool is_bool() const { return type == BOOL; }
    bool is_number() const { return type == NUMBER; }
    bool is_string() const { return type == STRING; }
    bool is_array() const { return type == ARRAY; }
    bool is_object() const { return type == OBJECT; }

    bool as_bool(bool def = false) const {
        if (type == BOOL) return bool_val;
        if (type == NUMBER) return num_val != 0.0;
        if (type == STRING) return str_val == "true" || str_val == "1";
        return def;
    }

    int as_int(int def = 0) const {
        if (type == NUMBER) return static_cast<int>(num_val);
        if (type == STRING) {
            try { return std::stoi(str_val); } catch (...) {}
        }
        return def;
    }

    double as_double(double def = 0.0) const {
        if (type == NUMBER) return num_val;
        if (type == STRING) {
            try { return std::stod(str_val); } catch (...) {}
        }
        return def;
    }

    std::string as_string(const std::string& def = "") const {
        if (type == STRING) return str_val;
        if (type == NUMBER) {
            std::ostringstream ss;
            ss << num_val;
            return ss.str();
        }
        if (type == BOOL) return bool_val ? "true" : "false";
        return def;
    }

    bool contains(const std::string& key) const {
        if (type != OBJECT) return false;
        return obj_val.find(key) != obj_val.end();
    }

    Value& operator[](const std::string& key) {
        if (type != OBJECT) {
            type = OBJECT;
            obj_val.clear();
        }
        return obj_val[key];
    }

    const Value& operator[](const std::string& key) const {
        static Value null_val;
        if (type != OBJECT) return null_val;
        auto it = obj_val.find(key);
        if (it != obj_val.end()) return it->second;
        return null_val;
    }

    Value& operator[](size_t idx) {
        if (type != ARRAY) {
            type = ARRAY;
            arr_val.clear();
        }
        if (idx >= arr_val.size()) {
            arr_val.resize(idx + 1);
        }
        return arr_val[idx];
    }

    const Value& operator[](size_t idx) const {
        static Value null_val;
        if (type != ARRAY || idx >= arr_val.size()) return null_val;
        return arr_val[idx];
    }

    void push_back(const Value& v) {
        if (type != ARRAY) {
            type = ARRAY;
            arr_val.clear();
        }
        arr_val.push_back(v);
    }

    size_t size() const {
        if (type == ARRAY) return arr_val.size();
        if (type == OBJECT) return obj_val.size();
        if (type == STRING) return str_val.size();
        return 0;
    }

    std::string dump(int indent = -1, int current_indent = 0) const {
        std::ostringstream ss;
        std::string ind = (indent >= 0) ? std::string(current_indent, ' ') : "";
        std::string next_ind = (indent >= 0) ? std::string(current_indent + indent, ' ') : "";
        std::string nl = (indent >= 0) ? "\n" : "";
        std::string sp = (indent >= 0) ? " " : "";

        switch (type) {
            case NUL:
                ss << "null";
                break;
            case BOOL:
                ss << (bool_val ? "true" : "false");
                break;
            case NUMBER: {
                if (num_val == static_cast<long long>(num_val)) {
                    ss << static_cast<long long>(num_val);
                } else {
                    ss << std::setprecision(10) << num_val;
                }
                break;
            }
            case STRING: {
                ss << "\"";
                for (char c : str_val) {
                    switch (c) {
                        case '"': ss << "\\\""; break;
                        case '\\': ss << "\\\\"; break;
                        case '\b': ss << "\\b"; break;
                        case '\f': ss << "\\f"; break;
                        case '\n': ss << "\\n"; break;
                        case '\r': ss << "\\r"; break;
                        case '\t': ss << "\\t"; break;
                        default:
                            if (static_cast<unsigned char>(c) < 0x20) {
                                ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                            } else {
                                ss << c;
                            }
                    }
                }
                ss << "\"";
                break;
            }
            case ARRAY: {
                if (arr_val.empty()) {
                    ss << "[]";
                } else {
                    ss << "[" << nl;
                    for (size_t i = 0; i < arr_val.size(); ++i) {
                        ss << next_ind << arr_val[i].dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (i + 1 < arr_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << ind << "]";
                }
                break;
            }
            case OBJECT: {
                if (obj_val.empty()) {
                    ss << "{}";
                } else {
                    ss << "{" << nl;
                    size_t count = 0;
                    for (auto it = obj_val.begin(); it != obj_val.end(); ++it, ++count) {
                        ss << next_ind << "\"" << it->first << "\":" << sp;
                        ss << it->second.dump(indent, current_indent + (indent >= 0 ? indent : 0));
                        if (count + 1 < obj_val.size()) ss << ",";
                        ss << nl;
                    }
                    ss << ind << "}";
                }
                break;
            }
        }
        return ss.str();
    }
};

// Parser implementation
class Parser {
private:
    const std::string& src;
    size_t pos;

    void skip_whitespace() {
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos++;
            } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                // skip single line comment
                while (pos < src.size() && src[pos] != '\n') pos++;
            } else {
                break;
            }
        }
    }

    char peek() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos];
    }

    char get() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos++];
    }

    bool match(char expected) {
        skip_whitespace();
        if (pos < src.size() && src[pos] == expected) {
            pos++;
            return true;
        }
        return false;
    }

public:
    Parser(const std::string& s) : src(s), pos(0) {}

    Value parse() {
        skip_whitespace();
        if (pos >= src.size()) return Value();
        return parse_value();
    }

private:
    Value parse_value() {
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        return Value();
    }

    Value parse_object() {
        Value obj = Value::make_object();
        get(); // consume '{'
        skip_whitespace();
        if (peek() == '}') {
            get();
            return obj;
        }
        while (pos < src.size()) {
            skip_whitespace();
            if (peek() != '"') break;
            Value key = parse_string();
            skip_whitespace();
            if (!match(':')) break;
            Value val = parse_value();
            obj[key.as_string()] = val;
            skip_whitespace();
            if (match('}')) return obj;
            if (!match(',')) break;
        }
        return obj;
    }

    Value parse_array() {
        Value arr = Value::make_array();
        get(); // consume '['
        skip_whitespace();
        if (peek() == ']') {
            get();
            return arr;
        }
        while (pos < src.size()) {
            Value val = parse_value();
            arr.push_back(val);
            skip_whitespace();
            if (match(']')) return arr;
            if (!match(',')) break;
        }
        return arr;
    }

    Value parse_string() {
        get(); // consume '"'
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') {
                return Value(s);
            }
            if (c == '\\') {
                if (pos >= src.size()) break;
                char esc = src[pos++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (pos + 4 <= src.size()) {
                            std::string hex_str = src.substr(pos, 4);
                            pos += 4;
                            int code = std::stoi(hex_str, nullptr, 16);
                            if (code < 128) s += (char)code;
                            else s += '?';
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        return Value(s);
    }

    Value parse_number() {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
            pos++;
        }
        std::string num_str = src.substr(start, pos - start);
        try {
            double d = std::stod(num_str);
            return Value(d);
        } catch (...) {
            return Value(0.0);
        }
    }

    Value parse_bool() {
        if (src.substr(pos, 4) == "true") {
            pos += 4;
            return Value(true);
        }
        if (src.substr(pos, 5) == "false") {
            pos += 5;
            return Value(false);
        }
        return Value(false);
    }

    Value parse_null() {
        if (src.substr(pos, 4) == "null") {
            pos += 4;
        }
        return Value();
    }
};

inline Value parse(const std::string& str) {
    Parser p(str);
    return p.parse();
}

} // namespace sm_json
