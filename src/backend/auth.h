#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include "models.h"
#include "storage.h"
#include "crypto.h"
#include "http_types.h"

namespace studymate {

class AuthService {
private:
    Storage& storage;

    bool is_valid_email(const std::string& email) {
        if (email.length() < 5 || email.length() > 100) return false;
        size_t at = email.find('@');
        size_t dot = email.rfind('.');
        return at != std::string::npos && dot != std::string::npos && at < dot && at > 0 && dot < email.length() - 1;
    }

    bool is_valid_username(const std::string& username) {
        if (username.length() < 3 || username.length() > 30) return false;
        for (char c : username) {
            if (!isalnum(c) && c != '_' && c != '-') return false;
        }
        return true;
    }

public:
    AuthService(Storage& st) : storage(st) {}

    // Clean expired sessions
    void clean_expired_sessions() {
        std::vector<Session> sessions = storage.get_all_sessions();
        long long now = CryptoUtils::get_current_epoch_seconds();
        std::vector<Session> valid;
        for (size_t i = 0; i < sessions.size(); ++i) {
            if (sessions[i].expires_at > now) {
                valid.push_back(sessions[i]);
            }
        }
        if (valid.size() != sessions.size()) {
            storage.save_all_sessions(valid);
        }
    }

    bool signup(const std::string& full_name, const std::string& email, const std::string& username, const std::string& password, User& out_user, std::string& err_msg) {
        if (full_name.empty()) {
            err_msg = "Full name is required.";
            return false;
        }
        if (!is_valid_email(email)) {
            err_msg = "Please provide a valid email address.";
            return false;
        }
        if (!is_valid_username(username)) {
            err_msg = "Username must be 3-30 alphanumeric characters.";
            return false;
        }
        if (password.length() < 6) {
            err_msg = "Password must be at least 6 characters long.";
            return false;
        }

        std::vector<User> users = storage.get_all_users();
        std::string lower_email = email;
        std::transform(lower_email.begin(), lower_email.end(), lower_email.begin(), ::tolower);
        std::string lower_uname = username;
        std::transform(lower_uname.begin(), lower_uname.end(), lower_uname.begin(), ::tolower);

        for (size_t i = 0; i < users.size(); ++i) {
            std::string u_email = users[i].email;
            std::transform(u_email.begin(), u_email.end(), u_email.begin(), ::tolower);
            if (u_email == lower_email) {
                err_msg = "Email is already registered. Please login instead.";
                return false;
            }
            std::string u_name = users[i].username;
            std::transform(u_name.begin(), u_name.end(), u_name.begin(), ::tolower);
            if (u_name == lower_uname) {
                err_msg = "Username is already taken. Please choose another.";
                return false;
            }
        }

        User nu;
        nu.id = CryptoUtils::generate_id("usr");
        nu.full_name = full_name;
        nu.email = email;
        nu.username = username;
        nu.password_hash = CryptoUtils::hash_password(password);
        nu.created_at = CryptoUtils::get_current_iso_time();

        users.push_back(nu);
        storage.save_all_users(users);
        out_user = nu;
        return true;
    }

    bool login(const std::string& username_or_email, const std::string& password, User& out_user, Session& out_session, std::string& err_msg) {
        if (username_or_email.empty() || password.empty()) {
            err_msg = "Username/Email and password are required.";
            return false;
        }

        std::vector<User> users = storage.get_all_users();
        std::string target = username_or_email;
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);

        User* found = nullptr;
        for (size_t i = 0; i < users.size(); ++i) {
            std::string u_email = users[i].email;
            std::transform(u_email.begin(), u_email.end(), u_email.begin(), ::tolower);
            std::string u_name = users[i].username;
            std::transform(u_name.begin(), u_name.end(), u_name.begin(), ::tolower);

            if (u_email == target || u_name == target) {
                found = &users[i];
                break;
            }
        }

        if (!found) {
            err_msg = "Invalid credentials. Please verify your email/username and password.";
            return false;
        }

        if (!CryptoUtils::verify_password(password, found->password_hash)) {
            err_msg = "Invalid credentials. Please verify your email/username and password.";
            return false;
        }

        out_user = *found;

        // Create Session
        Session sess;
        sess.token = CryptoUtils::generate_random_hex(32);
        sess.user_id = found->id;
        sess.created_at = CryptoUtils::get_current_epoch_seconds();
        sess.expires_at = sess.created_at + (7 * 86400); // 7 days

        std::vector<Session> sessions = storage.get_all_sessions();
        sessions.push_back(sess);
        storage.save_all_sessions(sessions);

        out_session = sess;
        return true;
    }

    bool logout(const std::string& token) {
        if (token.empty()) return true;
        std::vector<Session> sessions = storage.get_all_sessions();
        std::vector<Session> remaining;
        for (size_t i = 0; i < sessions.size(); ++i) {
            if (sessions[i].token != token) {
                remaining.push_back(sessions[i]);
            }
        }
        storage.save_all_sessions(remaining);
        return true;
    }

    // Authenticate a request via session cookie or Authorization header (Bearer token)
    bool authenticate_request(const HttpRequest& req, User& out_user) {
        std::string token = req.get_cookie("sm_session");
        if (token.empty()) {
            std::string auth_hdr = req.get_header("Authorization");
            if (auth_hdr.substr(0, 7) == "Bearer ") {
                token = auth_hdr.substr(7);
            }
        }
        if (token.empty()) return false;

        std::vector<Session> sessions = storage.get_all_sessions();
        long long now = CryptoUtils::get_current_epoch_seconds();
        std::string found_user_id = "";

        for (size_t i = 0; i < sessions.size(); ++i) {
            if (sessions[i].token == token) {
                if (sessions[i].expires_at > now) {
                    found_user_id = sessions[i].user_id;
                }
                break;
            }
        }

        if (found_user_id.empty()) return false;

        std::vector<User> users = storage.get_all_users();
        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i].id == found_user_id) {
                out_user = users[i];
                return true;
            }
        }
        return false;
    }
};

} // namespace studymate
