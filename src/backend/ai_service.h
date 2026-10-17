#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "models.h"
#include "sm_json.h"
#include "crypto.h"

namespace studymate {

struct PlanRequestInput {
    std::string prompt;
    int daily_hours_limit;
    int preferred_session_min;
    std::vector<std::string> available_days;
    std::string start_time_pref; // e.g. "09:00"
    std::string focus_goal;
};

struct ProposedSession {
    std::string date;
    std::string start_time;
    std::string end_time;
    std::string subject_id;
    std::string subject_name;
    std::string task_id;
    std::string task_title;
    std::string activity;
    int duration_minutes;
    std::string priority;
    std::string reason;
    std::string break_after; // e.g. "15m break"

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["date"] = date;
        v["start_time"] = start_time;
        v["end_time"] = end_time;
        v["subject_id"] = subject_id;
        v["subject_name"] = subject_name;
        v["task_id"] = task_id;
        v["task_title"] = task_title;
        v["activity"] = activity;
        v["duration_minutes"] = duration_minutes;
        v["priority"] = priority;
        v["reason"] = reason;
        v["break_after"] = break_after;
        return v;
    }

    static ProposedSession from_json(const sm_json::Value& v) {
        ProposedSession s;
        s.date = v["date"].as_string();
        s.start_time = v["start_time"].as_string();
        s.end_time = v["end_time"].as_string();
        s.subject_id = v["subject_id"].as_string();
        s.subject_name = v["subject_name"].as_string();
        s.task_id = v["task_id"].as_string();
        s.task_title = v["task_title"].as_string();
        s.activity = v["activity"].as_string();
        s.duration_minutes = v["duration_minutes"].as_int(45);
        s.priority = v["priority"].as_string("Medium");
        s.reason = v["reason"].as_string();
        s.break_after = v["break_after"].as_string("15 min recovery break");
        return s;
    }
};

struct PlanResult {
    bool is_ai_generated;
    std::string provider_name;
    std::string summary;
    std::vector<ProposedSession> sessions;
    std::vector<std::string> recommendations;
    std::vector<std::string> conflict_warnings;
    double total_planned_hours;

    sm_json::Value to_json() const {
        sm_json::Value v = sm_json::Value::make_object();
        v["is_ai_generated"] = is_ai_generated;
        v["provider_name"] = provider_name;
        v["summary"] = summary;
        v["total_planned_hours"] = total_planned_hours;

        sm_json::Value s_arr = sm_json::Value::make_array();
        for (size_t i = 0; i < sessions.size(); ++i) {
            s_arr.push_back(sessions[i].to_json());
        }
        v["sessions"] = s_arr;

        sm_json::Value r_arr = sm_json::Value::make_array();
        for (size_t i = 0; i < recommendations.size(); ++i) {
            r_arr.push_back(recommendations[i]);
        }
        v["recommendations"] = r_arr;

        sm_json::Value c_arr = sm_json::Value::make_array();
        for (size_t i = 0; i < conflict_warnings.size(); ++i) {
            c_arr.push_back(conflict_warnings[i]);
        }
        v["conflict_warnings"] = c_arr;
        return v;
    }
};

class AIService {
private:
    std::string api_key;

    static std::string add_minutes_to_time(const std::string& time_hhmm, int minutes) {
        int h = 9, m = 0;
        size_t c = time_hhmm.find(':');
        if (c != std::string::npos) {
            try {
                h = std::stoi(time_hhmm.substr(0, c));
                m = std::stoi(time_hhmm.substr(c + 1));
            } catch (...) {}
        }
        m += minutes;
        h += (m / 60);
        m %= 60;
        h %= 24;
        char buf[16];
        sprintf(buf, "%02d:%02d", h, m);
        return std::string(buf);
    }

    // Cross-platform HTTPS POST request (WinINet on Windows, popen curl on Linux)
    bool send_https_post(const std::string& host, const std::string& path, const std::string& post_data, std::string& response_data) {
#ifdef _WIN32
        HINTERNET hInternet = InternetOpenA("StudyMate3D-Backend/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return false;

        HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return false;
        }

        DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE;
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }

