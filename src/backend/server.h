#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include "http_types.h"
#include "models.h"
#include "storage.h"
#include "auth.h"
#include "ai_service.h"
#include "progress.h"
#include "sm_json.h"

namespace studymate {

class Server {
private:
    int port;
    std::string public_dir;
    Storage storage;
    AuthService auth;
    AIService ai;
    ProgressService progress;
    SOCKET listen_sock;
    volatile bool running;

    std::string read_file_binary(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

public:
    Server(int p = 8080, const std::string& pub = "public", const std::string& data = "data")
        : port(p), public_dir(pub), storage(data), auth(storage), ai(), progress(storage), listen_sock(INVALID_SOCKET), running(false) {}

    ~Server() {
        stop();
    }

    void stop() {
        running = false;
        if (listen_sock != INVALID_SOCKET) {
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[ERROR] WSAStartup failed.\n";
            return false;
        }
#endif

        listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock == INVALID_SOCKET) {
            std::cerr << "[ERROR] socket creation failed.\n";
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        // Allow port reuse
        int opt = 1;
#ifdef _WIN32
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Bind failed on port " << port << ".\n";
            closesocket(listen_sock);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Listen failed.\n";
            closesocket(listen_sock);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        running = true;
        std::cout << "=========================================================\n";
        std::cout << " StudyMate 3D Backend Running on port " << port << "\n";
        std::cout << " WebGL 3D Universe + AI Study Planner + Focus Engine\n";
        std::cout << "=========================================================\n";

        while (running) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            SOCKET client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
            if (client_sock == INVALID_SOCKET) {
                if (!running) break;
                continue;
            }

            // Spawn worker thread portably
            ClientParam* param = new ClientParam{this, client_sock};
#ifdef _WIN32
            uintptr_t th = _beginthreadex(NULL, 0, &Server::client_thread_entry_win, param, 0, NULL);
            if (th) {
                CloseHandle((HANDLE)th);
            } else {
                delete param;
                closesocket(client_sock);
            }
#else
            pthread_t th;
            if (pthread_create(&th, NULL, &Server::client_thread_entry_posix, param) == 0) {
                pthread_detach(th);
            } else {
                delete param;
                closesocket(client_sock);
            }
#endif
        }
        return true;
    }

private:
    struct ClientParam {
        Server* server;
        SOCKET sock;
    };

#ifdef _WIN32
    static unsigned __stdcall client_thread_entry_win(void* arg) {
        ClientParam* p = static_cast<ClientParam*>(arg);
        p->server->handle_client(p->sock);
        delete p;
        return 0;
    }
#else
    static void* client_thread_entry_posix(void* arg) {
        ClientParam* p = static_cast<ClientParam*>(arg);
        p->server->handle_client(p->sock);
        delete p;
        return NULL;
    }
#endif

    void handle_client(SOCKET sock) {
        std::string raw_req;
        char buffer[8192];
        int bytes = 0;

        // Read request header
        while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';
            raw_req.append(buffer, bytes);
            if (raw_req.find("\r\n\r\n") != std::string::npos) {
                // If content length exists, read remaining body
                size_t cl_pos = raw_req.find("Content-Length:");
                if (cl_pos == std::string::npos) cl_pos = raw_req.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t line_end = raw_req.find("\r\n", cl_pos);
                    std::string cl_str = raw_req.substr(cl_pos + 15, line_end - (cl_pos + 15));
                    try {
                        size_t expected_len = std::stoul(cl_str);
                        size_t header_len = raw_req.find("\r\n\r\n") + 4;
                        while (raw_req.length() < header_len + expected_len) {
                            int more = recv(sock, buffer, sizeof(buffer) - 1, 0);
                            if (more <= 0) break;
                            raw_req.append(buffer, more);
                        }
                    } catch (...) {}
                }
                break;
            }
        }

        if (!raw_req.empty()) {
            HttpRequest req;
            if (HttpUtils::parse_request(raw_req, req)) {
                HttpResponse res = dispatch(req);
                std::string res_str = res.to_http_string();
                send(sock, res_str.c_str(), static_cast<int>(res_str.length()), 0);
            }
        }

