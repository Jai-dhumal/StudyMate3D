#pragma once
#include <string>
#include <vector>
#include "sm_json.h"

namespace studymate {

struct User {
    std::string id;
    std::string full_name;
    std::string email;
    std::string username;
    std::string password_hash; // salt$hash
    std::string created_at;

    sm_json::Value to_json(bool include_sensitive = false) const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["full_name"] = full_name;
        v["email"] = email;
        v["username"] = username;
        v["created_at"] = created_at;
        if (include_sensitive) {
            v["password_hash"] = password_hash;
        }
        return v;
    }

    static User from_json(const sm_json::Value& v) {
        User u;
        u.id = v["id"].as_string();
        u.full_name = v["full_name"].as_string();
        u.email = v["email"].as_string();
        u.username = v["username"].as_string();
        u.password_hash = v["password_hash"].as_string();
        u.created_at = v["created_at"].as_string();
        return u;
    }
};

struct Session {
    std::string token;
    std::string user_id;
    long long created_at;
    long long expires_at;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["token"] = token;
        v["user_id"] = user_id;
        v["created_at"] = created_at;
        v["expires_at"] = expires_at;
        return v;
    }

    static Session from_json(const sm_json::Value& v) {
        Session s;
        s.token = v["token"].as_string();
        s.user_id = v["user_id"].as_string();
        s.created_at = static_cast<long long>(v["created_at"].as_double());
        s.expires_at = static_cast<long long>(v["expires_at"].as_double());
        return s;
    }
};

struct Subject {
    std::string id;
    std::string user_id;
    std::string name;
    std::string description;
    std::string color;
    double target_hours;
    std::string difficulty; // "Easy", "Medium", "Hard"
    int confidence; // 1-5
    std::string created_at;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["user_id"] = user_id;
        v["name"] = name;
        v["description"] = description;
        v["color"] = color;
        v["target_hours"] = target_hours;
        v["difficulty"] = difficulty.empty() ? "Medium" : difficulty;
        v["confidence"] = confidence > 0 ? confidence : 3;
        v["created_at"] = created_at;
        return v;
    }

    static Subject from_json(const sm_json::Value& v) {
        Subject s;
        s.id = v["id"].as_string();
        s.user_id = v["user_id"].as_string();
        s.name = v["name"].as_string();
        s.description = v["description"].as_string();
        s.color = v["color"].as_string("#6366f1");
        s.target_hours = v["target_hours"].as_double(10.0);
        s.difficulty = v["difficulty"].as_string("Medium");
        s.confidence = v["confidence"].as_int(3);
        s.created_at = v["created_at"].as_string();
        return s;
    }
};

struct Task {
    std::string id;
    std::string user_id;
    std::string subject_id;
    std::string title;
    std::string description;
    std::string due_date;       // YYYY-MM-DD
    std::string start_time;     // HH:MM (optional)
    int estimated_minutes;      // e.g. 45
    std::string priority;       // "Low", "Medium", "High"
    std::string status;         // "Pending", "In Progress", "Completed"
    std::string created_at;
    std::string completed_at;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["user_id"] = user_id;
        v["subject_id"] = subject_id;
        v["title"] = title;
        v["description"] = description;
        v["due_date"] = due_date;
        v["start_time"] = start_time;
        v["estimated_minutes"] = estimated_minutes;
        v["priority"] = priority;
        v["status"] = status;
        v["created_at"] = created_at;
        v["completed_at"] = completed_at;
        return v;
    }

    static Task from_json(const sm_json::Value& v) {
        Task t;
        t.id = v["id"].as_string();
        t.user_id = v["user_id"].as_string();
        t.subject_id = v["subject_id"].as_string();
        t.title = v["title"].as_string();
        t.description = v["description"].as_string();
        t.due_date = v["due_date"].as_string();
        t.start_time = v["start_time"].as_string();
        t.estimated_minutes = v["estimated_minutes"].as_int(30);
        t.priority = v["priority"].as_string("Medium");
        t.status = v["status"].as_string("Pending");
        t.created_at = v["created_at"].as_string();
        t.completed_at = v["completed_at"].as_string();
        return t;
    }
};

struct StudySession {
    std::string id;
    std::string user_id;
    std::string subject_id;
    std::string task_id;
    std::string date;           // YYYY-MM-DD
    std::string start_time;     // HH:MM
    std::string end_time;       // HH:MM
    int duration_minutes;
    std::string activity;       // "Reading", "Problem Solving", "Revision", etc.
    std::string priority;       // "High", "Medium", "Low"
    std::string reason;         // Reason AI or student scheduled it
    bool is_completed;
    std::string created_at;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["user_id"] = user_id;
        v["subject_id"] = subject_id;
        v["task_id"] = task_id;
        v["date"] = date;
        v["start_time"] = start_time;
        v["end_time"] = end_time;
        v["duration_minutes"] = duration_minutes;
        v["activity"] = activity;
        v["priority"] = priority;
        v["reason"] = reason;
        v["is_completed"] = is_completed;
        v["created_at"] = created_at;
        return v;
    }

    static StudySession from_json(const sm_json::Value& v) {
        StudySession s;
        s.id = v["id"].as_string();
        s.user_id = v["user_id"].as_string();
        s.subject_id = v["subject_id"].as_string();
        s.task_id = v["task_id"].as_string();
        s.date = v["date"].as_string();
        s.start_time = v["start_time"].as_string();
        s.end_time = v["end_time"].as_string();
        s.duration_minutes = v["duration_minutes"].as_int(45);
        s.activity = v["activity"].as_string();
        s.priority = v["priority"].as_string("Medium");
        s.reason = v["reason"].as_string();
        s.is_completed = v["is_completed"].as_bool(false);
        s.created_at = v["created_at"].as_string();
        return s;
    }
};

struct FocusRecord {
    std::string id;
    std::string user_id;
    std::string subject_id;
    std::string task_id;
    int duration_minutes;
    std::string mode; // "pomodoro", "short_break", "long_break"
    std::string timestamp;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["user_id"] = user_id;
        v["subject_id"] = subject_id;
        v["task_id"] = task_id;
        v["duration_minutes"] = duration_minutes;
        v["mode"] = mode;
        v["timestamp"] = timestamp;
        return v;
    }

    static FocusRecord from_json(const sm_json::Value& v) {
        FocusRecord r;
        r.id = v["id"].as_string();
        r.user_id = v["user_id"].as_string();
        r.subject_id = v["subject_id"].as_string();
        r.task_id = v["task_id"].as_string();
        r.duration_minutes = v["duration_minutes"].as_int(25);
        r.mode = v["mode"].as_string("pomodoro");
        r.timestamp = v["timestamp"].as_string();
        return r;
    }
};

struct Achievement {
    std::string id;
    std::string user_id;
    std::string title;
    std::string description;
    std::string icon;
    std::string unlocked_at;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["id"] = id;
        v["user_id"] = user_id;
        v["title"] = title;
        v["description"] = description;
        v["icon"] = icon;
        v["unlocked_at"] = unlocked_at;
        return v;
    }

    static Achievement from_json(const sm_json::Value& v) {
        Achievement a;
        a.id = v["id"].as_string();
        a.user_id = v["user_id"].as_string();
        a.title = v["title"].as_string();
        a.description = v["description"].as_string();
        a.icon = v["icon"].as_string();
        a.unlocked_at = v["unlocked_at"].as_string();
        return a;
    }
};

} // namespace studymate
