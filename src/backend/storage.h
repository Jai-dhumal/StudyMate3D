#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#endif
#include "models.h"
#include "sm_json.h"

namespace studymate {

class Storage {
private:
    std::string data_dir;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t p_mutex;
#endif

    std::string get_path(const std::string& filename) const {
        return data_dir + "/" + filename;
    }

    std::string read_file_string(const std::string& path) const {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    bool write_file_atomic(const std::string& path, const std::string& content) {
        std::string temp_path = path + ".tmp";
        {
            std::ofstream file(temp_path, std::ios::trunc);
            if (!file.is_open()) return false;
            file << content;
            file.flush();
        }
#ifdef _WIN32
        return MoveFileExA(temp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        return std::rename(temp_path.c_str(), path.c_str()) == 0;
#endif
    }

public:
    Storage(const std::string& dir = "data") : data_dir(dir) {
#ifdef _WIN32
        InitializeCriticalSection(&cs);
        CreateDirectoryA(data_dir.c_str(), NULL);
#else
        pthread_mutex_init(&p_mutex, NULL);
        mkdir(data_dir.c_str(), 0755);
#endif
    }

    ~Storage() {
#ifdef _WIN32
        DeleteCriticalSection(&cs);
#else
        pthread_mutex_destroy(&p_mutex);
#endif
    }

    void lock() {
#ifdef _WIN32
        EnterCriticalSection(&cs);
#else
        pthread_mutex_lock(&p_mutex);
#endif
    }

    void unlock() {
#ifdef _WIN32
        LeaveCriticalSection(&cs);
#else
        pthread_mutex_unlock(&p_mutex);
#endif
    }

    // --- Users ---
    std::vector<User> get_all_users() {
        lock();
        std::vector<User> list;
        std::string raw = read_file_string(get_path("users.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(User::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_users(const std::vector<User>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json(true));
        }
        lock();
        bool ok = write_file_atomic(get_path("users.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Sessions ---
    std::vector<Session> get_all_sessions() {
        lock();
        std::vector<Session> list;
        std::string raw = read_file_string(get_path("sessions.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(Session::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_sessions(const std::vector<Session>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("sessions.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Subjects ---
    std::vector<Subject> get_all_subjects() {
        lock();
        std::vector<Subject> list;
        std::string raw = read_file_string(get_path("subjects.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(Subject::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_subjects(const std::vector<Subject>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("subjects.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Tasks ---
    std::vector<Task> get_all_tasks() {
        lock();
        std::vector<Task> list;
        std::string raw = read_file_string(get_path("tasks.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(Task::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_tasks(const std::vector<Task>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("tasks.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Study Sessions (Timetable / Scheduled) ---
    std::vector<StudySession> get_all_study_sessions() {
        lock();
        std::vector<StudySession> list;
        std::string raw = read_file_string(get_path("study_sessions.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(StudySession::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_study_sessions(const std::vector<StudySession>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("study_sessions.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Focus Records ---
    std::vector<FocusRecord> get_all_focus_records() {
        lock();
        std::vector<FocusRecord> list;
        std::string raw = read_file_string(get_path("focus_records.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(FocusRecord::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_focus_records(const std::vector<FocusRecord>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("focus_records.json"), arr.dump(2));
        unlock();
        return ok;
    }

    // --- Achievements ---
    std::vector<Achievement> get_all_achievements() {
        lock();
        std::vector<Achievement> list;
        std::string raw = read_file_string(get_path("achievements.json"));
        unlock();
        if (raw.empty()) return list;
        sm_json::Value j = sm_json::parse(raw);
        if (j.is_array()) {
            for (size_t i = 0; i < j.size(); ++i) {
                list.push_back(Achievement::from_json(j[i]));
            }
        }
        return list;
    }

    bool save_all_achievements(const std::vector<Achievement>& list) {
        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t i = 0; i < list.size(); ++i) {
            arr.push_back(list[i].to_json());
        }
        lock();
        bool ok = write_file_atomic(get_path("achievements.json"), arr.dump(2));
        unlock();
        return ok;
    }
};

} // namespace studymate