        closesocket(sock);
    }

    HttpResponse dispatch(const HttpRequest& req) {
        // Handle CORS Preflight
        if (req.method == "OPTIONS") {
            HttpResponse res;
            res.status_code = 204;
            res.status_text = "No Content";
            return res;
        }

        // Static files or Landing
        if (req.path.substr(0, 5) != "/api/") {
            return serve_static(req.path);
        }

        // --- API ROUTES ---

        // Health check
        if (req.method == "GET" && req.path == "/api/health") {
            sm_json::Value h = sm_json::Value::make_object();
            h["status"] = "healthy";
            h["server"] = "StudyMate-C++-Backend";
            h["ai_available"] = ai.has_api_key();
            return HttpResponse::json(200, h.dump());
        }

        // AUTH: Signup
        if (req.method == "POST" && req.path == "/api/auth/signup") {
            sm_json::Value body = sm_json::parse(req.body);
            std::string name = body["full_name"].as_string();
            std::string email = body["email"].as_string();
            std::string uname = body["username"].as_string();
            std::string pwd = body["password"].as_string();

            User user;
            std::string err;
            if (!auth.signup(name, email, uname, pwd, user, err)) {
                sm_json::Value err_res = sm_json::Value::make_object();
                err_res["error"] = err;
                return HttpResponse::json(400, err_res.dump());
            }

            // Auto-login upon registration
            Session sess;
            auth.login(uname, pwd, user, sess, err);

            sm_json::Value ok = sm_json::Value::make_object();
            ok["message"] = "Account created successfully!";
            ok["user"] = user.to_json();
            ok["token"] = sess.token;

            HttpResponse res = HttpResponse::json(201, ok.dump());
            res.set_cookie("sm_session", sess.token, 7 * 86400, true);
            return res;
        }

        // AUTH: Login
        if (req.method == "POST" && req.path == "/api/auth/login") {
            sm_json::Value body = sm_json::parse(req.body);
            std::string ident = body["username_or_email"].as_string();
            std::string pwd = body["password"].as_string();

            User user;
            Session sess;
            std::string err;
            if (!auth.login(ident, pwd, user, sess, err)) {
                sm_json::Value err_res = sm_json::Value::make_object();
                err_res["error"] = err;
                return HttpResponse::json(401, err_res.dump());
            }

            sm_json::Value ok = sm_json::Value::make_object();
            ok["message"] = "Login successful!";
            ok["user"] = user.to_json();
            ok["token"] = sess.token;

            HttpResponse res = HttpResponse::json(200, ok.dump());
            res.set_cookie("sm_session", sess.token, 7 * 86400, true);
            return res;
        }

        // AUTH: Logout
        if (req.method == "POST" && req.path == "/api/auth/logout") {
            std::string token = req.get_cookie("sm_session");
            auth.logout(token);
            HttpResponse res = HttpResponse::json(200, "{\"message\": \"Logged out successfully\"}");
            res.clear_cookie("sm_session");
            return res;
        }

        // AUTH: Me
        if (req.method == "GET" && req.path == "/api/auth/me") {
            User user;
            if (!auth.authenticate_request(req, user)) {
                return HttpResponse::json(401, "{\"error\": \"Unauthorized. Please log in.\"}");
            }
            sm_json::Value ok = sm_json::Value::make_object();
            ok["user"] = user.to_json();
            return HttpResponse::json(200, ok.dump());
        }

        // All subsequent routes require authentication!
        User user;
        if (!auth.authenticate_request(req, user)) {
            return HttpResponse::json(401, "{\"error\": \"Unauthorized. Session expired or missing.\"}");
        }

        // --- SUBJECTS CRUD ---
        if (req.path == "/api/subjects") {
            if (req.method == "GET") {
                std::vector<Subject> subs = storage.get_all_subjects();
                sm_json::Value arr = sm_json::Value::make_array();
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (subs[i].user_id == user.id) {
                        arr.push_back(subs[i].to_json());
                    }
                }
                return HttpResponse::json(200, arr.dump());
            } else if (req.method == "POST") {
                sm_json::Value b = sm_json::parse(req.body);
                std::string name = b["name"].as_string();
                if (name.empty()) {
                    return HttpResponse::json(400, "{\"error\": \"Subject name is required.\"}");
                }
                Subject s;
                s.id = CryptoUtils::generate_id("sub");
                s.user_id = user.id;
                s.name = name;
                s.description = b["description"].as_string();
                s.color = b["color"].as_string("#6366f1");
                s.target_hours = b["target_hours"].as_double(10.0);
                s.difficulty = b["difficulty"].as_string("Medium");
                s.confidence = b["confidence"].as_int(3);
                s.created_at = CryptoUtils::get_current_iso_time();

                std::vector<Subject> subs = storage.get_all_subjects();
                subs.push_back(s);
                storage.save_all_subjects(subs);
                return HttpResponse::json(201, s.to_json().dump());
            }
        }

        // Single subject: /api/subjects/{id}
        if (req.path.substr(0, 14) == "/api/subjects/") {
            std::string sub_id = req.path.substr(14);
            std::vector<Subject> subs = storage.get_all_subjects();

            if (req.method == "GET") {
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (subs[i].id == sub_id && subs[i].user_id == user.id) {
                        return HttpResponse::json(200, subs[i].to_json().dump());
                    }
                }
                return HttpResponse::json(404, "{\"error\": \"Subject not found.\"}");
            } else if (req.method == "PUT") {
                sm_json::Value b = sm_json::parse(req.body);
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (subs[i].id == sub_id && subs[i].user_id == user.id) {
                        if (b.contains("name")) subs[i].name = b["name"].as_string();
                        if (b.contains("description")) subs[i].description = b["description"].as_string();
                        if (b.contains("color")) subs[i].color = b["color"].as_string();
                        if (b.contains("target_hours")) subs[i].target_hours = b["target_hours"].as_double();
                        if (b.contains("difficulty")) subs[i].difficulty = b["difficulty"].as_string();
                        if (b.contains("confidence")) subs[i].confidence = b["confidence"].as_int();
                        storage.save_all_subjects(subs);
                        return HttpResponse::json(200, subs[i].to_json().dump());
                    }
                }
                return HttpResponse::json(404, "{\"error\": \"Subject not found.\"}");
            } else if (req.method == "DELETE") {
                std::vector<Subject> kept;
                bool found = false;
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (subs[i].id == sub_id && subs[i].user_id == user.id) {
                        found = true;
                    } else {
                        kept.push_back(subs[i]);
                    }
                }
                if (found) {
                    storage.save_all_subjects(kept);
                    return HttpResponse::json(200, "{\"message\": \"Subject deleted successfully.\"}");
                }
                return HttpResponse::json(404, "{\"error\": \"Subject not found.\"}");
            }
        }

        // --- TASKS CRUD ---
        if (req.path == "/api/tasks") {
            if (req.method == "GET") {
                std::vector<Task> tasks = storage.get_all_tasks();
                sm_json::Value arr = sm_json::Value::make_array();
                for (size_t i = 0; i < tasks.size(); ++i) {
                    if (tasks[i].user_id == user.id) {
                        arr.push_back(tasks[i].to_json());
                    }
                }
                return HttpResponse::json(200, arr.dump());
            } else if (req.method == "POST") {
                sm_json::Value b = sm_json::parse(req.body);
                std::string title = b["title"].as_string();
                if (title.empty()) {
                    return HttpResponse::json(400, "{\"error\": \"Task title is required.\"}");
                }
                Task t;
                t.id = CryptoUtils::generate_id("tsk");
                t.user_id = user.id;
                t.subject_id = b["subject_id"].as_string();
                t.title = title;
                t.description = b["description"].as_string();
                t.due_date = b["due_date"].as_string();
                t.start_time = b["start_time"].as_string();
                t.estimated_minutes = b["estimated_minutes"].as_int(45);
                t.priority = b["priority"].as_string("Medium");
                t.status = b["status"].as_string("Pending");
                t.created_at = CryptoUtils::get_current_iso_time();

                std::vector<Task> tasks = storage.get_all_tasks();
                tasks.push_back(t);
                storage.save_all_tasks(tasks);
                return HttpResponse::json(201, t.to_json().dump());
            }
        }

        // PATCH task status: /api/tasks/{id}/status
        if (req.method == "PATCH" && req.path.find("/status") != std::string::npos && req.path.substr(0, 11) == "/api/tasks/") {
            size_t s_pos = req.path.find("/status");
            std::string task_id = req.path.substr(11, s_pos - 11);
            sm_json::Value b = sm_json::parse(req.body);
            std::string new_status = b["status"].as_string();

            std::vector<Task> tasks = storage.get_all_tasks();
            for (size_t i = 0; i < tasks.size(); ++i) {
                if (tasks[i].id == task_id && tasks[i].user_id == user.id) {
                    tasks[i].status = new_status;
                    if (new_status == "Completed") {
                        tasks[i].completed_at = CryptoUtils::get_current_iso_time();
                    }
                    storage.save_all_tasks(tasks);
                    return HttpResponse::json(200, tasks[i].to_json().dump());
                }
            }
            return HttpResponse::json(404, "{\"error\": \"Task not found.\"}");
        }

        // Single task: /api/tasks/{id}
        if (req.path.substr(0, 11) == "/api/tasks/") {
            std::string task_id = req.path.substr(11);
            std::vector<Task> tasks = storage.get_all_tasks();

            if (req.method == "GET") {
                for (size_t i = 0; i < tasks.size(); ++i) {
                    if (tasks[i].id == task_id && tasks[i].user_id == user.id) {
                        return HttpResponse::json(200, tasks[i].to_json().dump());
                    }
                }
                return HttpResponse::json(404, "{\"error\": \"Task not found.\"}");
            } else if (req.method == "PUT") {
                sm_json::Value b = sm_json::parse(req.body);
                for (size_t i = 0; i < tasks.size(); ++i) {
                    if (tasks[i].id == task_id && tasks[i].user_id == user.id) {
                        if (b.contains("title")) tasks[i].title = b["title"].as_string();
                        if (b.contains("subject_id")) tasks[i].subject_id = b["subject_id"].as_string();
                        if (b.contains("description")) tasks[i].description = b["description"].as_string();
                        if (b.contains("due_date")) tasks[i].due_date = b["due_date"].as_string();
                        if (b.contains("start_time")) tasks[i].start_time = b["start_time"].as_string();
                        if (b.contains("estimated_minutes")) tasks[i].estimated_minutes = b["estimated_minutes"].as_int();
                        if (b.contains("priority")) tasks[i].priority = b["priority"].as_string();
                        if (b.contains("status")) {
                            tasks[i].status = b["status"].as_string();
                            if (tasks[i].status == "Completed" && tasks[i].completed_at.empty()) {
                                tasks[i].completed_at = CryptoUtils::get_current_iso_time();
                            }
                        }
                        storage.save_all_tasks(tasks);
                        return HttpResponse::json(200, tasks[i].to_json().dump());
                    }
                }
                return HttpResponse::json(404, "{\"error\": \"Task not found.\"}");
            } else if (req.method == "DELETE") {
                std::vector<Task> kept;
                bool found = false;
                for (size_t i = 0; i < tasks.size(); ++i) {
                    if (tasks[i].id == task_id && tasks[i].user_id == user.id) {
                        found = true;
                    } else {
                        kept.push_back(tasks[i]);
                    }
                }
                if (found) {
                    storage.save_all_tasks(kept);
                    return HttpResponse::json(200, "{\"message\": \"Task deleted successfully.\"}");
                }
                return HttpResponse::json(404, "{\"error\": \"Task not found.\"}");
            }
        }

        // --- AI CONFIG & STATUS ---
        if (req.method == "GET" && req.path == "/api/ai/config") {
            sm_json::Value resp = sm_json::Value::make_object();
            resp["has_key"] = ai.has_api_key();
            resp["masked_key"] = ai.get_masked_key();
            resp["active_engine"] = ai.has_api_key() ? "Google Gemini AI (1.5 Flash)" : "StudyMate Cognitive AI Engine (Adaptive)";
            return HttpResponse::json(200, resp.dump());
        }

        if (req.method == "POST" && req.path == "/api/ai/config") {
            sm_json::Value b = sm_json::parse(req.body);
            std::string new_key = b["api_key"].as_string();
            // Trim whitespace
            while (!new_key.empty() && (new_key[0] == ' ' || new_key[0] == '\t')) new_key.erase(0, 1);
            while (!new_key.empty() && (new_key.back() == ' ' || new_key.back() == '\t')) new_key.pop_back();
            
            ai.set_api_key(new_key);
            sm_json::Value resp = sm_json::Value::make_object();
            resp["success"] = true;
            resp["has_key"] = ai.has_api_key();
            resp["masked_key"] = ai.get_masked_key();
            resp["active_engine"] = ai.has_api_key() ? "Google Gemini AI (1.5 Flash)" : "StudyMate Cognitive AI Engine (Adaptive)";
            return HttpResponse::json(200, resp.dump());
        }

        // --- AI PLANNER ---
        if (req.method == "POST" && req.path == "/api/ai/plan") {
            sm_json::Value b = sm_json::parse(req.body);
            PlanRequestInput pin;
            pin.prompt = b["prompt"].as_string();
            pin.daily_hours_limit = b["daily_hours_limit"].as_int(4);
            pin.preferred_session_min = b["preferred_session_min"].as_int(45);
            pin.start_time_pref = b["start_time_pref"].as_string("09:00");
            pin.focus_goal = b["focus_goal"].as_string();

            std::vector<Subject> all_subs = storage.get_all_subjects();
            std::vector<Subject> user_subs;
            for (size_t i = 0; i < all_subs.size(); ++i) {
                if (all_subs[i].user_id == user.id) user_subs.push_back(all_subs[i]);
            }

            std::vector<Task> all_tasks = storage.get_all_tasks();
            std::vector<Task> user_tasks;
            for (size_t i = 0; i < all_tasks.size(); ++i) {
                if (all_tasks[i].user_id == user.id) user_tasks.push_back(all_tasks[i]);
            }

            PlanResult plan = ai.generate_ai_plan(pin, user_subs, user_tasks, CryptoUtils::get_current_date());
            return HttpResponse::json(200, plan.to_json().dump());
        }

        // Accept AI plan & save sessions
        if (req.method == "POST" && req.path == "/api/ai/plan/accept") {
            sm_json::Value b = sm_json::parse(req.body);
            if (!b["sessions"].is_array()) {
                return HttpResponse::json(400, "{\"error\": \"Invalid sessions array in payload.\"}");
            }
            std::vector<StudySession> all_sess = storage.get_all_study_sessions();
            sm_json::Value arr = b["sessions"];
            int saved_count = 0;

            for (size_t i = 0; i < arr.size(); ++i) {
                StudySession s;
                s.id = CryptoUtils::generate_id("ses");
                s.user_id = user.id;
                s.subject_id = arr[i]["subject_id"].as_string();
                s.task_id = arr[i]["task_id"].as_string();
                s.date = arr[i]["date"].as_string();
                s.start_time = arr[i]["start_time"].as_string();
                s.end_time = arr[i]["end_time"].as_string();
                s.duration_minutes = arr[i]["duration_minutes"].as_int(45);
                s.activity = arr[i]["activity"].as_string();
                s.priority = arr[i]["priority"].as_string("Medium");
                s.reason = arr[i]["reason"].as_string();
                s.is_completed = false;
                s.created_at = CryptoUtils::get_current_iso_time();
                all_sess.push_back(s);
                saved_count++;
            }
            storage.save_all_study_sessions(all_sess);

            sm_json::Value res_j = sm_json::Value::make_object();
            res_j["message"] = "Plan saved to your active timetable!";
            res_j["saved_count"] = saved_count;
            return HttpResponse::json(200, res_j.dump());
        }

        // --- STUDY SESSIONS / TIMETABLE ---
        if (req.path == "/api/planner/sessions") {
            if (req.method == "GET") {
                std::vector<StudySession> list = storage.get_all_study_sessions();
                sm_json::Value arr = sm_json::Value::make_array();
                for (size_t i = 0; i < list.size(); ++i) {
                    if (list[i].user_id == user.id) {
                        arr.push_back(list[i].to_json());
                    }
                }
                return HttpResponse::json(200, arr.dump());
            } else if (req.method == "POST") {
                sm_json::Value b = sm_json::parse(req.body);
                StudySession s = StudySession::from_json(b);
                s.id = CryptoUtils::generate_id("ses");
                s.user_id = user.id;
                s.created_at = CryptoUtils::get_current_iso_time();
                std::vector<StudySession> list = storage.get_all_study_sessions();
                list.push_back(s);
                storage.save_all_study_sessions(list);
                return HttpResponse::json(201, s.to_json().dump());
            }
        }

        if (req.path.substr(0, 22) == "/api/planner/sessions/") {
            std::string sid = req.path.substr(22);
            std::vector<StudySession> list = storage.get_all_study_sessions();
            if (req.method == "DELETE") {
                std::vector<StudySession> kept;
                bool found = false;
                for (size_t i = 0; i < list.size(); ++i) {
                    if (list[i].id == sid && list[i].user_id == user.id) {
                        found = true;
                    } else {
                        kept.push_back(list[i]);
                    }
                }
                if (found) {
                    storage.save_all_study_sessions(kept);
                    return HttpResponse::json(200, "{\"message\": \"Session deleted.\"}");
                }
                return HttpResponse::json(404, "{\"error\": \"Session not found.\"}");
            } else if (req.method == "PATCH" || req.method == "PUT") {
                sm_json::Value b = sm_json::parse(req.body);
                for (size_t i = 0; i < list.size(); ++i) {
                    if (list[i].id == sid && list[i].user_id == user.id) {
                        if (b.contains("is_completed")) list[i].is_completed = b["is_completed"].as_bool();
                        if (b.contains("start_time")) list[i].start_time = b["start_time"].as_string();
                        if (b.contains("end_time")) list[i].end_time = b["end_time"].as_string();
                        if (b.contains("date")) list[i].date = b["date"].as_string();
                        storage.save_all_study_sessions(list);
                        return HttpResponse::json(200, list[i].to_json().dump());
                    }
                }
                return HttpResponse::json(404, "{\"error\": \"Session not found.\"}");
            }
        }

        // --- FOCUS TIMER ---
        if (req.path == "/api/focus/sessions") {
            if (req.method == "POST") {
                sm_json::Value b = sm_json::parse(req.body);
                FocusRecord fr;
                fr.id = CryptoUtils::generate_id("foc");
                fr.user_id = user.id;
                fr.subject_id = b["subject_id"].as_string();
                fr.task_id = b["task_id"].as_string();
                fr.duration_minutes = b["duration_minutes"].as_int(25);
                fr.mode = b["mode"].as_string("pomodoro");
                fr.timestamp = CryptoUtils::get_current_iso_time();

                std::vector<FocusRecord> recs = storage.get_all_focus_records();
                recs.push_back(fr);
                storage.save_all_focus_records(recs);

                return HttpResponse::json(201, fr.to_json().dump());
            } else if (req.method == "GET") {
                std::vector<FocusRecord> recs = storage.get_all_focus_records();
                sm_json::Value arr = sm_json::Value::make_array();
                for (size_t i = 0; i < recs.size(); ++i) {
                    if (recs[i].user_id == user.id) {
                        arr.push_back(recs[i].to_json());
                    }
                }
                return HttpResponse::json(200, arr.dump());
            }
        }

        // --- PROGRESS & ACHIEVEMENTS ---
        if (req.method == "GET" && req.path == "/api/progress/summary") {
            sm_json::Value summary = progress.get_summary(user.id);
            return HttpResponse::json(200, summary.dump());
        }

        if (req.method == "GET" && req.path == "/api/progress/subjects") {
            sm_json::Value sub_prog = progress.get_subject_progress(user.id);
            return HttpResponse::json(200, sub_prog.dump());
        }

        if (req.method == "GET" && req.path == "/api/achievements") {
            std::vector<Achievement> all_a = storage.get_all_achievements();
            sm_json::Value arr = sm_json::Value::make_array();
            for (size_t i = 0; i < all_a.size(); ++i) {
                if (all_a[i].user_id == user.id) {
                    arr.push_back(all_a[i].to_json());
                }
            }
            return HttpResponse::json(200, arr.dump());
        }

        return HttpResponse::json(404, "{\"error\": \"Endpoint not found.\"}");
    }

    HttpResponse serve_static(const std::string& raw_path) {
        std::string p = raw_path;
        if (p == "/" || p.empty()) p = "/index.html";

        // Security check against directory traversal
        if (p.find("..") != std::string::npos) {
            return HttpResponse::json(403, "{\"error\": \"Forbidden\"}");
        }

        std::string local_path = public_dir + p;
        std::string content = read_file_binary(local_path);
        if (content.empty()) {
            // SPA fallback to index.html if file doesn't have an extension like .js or .css
            if (p.find('.') == std::string::npos) {
                content = read_file_binary(public_dir + "/index.html");
                if (!content.empty()) {
                    return HttpResponse::file(200, "text/html; charset=utf-8", content);
                }
            }
            return HttpResponse::html(404, "<h1>404 Not Found - StudyMate 3D</h1>");
        }

        std::string mime = HttpUtils::get_mime_type(p);
        return HttpResponse::file(200, mime, content);
    }
};

} // namespace studymate
