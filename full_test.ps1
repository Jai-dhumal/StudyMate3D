$ErrorActionPreference = "Stop"

# 1. Test Static files
$html = Invoke-WebRequest -Uri "http://localhost:8080/" -Method Get
Write-Output "GET / status: $($html.StatusCode), length: $($html.RawContentLength)"
if ($html.Content -notmatch "StudyMate 3D") { throw "HTML content check failed" }

$css = Invoke-WebRequest -Uri "http://localhost:8080/css/style.css" -Method Get
Write-Output "GET /css/style.css status: $($css.StatusCode)"

$js = Invoke-WebRequest -Uri "http://localhost:8080/js/app.js" -Method Get
Write-Output "GET /js/app.js status: $($js.StatusCode)"

$uni = Invoke-WebRequest -Uri "http://localhost:8080/js/universe.js" -Method Get
Write-Output "GET /js/universe.js status: $($uni.StatusCode)"

# 2. Test Auth Flow
$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession

# Signup
$signupPayload = @{
    full_name = "Maya Lin"
    email = "maya@mit.edu"
    username = "mayalin"
    password = "QuantumStudy2026!"
} | ConvertTo-Json

$signupRes = Invoke-RestMethod -Uri "http://localhost:8080/api/auth/signup" -Method Post -Body $signupPayload -ContentType "application/json" -WebSession $session
Write-Output "Auth Signup: $($signupRes.message) (User: $($signupRes.user.username))"

# Check Me
$meRes = Invoke-RestMethod -Uri "http://localhost:8080/api/auth/me" -Method Get -WebSession $session
Write-Output "Auth Me: Verified logged in as $($meRes.user.full_name)"

# 3. Test Subjects CRUD
$sub1 = @{
    name = "Quantum Computing"
    description = "Qubits, quantum circuits, and Shor algorithm"
    color = "#8b5cf6"
    target_hours = 12
    difficulty = "Hard"
    confidence = 4
} | ConvertTo-Json
$sub1Res = Invoke-RestMethod -Uri "http://localhost:8080/api/subjects" -Method Post -Body $sub1 -ContentType "application/json" -WebSession $session
Write-Output "Subject Created: $($sub1Res.name) [ID: $($sub1Res.id)]"

$sub2 = @{
    name = "Linear Algebra"
    description = "Eigenvectors, SVD, vector spaces"
    color = "#06b6d4"
    target_hours = 8
    difficulty = "Medium"
    confidence = 3
} | ConvertTo-Json
$sub2Res = Invoke-RestMethod -Uri "http://localhost:8080/api/subjects" -Method Post -Body $sub2 -ContentType "application/json" -WebSession $session
Write-Output "Subject Created: $($sub2Res.name) [ID: $($sub2Res.id)]"

# 4. Test Tasks CRUD
$task1 = @{
    subject_id = $sub1Res.id
    title = "Complete Problem Set 4 on Entanglement"
    description = "Problems 1 through 6 on density matrices"
    due_date = "2026-10-24"
    estimated_minutes = 90
    priority = "High"
} | ConvertTo-Json
$task1Res = Invoke-RestMethod -Uri "http://localhost:8080/api/tasks" -Method Post -Body $task1 -ContentType "application/json" -WebSession $session
Write-Output "Task Created: $($task1Res.title) [Priority: $($task1Res.priority)]"

$task2 = @{
    subject_id = $sub2Res.id
    title = "Review Gram-Schmidt Orthogonalization"
    description = "Chapter 5 review before seminar"
    due_date = "2026-10-22"
    estimated_minutes = 45
    priority = "Medium"
} | ConvertTo-Json
$task2Res = Invoke-RestMethod -Uri "http://localhost:8080/api/tasks" -Method Post -Body $task2 -ContentType "application/json" -WebSession $session
Write-Output "Task Created: $($task2Res.title)"

# 5. Test AI Planner & Timetable Acceptance
$planPayload = @{
    prompt = "I have a major quantum mechanics assignment due Saturday and need balanced breaks."
    daily_hours_limit = 4
    preferred_session_min = 45
    start_time_pref = "10:00"
    focus_goal = "Master Qubit Formulations"
} | ConvertTo-Json
$planRes = Invoke-RestMethod -Uri "http://localhost:8080/api/ai/plan" -Method Post -Body $planPayload -ContentType "application/json" -WebSession $session
Write-Output "AI Engine: $($planRes.provider_name)"
Write-Output "Total Planned Hours: $($planRes.total_planned_hours) hrs across $($planRes.sessions.Count) sessions"

# Accept the plan into Timetable
$acceptPayload = @{
    sessions = $planRes.sessions
} | ConvertTo-Json
$acceptRes = Invoke-RestMethod -Uri "http://localhost:8080/api/ai/plan/accept" -Method Post -Body $acceptPayload -ContentType "application/json" -WebSession $session
Write-Output "AI Plan Acceptance: $($acceptRes.message)"

# 6. Test Focus Mode Record
$focusPayload = @{
    subject_id = $sub1Res.id
    task_id = $task1Res.id
    duration_minutes = 25
    mode = "pomodoro"
} | ConvertTo-Json
$focusRes = Invoke-RestMethod -Uri "http://localhost:8080/api/focus/sessions" -Method Post -Body $focusPayload -ContentType "application/json" -WebSession $session
Write-Output "Focus Record: Saved $($focusRes.duration_minutes)m session"

# 7. Complete a Task
$patchRes = Invoke-RestMethod -Uri "http://localhost:8080/api/tasks/$($task1Res.id)/status" -Method Patch -Body '{"status":"Completed"}' -ContentType "application/json" -WebSession $session
Write-Output "Task 1 Status Patched: $($patchRes.status)"

# 8. Check Progress & Achievements
$progSummary = Invoke-RestMethod -Uri "http://localhost:8080/api/progress/summary" -Method Get -WebSession $session
Write-Output "Progress Summary -> Total Tasks: $($progSummary.total_tasks), Completed: $($progSummary.completed_tasks), Streak: $($progSummary.streak_days) days, Total Study: $($progSummary.total_study_minutes) mins"

$achs = Invoke-RestMethod -Uri "http://localhost:8080/api/achievements" -Method Get -WebSession $session
Write-Output "Unlocked Achievements Count: $($achs.Count)"
foreach ($a in $achs) {
    Write-Output "  * Trophy: $($a.title) - $($a.description)"
}

Write-Output "=========================================="
Write-Output "ALL SYSTEM WORKFLOW TESTS COMPLETED 100%!"
Write-Output "=========================================="
