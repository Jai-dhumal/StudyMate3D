#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include "models.h"
#include "storage.h"
#include "crypto.h"
#include "sm_json.h"

namespace studymate {

class ProgressService {
private:
    Storage& storage;

public:
    ProgressService(Storage& st) : storage(st) {}

    sm_json::Value get_summary(const std::string& user_id) {
        std::vector<Subject> all_subjects = storage.get_all_subjects();
        std::vector<Task> all_tasks = storage.get_all_tasks();
        std::vector<FocusRecord> all_focus = storage.get_all_focus_records();
        std::vector<StudySession> all_sessions = storage.get_all_study_sessions();
        std::string today = CryptoUtils::get_current_date();

        int total_subjects = 0;
        int total_tasks = 0;
        int completed_tasks = 0;
        int pending_tasks = 0;
        int overdue_tasks = 0;
        int total_estimated_minutes = 0;
        int recorded_study_minutes = 0;
        int today_study_minutes = 0;

        for (size_t i = 0; i < all_subjects.size(); ++i) {
            if (all_subjects[i].user_id == user_id) {
                total_subjects++;
            }
        }

        std::vector<sm_json::Value> upcoming_tasks_json;

        for (size_t i = 0; i < all_tasks.size(); ++i) {
            if (all_tasks[i].user_id == user_id) {
                total_tasks++;
                total_estimated_minutes += all_tasks[i].estimated_minutes;
                if (all_tasks[i].status == "Completed") {
                    completed_tasks++;
                } else {
                    pending_tasks++;
                    if (!all_tasks[i].due_date.empty() && all_tasks[i].due_date < today) {
                        overdue_tasks++;
                    }
                    if (upcoming_tasks_json.size() < 5) {
                        upcoming_tasks_json.push_back(all_tasks[i].to_json());
                    }
                }
            }
        }

        // Calculate focus minutes & streak
        std::set<std::string> active_days;
        for (size_t i = 0; i < all_focus.size(); ++i) {
            if (all_focus[i].user_id == user_id) {
                recorded_study_minutes += all_focus[i].duration_minutes;
                std::string date_part = all_focus[i].timestamp.substr(0, 10);
                active_days.insert(date_part);
                if (date_part == today) {
                    today_study_minutes += all_focus[i].duration_minutes;
                }
            }
        }

        // Also check completed study sessions for streak and minutes
        for (size_t i = 0; i < all_sessions.size(); ++i) {
            if (all_sessions[i].user_id == user_id && all_sessions[i].is_completed) {
                active_days.insert(all_sessions[i].date);
                if (all_sessions[i].date == today) {
                    today_study_minutes += all_sessions[i].duration_minutes;
                }
            }
        }

        // Calculate continuous streak
        int streak = 0;
        time_t t_now = time(nullptr);
        for (int d = 0; d < 365; ++d) {
            time_t t_check = t_now - (d * 86400);
            tm* lt = localtime(&t_check);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d", lt);
            std::string d_str(buf);
            if (active_days.find(d_str) != active_days.end()) {
                streak++;
            } else if (d > 0) {
                // broken streak
                break;
            }
        }

        double completion_pct = (total_tasks > 0) ? ((double)completed_tasks / total_tasks * 100.0) : 0.0;

        // Check & unlock achievements
        check_achievements(user_id, completed_tasks, recorded_study_minutes, streak);

        sm_json::Value summary = sm_json::Value::make_object();
        summary["total_subjects"] = total_subjects;
        summary["total_tasks"] = total_tasks;
        summary["completed_tasks"] = completed_tasks;
        summary["pending_tasks"] = pending_tasks;
        summary["overdue_tasks"] = overdue_tasks;
        summary["completion_percentage"] = completion_pct;
        summary["streak_days"] = streak;
        summary["total_study_minutes"] = recorded_study_minutes;
        summary["today_study_minutes"] = today_study_minutes;
        summary["today_date"] = today;

        sm_json::Value upcoming = sm_json::Value::make_array();
        for (size_t i = 0; i < upcoming_tasks_json.size(); ++i) {
            upcoming.push_back(upcoming_tasks_json[i]);
        }
        summary["upcoming_tasks"] = upcoming;

        return summary;
    }