        std::string headers = "Content-Type: application/json\r\n";
        BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()), (LPVOID)post_data.c_str(), static_cast<DWORD>(post_data.length()));
        if (!sent) {
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }

        char buffer[4096];
        DWORD bytesRead = 0;
        std::ostringstream responseStream;
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            responseStream << buffer;
        }

        response_data = responseStream.str();
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return true;
#else
        // Portable curl-based HTTPS client for Linux / Docker environments
        std::string url = "https://" + host + path;
        std::string tmp_in = "/tmp/sm_ai_in_" + CryptoUtils::generate_random_hex(6) + ".json";
        std::string tmp_out = "/tmp/sm_ai_out_" + CryptoUtils::generate_random_hex(6) + ".json";

        std::ofstream tf(tmp_in);
        if (!tf.is_open()) return false;
        tf << post_data;
        tf.close();

        std::string cmd = "curl -s -X POST -H 'Content-Type: application/json' -d @" + tmp_in + " '" + url + "' > " + tmp_out;
        int ret = system(cmd.c_str());
        std::remove(tmp_in.c_str());

        if (ret == 0) {
            std::ifstream rf(tmp_out);
            if (rf.is_open()) {
                std::ostringstream ss;
                ss << rf.rdbuf();
                response_data = ss.str();
                rf.close();
            }
        }
        std::remove(tmp_out.c_str());
        return !response_data.empty();
#endif
    }

    // Helper to strip conversational phrases and filler words from prompts
    std::string clean_phrase(const std::string& input_str) {
        std::string s = input_str;
        while (!s.empty() && (s[0] == ' ' || s[0] == '\t' || s[0] == '"' || s[0] == '\'')) s.erase(0, 1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '"' || s.back() == '\'')) s.pop_back();

        std::vector<std::string> prefixes = {
            "i have a ", "I have a ", "i have an ", "I have an ", "i have ", "I have ",
            "we have a ", "We have a ", "we have ", "We have ",
            "help me study ", "Help me study ", "help me learn ", "Help me learn ", "help me with ", "Help me with ", "help me ", "Help me ",
            "i need to master ", "I need to master ", "i need to study ", "I need to study ", "i need to prepare for ", "I need to prepare for ", "i need to ", "I need to ",
            "i want to study ", "I want to study ", "i want to learn ", "I want to learn ", "i want to ", "I want to ",
            "prepare for my ", "Prepare for my ", "prepare for ", "Prepare for ",
            "study for my ", "Study for my ", "study for ", "Study for ", "study ", "Study ",
            "master ", "Master ", "learn ", "Learn ", "review ", "Review "
        };

        for (size_t pi = 0; pi < prefixes.size(); ++pi) {
            if (s.size() >= prefixes[pi].size()) {
                std::string sub = s.substr(0, prefixes[pi].size());
                std::string p_lower = prefixes[pi];
                std::string s_lower = sub;
                std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), ::tolower);
                std::transform(s_lower.begin(), s_lower.end(), s_lower.begin(), ::tolower);
                if (p_lower == s_lower) {
                    s.erase(0, prefixes[pi].size());
                    while (!s.empty() && (s[0] == ' ' || s[0] == '\t')) s.erase(0, 1);
                }
            }
        }
        // Also strip trailing filler like "paper", "exam", "test" if helpful
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '.' || s.back() == '!')) s.pop_back();
        return s;
    }

    // Helper to extract clean topics and sub-tasks directly from user input
    std::vector<std::string> extract_topics_from_text(const std::string& text) {
        std::vector<std::string> topics;
        if (text.empty()) return topics;

        // Split by punctuation: comma, semicolon, newline, period, 'and'
        std::string clean = text;
        for (char &c : clean) {
            if (c == ',' || c == ';' || c == '\n' || c == '\r' || c == '.' || c == '&') {
                c = '|';
            }
        }

        std::stringstream ss(clean);
        std::string seg;
        while (std::getline(ss, seg, '|')) {
            std::string c_seg = clean_phrase(seg);
            if (c_seg.size() > 1) {
                topics.push_back(c_seg);
            }
        }
        return topics;
    }

