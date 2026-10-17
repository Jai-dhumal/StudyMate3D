# StudyMate 3D — Your Intelligent Study Universe

**StudyMate 3D** is a full-stack, AI-powered study planner application featuring a multi-threaded C++ backend, persistent JSON storage, a 3D interactive celestial study universe built with Three.js, a Pomodoro focus engine, and cognitive study scheduling.

---

## 🌟 Key Features

1. **Interactive 3D Study Universe (Three.js)**:
   - Central study orb representing your academic life with surrounding orbital rings.
   - Subjects rendered as orbiting satellites with custom color palettes and difficulty indicators.
   - Tasks rendered as interactive asteroid clusters color-coded by urgency (High, Medium, Low).
   - Mouse click inspection, full 3D rotation, zoom, camera reset, and real-time synchronization with task completion.
   - Graceful fallback for non-WebGL environments.

2. **C++ Multi-Threaded Backend**:
   - Written in modern C++ with zero external runtime bloat.
   - Multi-threaded TCP socket server supporting concurrent requests using native Windows worker threads.
   - Custom high-speed JSON parser and serializer (`sm_json.h`).
   - Atomic file operations (`MoveFileExA`) ensuring zero data corruption during concurrent writes.

3. **Enterprise Authentication & Security**:
   - Salted iterative PBKDF/SHA-256 password hashing.
   - Cryptographically secure 256-bit random tokens via Windows CryptoAPI (`CryptGenRandom`).
   - Secure server-side sessions with `HttpOnly` and `SameSite=Lax` cookies, with Bearer header fallback.
   - Strict resource ownership validation (prevents horizontal privilege escalation).

4. **Dual AI Planning Architecture**:
   - **Gemini AI Integration**: Connects via secure WinINet HTTPS requests to Google Gemini 1.5 Flash to synthesize intelligent study timetables based on task deadlines, subject difficulty, confidence ratings, and daily hour limits.
   - **Cognitive Rule-Based Fallback**: Activates automatically whenever no API key is provided or the network is unavailable. Balances cognitive workload, prioritizes urgent assignments, injects 15-minute recovery intervals, and issues workload warnings.
   - Interactive student review: Students can inspect, edit, and accept proposed study schedules into their active weekly calendar.

5. **Deep Focus Engine (Pomodoro)**:
   - Configurable 25m Focus / 5m Short Break / 15m Long Break intervals with circular SVG animated timer.
   - Monotonic time tracking persisted directly to the backend.
   - Focus history connected to daily and weekly productivity metrics.

6. **Gamification & Analytics**:
   - Automatic study streak calculation based on activity dates.
   - Automated milestone achievement system (*First Milestone*, *Momentum Builder*, *Deep Focus Initiate*, etc.).
   - Visual progress bars, task completion percentages, and subject-by-subject target hours tracking.

---

## 📂 Project Architecture

```
StudyMate/
├── CMakeLists.txt                 # CMake build configuration
├── .env.example                   # Environment configuration template
├── README.md                      # Complete documentation
├── studymate_server.exe           # Compiled high-performance C++ server binary
├── src/
│   └── backend/
│       ├── main.cpp               # Application entry point
│       ├── server.h               # Multi-threaded HTTP server & routing
│       ├── http_types.h           # HTTP parser and request/response models
│       ├── models.h               # Domain data models (User, Subject, Task, etc.)
│       ├── storage.h              # Thread-safe atomic JSON persistence layer
│       ├── auth.h                 # Authentication, hashing, and session management
│       ├── ai_service.h           # Gemini AI HTTPS client & rule-based engine
│       ├── progress.h             # Streak, analytics, and achievement calculations
│       ├── crypto.h               # WinCrypt SHA-256 and CSPRNG utilities
│       └── sm_json.h              # High-performance lightweight JSON library
├── public/                        # Single-origin frontend assets
│   ├── index.html                 # Complete Single-Page Application (SPA)
│   ├── css/
│   │   └── style.css              # Glassmorphic futuristic design system
│   └── js/
│       ├── app.js                 # Frontend SPA controller and state sync
│       └── universe.js            # Three.js 3D Study Universe scene controller
└── data/                          # Persistent JSON storage directory (auto-created)
    ├── users.json
    ├── sessions.json
    ├── subjects.json
    ├── tasks.json
    ├── study_sessions.json
    ├── focus_records.json
    └── achievements.json
```

---

## 🛠️ Build and Compilation

### Prerequisites
- **Compiler**: MinGW GCC 6.3+ (or modern GCC / Clang / MSVC).
- **Libraries Linked**: `ws2_32` (Winsock), `advapi32` (WinCrypt), `wininet` (HTTPS).

### Direct Build (Windows PowerShell / CMD)
```powershell
g++ -std=c++14 -O2 -Isrc\backend src\backend\main.cpp -o studymate_server.exe -lws2_32 -ladvapi32 -lwininet
```

### Build with CMake
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 🚀 Running the Server

1. **(Optional) Configure Gemini AI API Key**:
   To enable online AI planning with Gemini 1.5 Flash:
   ```powershell
   $env:GEMINI_API_KEY = "your_actual_gemini_api_key"
   ```
   *(If not set, the built-in rule-based fallback will handle scheduling seamlessly).*

2. **Launch Server**:
   ```powershell
   .\studymate_server.exe 8080
   ```

3. **Access Application**:
   Open your browser and navigate to:
   [http://localhost:8080](http://localhost:8080)

---

## 🧪 Verified Test Results

All core backend endpoints, security layers, and frontend components were verified end-to-end:

| Module | Verification Step | Status |
| :--- | :--- | :--- |
| **Server Health** | `GET /api/health` | Passed (200 OK) |
| **Authentication** | Registration, duplicate prevention, PBKDF hashing | Passed |
| **Session Control** | Cookie session validation & Bearer header auth | Passed |
| **Subjects CRUD** | Create, Read, Update, Delete with ownership isolation | Passed |
| **Tasks Engine** | Status patching (`Pending` -> `Completed`), priority sorting | Passed |
| **AI Planner** | Rule-based cognitive synthesis & Gemini integration | Passed |
| **Timetable Sync** | Proposed AI plan approval and weekly calendar storage | Passed |
| **Focus Mode** | Recorded session logging and time tracking | Passed |
| **Achievements** | Automatic unlock triggers on milestone completions | Passed |
| **3D Universe** | Three.js scene creation, orbit physics, hover tooltips | Passed |

---

## 🔒 Security Design

- Passwords are never stored in plaintext; salted iterative SHA-256 hashes are used.
- HttpOnly session cookies mitigate XSS session hijacking.
- Authorization checks ensure students can only view and modify their own records.
- Input validation sanitizes strings, checks date formats, and enforces email structures.
- Static file delivery includes directory traversal protection (`..` prevention).

---

## 📝 Known Limitations

- **File-based JSON storage**: Ideal for local or single-instance setups; migrating to SQLite or PostgreSQL is recommended for large multi-node deployments.
- **Offline Three.js**: Uses standard CDN for Three.js script; for fully air-gapped environments, copy `three.min.js` to `/public/libs/`.