    sm_json::Value get_subject_progress(const std::string& user_id) {
        std::vector<Subject> all_subjects = storage.get_all_subjects();
        std::vector<Task> all_tasks = storage.get_all_tasks();
        std::vector<FocusRecord> all_focus = storage.get_all_focus_records();

        sm_json::Value arr = sm_json::Value::make_array();
        for (size_t si = 0; si < all_subjects.size(); ++si) {
            if (all_subjects[si].user_id != user_id) continue;

            const Subject& sub = all_subjects[si];
            int sub_tasks = 0;
            int sub_completed = 0;
            int sub_minutes = 0;

            for (size_t ti = 0; ti < all_tasks.size(); ++ti) {
                if (all_tasks[ti].subject_id == sub.id) {
                    sub_tasks++;
                    if (all_tasks[ti].status == "Completed") {
                        sub_completed++;
                    }
                }
            }

            for (size_t fi = 0; fi < all_focus.size(); ++fi) {
                if (all_focus[fi].subject_id == sub.id) {
                    sub_minutes += all_focus[fi].duration_minutes;
                }
            }

            double pct = (sub_tasks > 0) ? ((double)sub_completed / sub_tasks * 100.0) : 0.0;

            sm_json::Value item = sm_json::Value::make_object();
            item["id"] = sub.id;
            item["name"] = sub.name;
            item["color"] = sub.color;
            item["total_tasks"] = sub_tasks;
            item["completed_tasks"] = sub_completed;
            item["recorded_hours"] = sub_minutes / 60.0;
            item["target_hours"] = sub.target_hours;
            item["progress_percentage"] = pct;
            arr.push_back(item);
        }
        return arr;
    }

    void check_achievements(const std::string& user_id, int completed_tasks, int total_study_mins, int streak) {
        std::vector<Achievement> all_ach = storage.get_all_achievements();
        std::vector<Achievement> user_ach;
        for (size_t i = 0; i < all_ach.size(); ++i) {
            if (all_ach[i].user_id == user_id) {
                user_ach.push_back(all_ach[i]);
            }
        }

        auto has_badge = [&](const std::string& title) {
            for (size_t i = 0; i < user_ach.size(); ++i) {
                if (user_ach[i].title == title) return true;
            }
            return false;
        };

        bool new_unlocked = false;
        auto award = [&](const std::string& title, const std::string& desc, const std::string& icon) {
            if (!has_badge(title)) {
                Achievement a;
                a.id = CryptoUtils::generate_id("ach");
                a.user_id = user_id;
                a.title = title;
                a.description = desc;
                a.icon = icon;
                a.unlocked_at = CryptoUtils::get_current_iso_time();
                all_ach.push_back(a);
                user_ach.push_back(a);
                new_unlocked = true;
            }
        };

        if (completed_tasks >= 1) {
            award("First Milestone", "Completed your very first study task!", "sparkles");
        }
        if (completed_tasks >= 5) {
            award("Momentum Builder", "Completed 5 tasks. You're building serious study habits!", "flame");
        }
        if (completed_tasks >= 15) {
            award("Study Master", "Crushed 15 study tasks like a pro!", "trophy");
        }
        if (total_study_mins >= 25) {
            award("Deep Focus Initiate", "Completed your first 25+ min focus session!", "clock");
        }
        if (total_study_mins >= 120) {
            award("Centurion Learner", "Logged over 2 hours of concentrated focus time!", "brain");
        }
        if (streak >= 3) {
            award("Unstoppable Streak", "Maintained an active study streak for 3 consecutive days!", "zap");
        }

        if (new_unlocked) {
            storage.save_all_achievements(all_ach);
        }
    }
};

} // namespace studymate