public:
    AIService() {
        reload_key();
    }

    void reload_key() {
        // 1. Check environment variable
        char* env_key = getenv("GEMINI_API_KEY");
        if (env_key && strlen(env_key) > 5) {
            api_key = env_key;
            return;
        }

        // 2. Check data/gemini_key.txt
        std::ifstream kf("data/gemini_key.txt");
        if (kf.is_open()) {
            std::string line;
            if (std::getline(kf, line)) {
                // Trim whitespace
                while (!line.empty() && (line[0] == ' ' || line[0] == '\r' || line[0] == '\n' || line[0] == '\t')) line.erase(0, 1);
                while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n' || line.back() == '\t')) line.pop_back();
                if (line.size() > 5) {
                    api_key = line;
                    return;
                }
            }
        }

        // 3. Check .env file in root
        std::ifstream ef(".env");
        if (ef.is_open()) {
            std::string line;
            while (std::getline(ef, line)) {
                if (line.find("GEMINI_API_KEY=") == 0) {
                    std::string val = line.substr(15);
                    while (!val.empty() && (val[0] == ' ' || val[0] == '"' || val[0] == '\'')) val.erase(0, 1);
                    while (!val.empty() && (val.back() == ' ' || val.back() == '\r' || val.back() == '\n' || val.back() == '"' || val.back() == '\'')) val.pop_back();
                    if (val.size() > 5) {
                        api_key = val;
                        return;
                    }
                }
            }
        }
    }

    void set_api_key(const std::string& key) {
        api_key = key;
        // Also persist to data/gemini_key.txt so it survives restarts
#ifdef _WIN32
        CreateDirectoryA("data", NULL);
#else
        mkdir("data", 0755);
#endif
        std::ofstream kf("data/gemini_key.txt");
        if (kf.is_open()) {
            kf << key << "\n";
        }
    }

    bool has_api_key() const {
        return !api_key.empty();
    }

    std::string get_masked_key() const {
        if (api_key.empty()) return "";
        if (api_key.length() <= 8) return "********";
        return api_key.substr(0, 4) + "...." + api_key.substr(api_key.length() - 4);
    }

    // Deep cognitive planner synthesizing specifically around the user's exact input, subjects, and tasks
    PlanResult generate_rule_based_plan(
        const PlanRequestInput& input,
        const std::vector<Subject>& subjects,
        const std::vector<Task>& tasks,
        const std::string& current_date
    ) {
        PlanResult res;
        res.is_ai_generated = false;
        res.provider_name = "StudyMate Cognitive AI Engine (Adaptive)";
        
        // 1. Thoroughly clean user's focus topic and prompt
        std::string raw_focus = input.prompt;
        if (raw_focus.empty()) raw_focus = input.focus_goal;
        if (raw_focus.empty() && !subjects.empty()) raw_focus = subjects[0].name;
        if (raw_focus.empty()) raw_focus = "Targeted Academic Preparation";

        // Remove conversational prefixes: "i have ...", "prepare for ...", "study ...", etc.
        std::string focus_topic = clean_phrase(raw_focus);
        if (focus_topic.empty()) focus_topic = raw_focus;

        std::ostringstream sum_ss;
        sum_ss << "Personalized study timetable generated for \"" << focus_topic << "\". "
               << "Synthesized tailored focus blocks, interleaved practice, and recovery rest intervals tailored to your specific schedule.";
        res.summary = sum_ss.str();

        // Extract sub-topics specifically from user prompt and goal
        std::vector<std::string> parsed_topics = extract_topics_from_text(input.prompt + " | " + input.focus_goal);
        if (parsed_topics.empty()) {
            parsed_topics.push_back(focus_topic);
        }

        // Only include pending tasks that actually relate to the user's requested topics!
        // (Do NOT inject random old tasks like "eat" or unrelated subjects if user specified a clear focus)
        std::vector<Task> relevant_tasks;
        bool has_custom_goal = (!input.prompt.empty() || !input.focus_goal.empty());

        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].status != "Completed") {
                if (!has_custom_goal) {
                    relevant_tasks.push_back(tasks[i]);
                } else {
                    // Check if task matches any requested topic or subject
                    std::string t_lower = tasks[i].title;
                    std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(), ::tolower);
                    std::string f_lower = focus_topic;
                    std::transform(f_lower.begin(), f_lower.end(), f_lower.begin(), ::tolower);
                    if (f_lower.find(t_lower) != std::string::npos || t_lower.find(f_lower) != std::string::npos) {
                        relevant_tasks.push_back(tasks[i]);
                    }
                }
            }
        }

        int daily_limit_minutes = (input.daily_hours_limit > 0 ? input.daily_hours_limit : 4) * 60;
        int session_len = (input.preferred_session_min > 0 ? input.preferred_session_min : 45);
        std::string current_time = input.start_time_pref.empty() ? "09:00" : input.start_time_pref;

        int total_minutes_scheduled = 0;
        size_t task_idx = 0;

        // Custom synthesized study progression customized around user's topic
        struct CustomStage {
            std::string title_format;
            std::string activity;
            std::string priority;
            std::string reason_format;
        };

        std::vector<CustomStage> stages = {
            { "Core Conceptual Foundations", "Deep Concept Mapping & Theory Notes", "High", "Anchor foundational concepts and core frameworks for %s." },
            { "Methodology & Hands-On Problem Drill", "Deliberate Problem-Solving Practice", "High", "Strengthen retrieval speed through practical application on %s." },
            { "Advanced Case Studies & Analytics", "Analytical Drill & Diagnostic Review", "High", "Identify and remediate conceptual bottlenecks in %s." },
            { "Timed Exam Simulation & Past Papers", "Stress-Inoculated Mock Review", "High", "Build confidence under simulated exam time constraints for %s." },
            { "Spaced Repetition & Key Formula Synthesis", "Feynman Technique & Teaching Drill", "Medium", "Convert working knowledge of %s into permanent memory retention." }
        };

        // Schedule over 4 days
        for (size_t day_i = 0; day_i < 4; ++day_i) {
            int day_budget = daily_limit_minutes;
            std::string slot_time = current_time;

            while (day_budget >= session_len && res.sessions.size() < 12) {
                ProposedSession ps;
                char date_buf[32];
                time_t t_now = time(nullptr) + (day_i * 86400);
                tm* lt = localtime(&t_now);
                strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", lt);
                ps.date = date_buf;

                ps.start_time = slot_time;
                ps.duration_minutes = session_len;
                ps.end_time = add_minutes_to_time(slot_time, session_len);
                slot_time = add_minutes_to_time(ps.end_time, 15); // 15m recovery break

                if (task_idx < relevant_tasks.size()) {
                    // Prioritize relevant tasks matching this goal
                    const Task& t = relevant_tasks[task_idx];
                    ps.task_id = t.id;
                    ps.task_title = t.title;
                    ps.subject_id = t.subject_id;

                    std::string s_name = focus_topic;
                    for (size_t si = 0; si < subjects.size(); ++si) {
                        if (subjects[si].id == t.subject_id) {
                            s_name = subjects[si].name;
                            break;
                        }
                    }
                    ps.subject_name = s_name;
                    ps.activity = (t.priority == "High") ? "Deep Focus Assignment" : "Practice & Problem Drill";
                    ps.priority = t.priority;
                    ps.reason = "Relevant pending task (" + t.title + ") prioritized for completion.";
                    task_idx++;
                } else {
                    // Synthesize directly from user's custom topic!
                    std::string sub_name = parsed_topics[res.sessions.size() % parsed_topics.size()];

                    ps.subject_id = "ai_sub_" + std::to_string(res.sessions.size() + 1);
                    ps.subject_name = sub_name;
                    ps.task_id = "";

                    const CustomStage& stage = stages[res.sessions.size() % stages.size()];
                    char buf[256];
                    sprintf(buf, stage.reason_format.c_str(), sub_name.c_str());

                    ps.task_title = stage.title_format + " (" + sub_name + ")";
                    ps.activity = stage.activity;
                    ps.priority = stage.priority;
                    ps.reason = std::string(buf);
                }

                ps.break_after = "15-minute screen-free recovery break (hydration + eye rest)";
                res.sessions.push_back(ps);
                day_budget -= (session_len + 15);
                total_minutes_scheduled += session_len;

                // Max 2-3 sessions per day to prevent cognitive fatigue
                if (day_budget < (session_len + 15) || (res.sessions.size() % 3 == 0)) {
                    break;
                }
            }
            if (res.sessions.size() >= 8) break;
        }

        res.total_planned_hours = total_minutes_scheduled / 60.0;
        
        // Dynamic customized recommendations based on user prompt
        std::string rec1 = "Focus intensively on " + focus_topic + " using active recall rather than passive reading.";
        res.recommendations.push_back(rec1);
        res.recommendations.push_back("Take a 15-minute screen-free break between every block to consolidate neural pathways.");
        res.recommendations.push_back("Use the Feynman technique: Explain concepts out loud in simple terms without looking at references.");
        res.recommendations.push_back("Ensure 7-8 hours of sleep nightly: Non-REM and REM sleep cycles solidify newly encoded material.");

        if (relevant_tasks.size() > 8) {
            res.conflict_warnings.push_back("Heavy workload detected: You have more than 8 active tasks. Focus strictly on High priority items first.");
        }

        return res;
    }

    // Real Gemini AI integration via HTTPS
    PlanResult generate_ai_plan(
        const PlanRequestInput& input,
        const std::vector<Subject>& subjects,
        const std::vector<Task>& tasks,
        const std::string& current_date
    ) {
        if (!has_api_key()) {
            return generate_rule_based_plan(input, subjects, tasks, current_date);
        }

        std::ostringstream prompt_ss;
        prompt_ss << "You are an expert academic study planner and cognitive scientist. "
                  << "Create a realistic, highly personalized study plan specifically tailored to the student's request, subjects, and tasks.\n"
                  << "Current Date: " << current_date << "\n"
                  << "Daily Study Hour Limit: " << (input.daily_hours_limit > 0 ? input.daily_hours_limit : 4) << " hours\n"
                  << "Preferred Session Duration: " << (input.preferred_session_min > 0 ? input.preferred_session_min : 45) << " minutes\n"
                  << "Preferred Start Time: " << (input.start_time_pref.empty() ? "09:00" : input.start_time_pref) << "\n";

        if (!input.focus_goal.empty()) {
            prompt_ss << "Student Focus Goal: " << input.focus_goal << "\n";
        }
        if (!input.prompt.empty()) {
            prompt_ss << "Student Custom Request: " << input.prompt << "\n";
        }

        prompt_ss << "\nSTUDENT SUBJECTS:\n";
        for (size_t i = 0; i < subjects.size(); ++i) {
            prompt_ss << "- ID: " << subjects[i].id 
                      << " | Name: " << subjects[i].name 
                      << " | Difficulty: " << subjects[i].difficulty 
                      << " | Confidence: " << subjects[i].confidence << "/5\n";
        }

        prompt_ss << "\nPENDING STUDENT TASKS:\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].status != "Completed") {
                prompt_ss << "- ID: " << tasks[i].id
                          << " | Title: " << tasks[i].title
                          << " | Subject ID: " << tasks[i].subject_id
                          << " | Due: " << tasks[i].due_date
                          << " | Priority: " << tasks[i].priority
                          << " | Est Min: " << tasks[i].estimated_minutes << "\n";
            }
        }

        prompt_ss << "\nINSTRUCTIONS:\n"
                  << "1. Schedule sessions specifically according to the student's custom request and focus goal.\n"
                  << "2. Return strictly valid JSON with no markdown wrapping or codeblocks.\n"
                  << "3. Schema:\n"
                  << "{\n"
                  << "  \"summary\": \"Brief overview tailored to the student's goal\",\n"
                  << "  \"recommendations\": [\"Tip 1\", \"Tip 2\"],\n"
                  << "  \"conflict_warnings\": [\"Warning 1 if any\"],\n"
                  << "  \"sessions\": [\n"
                  << "    {\n"
                  << "      \"date\": \"YYYY-MM-DD\",\n"
                  << "      \"start_time\": \"HH:MM\",\n"
                  << "      \"end_time\": \"HH:MM\",\n"
                  << "      \"subject_id\": \"id\",\n"
                  << "      \"subject_name\": \"name\",\n"
                  << "      \"task_id\": \"id or empty\",\n"
                  << "      \"task_title\": \"title\",\n"
                  << "      \"activity\": \"Reading / Practice / Revision\",\n"
                  << "      \"duration_minutes\": 45,\n"
                  << "      \"priority\": \"High/Medium/Low\",\n"
                  << "      \"reason\": \"Reason why scheduled now\",\n"
                  << "      \"break_after\": \"15 min recovery break\"\n"
                  << "    }\n"
                  << "  ]\n"
                  << "}\n";

        sm_json::Value req_body = sm_json::Value::make_object();
        sm_json::Value contents_arr = sm_json::Value::make_array();
        sm_json::Value content_obj = sm_json::Value::make_object();
        sm_json::Value parts_arr = sm_json::Value::make_array();
        sm_json::Value part_obj = sm_json::Value::make_object();
        part_obj["text"] = prompt_ss.str();
        parts_arr.push_back(part_obj);
        content_obj["parts"] = parts_arr;
        contents_arr.push_back(content_obj);
        // Enforce pure JSON response from Gemini
        sm_json::Value gen_cfg = sm_json::Value::make_object();
        gen_cfg["response_mime_type"] = "application/json";
        gen_cfg["temperature"] = 0.2;
        req_body["generationConfig"] = gen_cfg;

        std::string json_str = req_body.dump();
        std::string host = "generativelanguage.googleapis.com";
        std::string path = "/v1beta/models/gemini-1.5-flash:generateContent?key=" + api_key;

        std::string raw_response;
        bool ok = send_https_post(host, path, json_str, raw_response);

        if (!ok || raw_response.empty()) {
            PlanResult fallback = generate_rule_based_plan(input, subjects, tasks, current_date);
            fallback.conflict_warnings.push_back("Google Gemini endpoint was unreachable via network. Switched safely to adaptive cognitive engine.");
            return fallback;
        }

        sm_json::Value resp_json = sm_json::parse(raw_response);

        // Check if Gemini API returned an error object (e.g. invalid API key, quota exceeded, blocked)
        if (resp_json["error"].is_object()) {
            std::string api_err_msg = resp_json["error"]["message"].as_string();
            int api_code = resp_json["error"]["code"].as_int(400);
            PlanResult fallback = generate_rule_based_plan(input, subjects, tasks, current_date);
            if (api_err_msg.find("API key not valid") != std::string::npos || api_code == 400 || api_code == 403) {
                fallback.conflict_warnings.push_back("Gemini API Error: Invalid or expired API Key. Please verify your key under 'Configure Gemini API'.");
            } else if (api_code == 429) {
                fallback.conflict_warnings.push_back("Gemini API Quota Exceeded (HTTP 429). Used adaptive cognitive engine.");
            } else {
                fallback.conflict_warnings.push_back("Gemini API Notice: " + api_err_msg);
            }
            return fallback;
        }

        std::string candidate_text = "";
        if (resp_json["candidates"].is_array() && resp_json["candidates"].size() > 0) {
            sm_json::Value cand = resp_json["candidates"][0];
            if (cand["content"]["parts"].is_array() && cand["content"]["parts"].size() > 0) {
                candidate_text = cand["content"]["parts"][0]["text"].as_string();
            }
        }

        if (candidate_text.empty()) {
            PlanResult fallback = generate_rule_based_plan(input, subjects, tasks, current_date);
            fallback.conflict_warnings.push_back("AI returned no text output. Used adaptive cognitive engine.");
            return fallback;
        }

        size_t first_brace = candidate_text.find('{');
        size_t last_brace = candidate_text.rfind('}');
        if (first_brace != std::string::npos && last_brace != std::string::npos && last_brace > first_brace) {
            candidate_text = candidate_text.substr(first_brace, last_brace - first_brace + 1);
        }

        sm_json::Value ai_plan_json = sm_json::parse(candidate_text);
        if (!ai_plan_json.is_object() || !ai_plan_json["sessions"].is_array()) {
            PlanResult fallback = generate_rule_based_plan(input, subjects, tasks, current_date);
            fallback.conflict_warnings.push_back("AI plan structure was incomplete. Used adaptive cognitive engine.");
            return fallback;
        }

        PlanResult result;
        result.is_ai_generated = true;
        result.provider_name = "Google Gemini AI (1.5 Flash)";
        result.summary = ai_plan_json["summary"].as_string("AI personalized study timetable.");

        int total_mins = 0;
        sm_json::Value sess_arr = ai_plan_json["sessions"];
        for (size_t i = 0; i < sess_arr.size(); ++i) {
            ProposedSession ps = ProposedSession::from_json(sess_arr[i]);
            bool subject_exists = false;
            for (size_t si = 0; si < subjects.size(); ++si) {
                if (subjects[si].id == ps.subject_id || subjects[si].name == ps.subject_name) {
                    subject_exists = true;
                    ps.subject_id = subjects[si].id;
                    ps.subject_name = subjects[si].name;
                    break;
                }
            }
            if (!subject_exists && !subjects.empty()) {
                ps.subject_id = subjects[0].id;
                ps.subject_name = subjects[0].name;
            }
            total_mins += ps.duration_minutes;
            result.sessions.push_back(ps);
        }

        result.total_planned_hours = total_mins / 60.0;

        sm_json::Value recs_arr = ai_plan_json["recommendations"];
        if (recs_arr.is_array()) {
            for (size_t i = 0; i < recs_arr.size(); ++i) {
                result.recommendations.push_back(recs_arr[i].as_string());
            }
        }

        sm_json::Value conf_arr = ai_plan_json["conflict_warnings"];
        if (conf_arr.is_array()) {
            for (size_t i = 0; i < conf_arr.size(); ++i) {
                result.conflict_warnings.push_back(conf_arr[i].as_string());
            }
        }

        return result;
    }
};

} // namespace studymate
