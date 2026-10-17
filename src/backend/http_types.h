#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace studymate {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> cookies;
    std::map<std::string, std::string> query_params;
    std::string body;

    std::string get_header(const std::string& key) const {
        std::string lower_k = key;
        std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(), ::tolower);
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            std::string k = it->first;
            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            if (k == lower_k) return it->second;
        }
        return "";
    }

    std::string get_cookie(const std::string& name) const {
        auto it = cookies.find(name);
        if (it != cookies.end()) return it->second;
        return "";
    }
};

struct HttpResponse {
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::vector<std::string> set_cookies;
    std::string body;

    HttpResponse() : status_code(200), status_text("OK") {
        headers["Content-Type"] = "application/json; charset=utf-8";
        headers["Access-Control-Allow-Origin"] = "*";
        headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, PATCH, DELETE, OPTIONS";
        headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, X-Requested-With";
    }

    static HttpResponse json(int code, const std::string& json_str) {
        HttpResponse res;
        res.status_code = code;
        res.status_text = (code == 200 ? "OK" : (code == 201 ? "Created" : (code == 400 ? "Bad Request" : (code == 401 ? "Unauthorized" : (code == 403 ? "Forbidden" : (code == 404 ? "Not Found" : "Error"))))));
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        res.body = json_str;
        return res;
    }

    static HttpResponse html(int code, const std::string& html_str) {
        HttpResponse res;
        res.status_code = code;
        res.status_text = "OK";
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        res.body = html_str;
        return res;
    }

    static HttpResponse file(int code, const std::string& mime, const std::string& content) {
        HttpResponse res;
        res.status_code = code;
        res.status_text = "OK";
        res.headers["Content-Type"] = mime;
        res.body = content;
        return res;
    }

    void set_cookie(const std::string& name, const std::string& value, int max_age = 86400, bool http_only = true) {
        std::ostringstream ss;
        ss << name << "=" << value << "; Path=/; Max-Age=" << max_age << "; SameSite=Lax";
        if (http_only) {
            ss << "; HttpOnly";
        }
        set_cookies.push_back(ss.str());
    }

    void clear_cookie(const std::string& name) {
        std::string s = name + "=; Path=/; Max-Age=0; HttpOnly";
        set_cookies.push_back(s);
    }

    std::string to_http_string() const {
        std::ostringstream ss;
        ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            ss << it->first << ": " << it->second << "\r\n";
        }
        for (size_t i = 0; i < set_cookies.size(); ++i) {
            ss << "Set-Cookie: " << set_cookies[i] << "\r\n";
        }
        ss << "Content-Length: " << body.size() << "\r\n";
        ss << "Connection: close\r\n\r\n";
        ss << body;
        return ss.str();
    }
};

class HttpUtils {
public:
    static std::string url_decode(const std::string& in) {
        std::string out;
        for (size_t i = 0; i < in.length(); ++i) {
            if (in[i] == '+') {
                out += ' ';
            } else if (in[i] == '%' && i + 2 < in.length()) {
                std::string hs = in.substr(i + 1, 2);
                try {
                    int c = std::stoi(hs, nullptr, 16);
                    out += static_cast<char>(c);
                    i += 2;
                } catch (...) {
                    out += in[i];
                }
            } else {
                out += in[i];
            }
        }
        return out;
    }

    static bool parse_request(const std::string& raw, HttpRequest& req) {
        size_t header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos) return false;

        std::string header_part = raw.substr(0, header_end);
        req.body = raw.substr(header_end + 4);

        std::istringstream stream(header_part);
        std::string line;
        if (!std::getline(stream, line)) return false;
        if (!line.empty() && line[line.size() - 1] == '\r') line.pop_back();

        // Parse Request line: METHOD /path?query HTTP/1.1
        size_t m_end = line.find(' ');
        if (m_end == std::string::npos) return false;
        req.method = line.substr(0, m_end);

        size_t p_end = line.find(' ', m_end + 1);
        if (p_end == std::string::npos) return false;
        std::string raw_path = line.substr(m_end + 1, p_end - (m_end + 1));

        size_t q_pos = raw_path.find('?');
        if (q_pos != std::string::npos) {
            req.path = raw_path.substr(0, q_pos);
            req.query_string = raw_path.substr(q_pos + 1);
            // Parse query params
            std::istringstream qs(req.query_string);
            std::string pair;
            while (std::getline(qs, pair, '&')) {
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.query_params[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
                } else if (!pair.empty()) {
                    req.query_params[url_decode(pair)] = "";
                }
            }
        } else {
            req.path = raw_path;
        }

        // Headers
        while (std::getline(stream, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') line.pop_back();
            if (line.empty()) continue;
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string k = line.substr(0, colon);
                std::string v = line.substr(colon + 1);
                // trim
                while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
                while (!v.empty() && (v[v.size() - 1] == ' ' || v[v.size() - 1] == '\t')) v.pop_back();
                req.headers[k] = v;

                // Cookies
                std::string lower_k = k;
                std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(), ::tolower);
                if (lower_k == "cookie") {
                    std::istringstream cs(v);
                    std::string c_pair;
                    while (std::getline(cs, c_pair, ';')) {
                        while (!c_pair.empty() && c_pair[0] == ' ') c_pair.erase(0, 1);
                        size_t eq = c_pair.find('=');
                        if (eq != std::string::npos) {
                            req.cookies[c_pair.substr(0, eq)] = c_pair.substr(eq + 1);
                        }
                    }
                }
            }
        }
        return true;
    }

    static std::string get_mime_type(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot == std::string::npos) return "text/plain";
        std::string ext = path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
        if (ext == "css") return "text/css; charset=utf-8";
        if (ext == "js") return "application/javascript; charset=utf-8";
        if (ext == "json") return "application/json; charset=utf-8";
        if (ext == "png") return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "svg") return "image/svg+xml";
        if (ext == "ico") return "image/x-icon";
        if (ext == "woff2") return "font/woff2";
        if (ext == "woff") return "font/woff";
        if (ext == "ttf") return "font/ttf";
        return "text/plain";
    }
};

} // namespace studymate
