// StudyMate 3D Core Frontend Application Controller

class StudyMateApp {
  constructor() {
    this.currentUser = null;
    this.subjects = [];
    this.tasks = [];
    this.studySessions = [];
    this.achievements = [];
    this.universe = null;
    this.currentView = 'dashboard';

    // Focus Timer state
    this.timerInterval = null;
    this.timerSeconds = 25 * 60;
    this.timerTotalSeconds = 25 * 60;
    this.timerIsRunning = false;
    this.timerMode = 'pomodoro'; // pomodoro, short_break, long_break
    this.selectedFocusSubjectId = '';
    this.selectedFocusTaskId = '';

    this.init();
  }

  async init() {
    this.setupTheme();
    this.setupNavigation();
    this.setupModals();
    this.setupFocusTimer();
    this.setupMobileMenu();

    // Check user authentication
    await this.checkAuth();
  }

  // --- THEME MANAGEMENT ---
  setupTheme() {
    const savedTheme = localStorage.getItem('sm_theme') || 'dark';
    document.documentElement.setAttribute('data-theme', savedTheme);
    this.updateThemeButtonIcon(savedTheme);

    const toggleBtn = document.getElementById('theme-toggle');
    if (toggleBtn) {
      toggleBtn.addEventListener('click', () => {
        const current = document.documentElement.getAttribute('data-theme');
        const next = current === 'dark' ? 'light' : 'dark';
        document.documentElement.setAttribute('data-theme', next);
        localStorage.setItem('sm_theme', next);
        this.updateThemeButtonIcon(next);
      });
    }
  }

  updateThemeButtonIcon(theme) {
    const icon = document.querySelector('#theme-toggle .theme-icon');
    if (icon) {
      icon.textContent = theme === 'dark' ? '☀️' : '🌙';
    }
  }

  // --- MOBILE MENU ---
  setupMobileMenu() {
    const menuBtn = document.getElementById('mobile-menu-btn');
    const sidebar = document.getElementById('sidebar');
    if (menuBtn && sidebar) {
      menuBtn.addEventListener('click', () => {
        sidebar.classList.toggle('open');
      });
    }
  }

  // --- NAVIGATION ---
  setupNavigation() {
    const navItems = document.querySelectorAll('.nav-item[data-view]');
    navItems.forEach(item => {
      item.addEventListener('click', (e) => {
        e.preventDefault();
        const view = item.getAttribute('data-view');
        this.switchView(view);
        // On mobile, close sidebar
        const sidebar = document.getElementById('sidebar');
        if (sidebar) sidebar.classList.remove('open');
      });
    });

    const logoutBtn = document.getElementById('logout-btn');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', () => this.logout());
    }

    const landingLoginBtn = document.getElementById('landing-login-btn');
    const landingSignupBtn = document.getElementById('landing-signup-btn');
    if (landingLoginBtn) {
      landingLoginBtn.addEventListener('click', () => this.openAuthModal('login'));
    }
    if (landingSignupBtn) {
      landingSignupBtn.addEventListener('click', () => this.openAuthModal('signup'));
    }
  }

  switchView(viewName) {
    this.currentView = viewName;
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    const activeNav = document.querySelector(`.nav-item[data-view="${viewName}"]`);
    if (activeNav) activeNav.classList.add('active');

    document.querySelectorAll('.view-container').forEach(v => v.style.display = 'none');
    const target = document.getElementById(`view-${viewName}`);
    if (target) {
      target.style.display = 'block';
      if (viewName === 'timetable') this.renderTimetable();
      if (viewName === 'progress') this.renderProgress();
      if (viewName === 'ai-planner') this.renderAIPlanner();
      if (viewName === 'tasks') this.renderTasksList();
      if (viewName === 'subjects') this.renderSubjectsGrid();
    }
  }

  // --- AUTHENTICATION ---
  async checkAuth() {
    try {
      const res = await fetch('/api/auth/me');
      if (res.ok) {
        const data = await res.json();
        this.currentUser = data.user;
        this.showApp();
        await this.loadAllData();
      } else {
        this.showLanding();
      }
    } catch (err) {
      console.warn('Backend not responding or network error:', err);
      this.showLanding();
    }
  }

  showLanding() {
    const landing = document.getElementById('landing-section');
    const appShell = document.getElementById('app-shell');
    if (landing) landing.style.display = 'block';
    if (appShell) appShell.style.display = 'none';
  }

  showApp() {
    const landing = document.getElementById('landing-section');
    const appShell = document.getElementById('app-shell');
    if (landing) landing.style.display = 'none';
    if (appShell) appShell.style.display = 'flex';

    // Update user info
    const nameEl = document.getElementById('user-display-name');
    const emailEl = document.getElementById('user-display-email');
    const avatarEl = document.getElementById('user-avatar');
    if (nameEl && this.currentUser) nameEl.textContent = this.currentUser.full_name;
    if (emailEl && this.currentUser) emailEl.textContent = '@' + this.currentUser.username;
    if (avatarEl && this.currentUser) {
      avatarEl.textContent = (this.currentUser.full_name || 'U').charAt(0).toUpperCase();
    }

    // Initialize 3D Universe if container present
    if (!this.universe && document.getElementById('universe-container')) {
      this.universe = new StudyUniverse('universe-container');
    }
  }

  async logout() {
    await fetch('/api/auth/logout', { method: 'POST' });
    this.currentUser = null;
    this.showToast('Logged out successfully', 'info');
    this.showLanding();
  }

  // --- DATA LOADING & SYNC ---
  async loadAllData() {
    try {
      const [subsRes, tasksRes, sessionsRes, progRes, achRes] = await Promise.all([
        fetch('/api/subjects'),
        fetch('/api/tasks'),
        fetch('/api/planner/sessions'),
        fetch('/api/progress/summary'),
        fetch('/api/achievements')
      ]);

      if (subsRes.ok) this.subjects = await subsRes.json();
      if (tasksRes.ok) this.tasks = await tasksRes.json();
      if (sessionsRes.ok) this.studySessions = await sessionsRes.json();
      if (achRes.ok) this.achievements = await achRes.json();

      let summaryData = {};
      if (progRes.ok) summaryData = await progRes.json();

      this.renderDashboard(summaryData);
      this.updateUniverse();
      this.populateSubjectSelects();
    } catch (err) {
      console.error('Error loading data:', err);
    }
  }

  updateUniverse() {
    if (this.universe) {
      this.universe.updateData(this.subjects, this.tasks);
    }
  }

  // --- DASHBOARD RENDERING ---
  renderDashboard(summary) {
    const totalSubsEl = document.getElementById('stat-total-subjects');
    const totalTasksEl = document.getElementById('stat-total-tasks');
    const completedTasksEl = document.getElementById('stat-completed-tasks');
    const pendingTasksEl = document.getElementById('stat-pending-tasks');
    const overdueTasksEl = document.getElementById('stat-overdue-tasks');
    const streakEl = document.getElementById('stat-streak');
    const todayStudyEl = document.getElementById('stat-today-study');
    const compPctEl = document.getElementById('stat-comp-pct');
    const progressBar = document.getElementById('dash-progress-fill');

    if (totalSubsEl) totalSubsEl.textContent = summary.total_subjects ?? this.subjects.length;
    if (totalTasksEl) totalTasksEl.textContent = summary.total_tasks ?? this.tasks.length;
    if (completedTasksEl) completedTasksEl.textContent = summary.completed_tasks ?? 0;
    if (pendingTasksEl) pendingTasksEl.textContent = summary.pending_tasks ?? 0;
    if (overdueTasksEl) overdueTasksEl.textContent = summary.overdue_tasks ?? 0;
    if (streakEl) streakEl.textContent = `${summary.streak_days ?? 0} Days 🔥`;
    if (todayStudyEl) todayStudyEl.textContent = `${summary.today_study_minutes ?? 0}m`;

    const pct = Math.round(summary.completion_percentage ?? 0);
    if (compPctEl) compPctEl.textContent = `${pct}%`;
    if (progressBar) progressBar.style.width = `${pct}%`;

    // Render upcoming tasks list
    const upcomingContainer = document.getElementById('dash-upcoming-tasks');
    if (upcomingContainer) {
      const pending = this.tasks.filter(t => t.status !== 'Completed').slice(0, 5);
      if (pending.length === 0) {
        upcomingContainer.innerHTML = `
          <div style="text-align:center;padding:2rem;color:var(--text-muted);">
            <div style="font-size:2rem;margin-bottom:0.5rem;">🎉</div>
            <p>All caught up! No pending tasks right now.</p>
          </div>
        `;
      } else {
        upcomingContainer.innerHTML = pending.map(t => {
          const sub = this.subjects.find(s => s.id === t.subject_id);
          const color = sub ? sub.color : '#8b5cf6';
          return `
            <div class="task-row glass-card" style="display:flex;align-items:center;justify-content:space-between;padding:0.85rem 1.25rem;margin-bottom:0.75rem;border-left:4px solid ${color};">
              <div style="display:flex;align-items:center;gap:0.85rem;">
                <input type="checkbox" class="task-checkbox" data-id="${t.id}" style="width:18px;height:18px;cursor:pointer;accent-color:var(--accent-purple);">
                <div>
                  <div style="font-weight:600;font-size:0.92rem;">${this.escapeHtml(t.title)}</div>
                  <div style="font-size:0.75rem;color:var(--text-muted);display:flex;gap:0.5rem;align-items:center;">
                    <span style="color:${color};font-weight:600;">${sub ? this.escapeHtml(sub.name) : 'General'}</span> •
                    <span>Due ${t.due_date || 'No deadline'}</span> •
                    <span>${t.estimated_minutes}m</span>
                  </div>
                </div>
              </div>
              <span class="badge" style="background:rgba(255,255,255,0.06);padding:0.25rem 0.6rem;border-radius:var(--radius-sm);font-size:0.75rem;font-weight:600;color:${t.priority === 'High' ? 'var(--accent-rose)' : (t.priority === 'Medium' ? 'var(--accent-amber)' : 'var(--accent-mint)')};">
                ${t.priority}
              </span>
            </div>
          `;
        }).join('');

        // Task checkbox handler
        upcomingContainer.querySelectorAll('.task-checkbox').forEach(cb => {
          cb.addEventListener('change', async (e) => {
            const taskId = e.target.getAttribute('data-id');
            await this.toggleTaskComplete(taskId);
          });
        });
      }
    }
  }

  // --- SUBJECTS CRUD ---
  renderSubjectsGrid() {
    const grid = document.getElementById('subjects-grid');
    if (!grid) return;

    if (this.subjects.length === 0) {
      grid.innerHTML = `
        <div style="grid-column: 1 / -1;text-align:center;padding:4rem;color:var(--text-muted);">
          <div style="font-size:3rem;margin-bottom:1rem;">📚</div>
          <h3>No subjects added yet</h3>
          <p style="margin-top:0.5rem;">Create your first subject to start organizing your study galaxy.</p>
          <button class="btn-primary action-btn" style="margin-top:1.25rem;" onclick="app.openNewSubjectModal()">+ Add First Subject</button>
        </div>
      `;
      return;
    }

    grid.innerHTML = this.subjects.map(sub => {
      const subTasks = this.tasks.filter(t => t.subject_id === sub.id);
      const doneTasks = subTasks.filter(t => t.status === 'Completed').length;
      const pct = subTasks.length > 0 ? Math.round((doneTasks / subTasks.length) * 100) : 0;

      return `
        <div class="glass-card" style="border-top:4px solid ${sub.color};position:relative;">
          <div style="display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:0.75rem;">
            <h3 style="font-size:1.1rem;font-weight:700;">${this.escapeHtml(sub.name)}</h3>
            <span class="badge" style="background:rgba(255,255,255,0.08);padding:0.2rem 0.5rem;border-radius:var(--radius-sm);font-size:0.72rem;font-weight:600;">${sub.difficulty}</span>
          </div>
          <p style="font-size:0.84rem;color:var(--text-secondary);min-height:40px;margin-bottom:1rem;">${this.escapeHtml(sub.description || 'No description provided.')}</p>
          
          <div style="margin-bottom:1rem;">
            <div style="display:flex;justify-content:space-between;font-size:0.78rem;color:var(--text-muted);margin-bottom:0.35rem;">
              <span>Tasks Progress (${doneTasks}/${subTasks.length})</span>
              <span>${pct}%</span>
            </div>
            <div style="width:100%;height:6px;background:rgba(255,255,255,0.06);border-radius:var(--radius-full);overflow:hidden;">
              <div style="width:${pct}%;height:100%;background:${sub.color};border-radius:var(--radius-full);transition:width 0.4s ease;"></div>
            </div>
          </div>

          <div style="display:flex;justify-content:space-between;align-items:center;border-top:1px solid var(--border-color);padding-top:0.85rem;">
            <span style="font-size:0.75rem;color:var(--text-muted);">Target: ${sub.target_hours} hrs/wk</span>
            <div style="display:flex;gap:0.5rem;">
              <button class="action-btn" style="padding:0.3rem 0.6rem;font-size:0.75rem;" onclick="app.openEditSubjectModal('${sub.id}')">Edit</button>
              <button class="btn-danger action-btn" style="padding:0.3rem 0.6rem;font-size:0.75rem;" onclick="app.deleteSubject('${sub.id}')">Delete</button>
            </div>
          </div>
        </div>
      `;
    }).join('');
  }

  // --- TASKS MANAGEMENT ---
  renderTasksList() {
    const listContainer = document.getElementById('tasks-table-body');
    if (!listContainer) return;

    if (this.tasks.length === 0) {
      listContainer.innerHTML = `
        <tr>
          <td colspan="6" style="text-align:center;padding:3rem;color:var(--text-muted);">
            No tasks found. Click "+ New Task" to schedule your studies.
          </td>
        </tr>
      `;
      return;
    }

    listContainer.innerHTML = this.tasks.map(t => {
      const sub = this.subjects.find(s => s.id === t.subject_id);
      const isDone = t.status === 'Completed';
      return `
        <tr style="border-bottom:1px solid var(--border-color);transition:background 0.2s;" class="task-row">
          <td style="padding:1rem;">
            <input type="checkbox" ${isDone ? 'checked' : ''} onchange="app.toggleTaskComplete('${t.id}')" style="width:18px;height:18px;cursor:pointer;accent-color:var(--accent-purple);">
          </td>
          <td style="padding:1rem;">
            <span style="font-weight:600;${isDone ? 'text-decoration:line-through;color:var(--text-muted);' : ''}">${this.escapeHtml(t.title)}</span>
          </td>
          <td style="padding:1rem;">
            <span style="display:inline-flex;align-items:center;gap:0.4rem;font-size:0.85rem;color:${sub ? sub.color : 'inherit'};">
              <span style="width:8px;height:8px;border-radius:50%;background:${sub ? sub.color : '#8b5cf6'};"></span>
              ${sub ? this.escapeHtml(sub.name) : 'General'}
            </span>
          </td>
          <td style="padding:1rem;font-size:0.85rem;color:var(--text-secondary);">${t.due_date || 'N/A'}</td>
          <td style="padding:1rem;">
            <span class="badge" style="background:rgba(255,255,255,0.06);padding:0.25rem 0.55rem;border-radius:var(--radius-sm);font-size:0.75rem;font-weight:600;color:${t.priority === 'High' ? 'var(--accent-rose)' : (t.priority === 'Medium' ? 'var(--accent-amber)' : 'var(--accent-mint)')};">
              ${t.priority}
            </span>
          </td>
          <td style="padding:1rem;text-align:right;">
            <button class="action-btn" style="padding:0.3rem 0.6rem;font-size:0.75rem;" onclick="app.openEditTaskModal('${t.id}')">Edit</button>
            <button class="btn-danger action-btn" style="padding:0.3rem 0.6rem;font-size:0.75rem;" onclick="app.deleteTask('${t.id}')">Delete</button>
          </td>
        </tr>
      `;
    }).join('');
  }

  async toggleTaskComplete(taskId) {
    const task = this.tasks.find(t => t.id === taskId);
    if (!task) return;

    const nextStatus = task.status === 'Completed' ? 'Pending' : 'Completed';
    try {
      const res = await fetch(`/api/tasks/${taskId}/status`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ status: nextStatus })
      });
      if (res.ok) {
        task.status = nextStatus;
        this.showToast(`Task marked as ${nextStatus}!`, 'success');
        await this.loadAllData();
      }
    } catch (e) {
      this.showToast('Failed to update task status.', 'error');
    }
  }

  // --- AI PLANNER ---
  clearAIInputs() {
    const promptInput = document.getElementById('ai-prompt-input');
    const goalInput = document.getElementById('ai-focus-goal');
    if (promptInput) promptInput.value = '';
    if (goalInput) goalInput.value = '';
    const res = document.getElementById('ai-plan-result');
    if (res) res.style.display = 'none';
    this.showToast('Inputs cleared. Type your custom topic and click Synthesize!', 'info');
  }

  async checkAIConfig() {
    try {
      const res = await fetch('/api/ai/config');
      if (res.ok) {
        const data = await res.json();
        const badge = document.getElementById('ai-engine-status-text');
        if (badge) {
          badge.textContent = data.has_key ? '⚡ Google Gemini 1.5 Flash' : '🧠 Cognitive Study Engine';
        }
      }
    } catch (e) {}
  }

  renderAIPlanner() {
    const promptInput = document.getElementById('ai-prompt-input');
    const genBtn = document.getElementById('ai-generate-btn');
    if (genBtn && !genBtn.dataset.bound) {
      genBtn.dataset.bound = 'true';
      genBtn.addEventListener('click', () => this.generateAIPlan());
    }
    this.checkAIConfig();
  }

  async generateAIPlan() {
    const promptInput = document.getElementById('ai-prompt-input');
    const hoursLimitInput = document.getElementById('ai-hours-limit');
    const sessionLenInput = document.getElementById('ai-session-len');
    const startTimeInput = document.getElementById('ai-start-time');
    const goalInput = document.getElementById('ai-focus-goal');
    const resultContainer = document.getElementById('ai-plan-result');
    const loadingEl = document.getElementById('ai-loading');

    if (loadingEl) loadingEl.style.display = 'flex';
    if (resultContainer) resultContainer.style.display = 'none';

    const payload = {
      prompt: promptInput ? promptInput.value : '',
      daily_hours_limit: hoursLimitInput ? parseInt(hoursLimitInput.value) || 4 : 4,
      preferred_session_min: sessionLenInput ? parseInt(sessionLenInput.value) || 45 : 45,
      start_time_pref: startTimeInput ? startTimeInput.value : '09:00',
      focus_goal: goalInput ? goalInput.value : ''
    };

    try {
      const res = await fetch('/api/ai/plan', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });

      if (loadingEl) loadingEl.style.display = 'none';

      if (res.ok) {
        const plan = await res.json();
        this.displayAIPlanResult(plan);
      } else {
        this.showToast('Failed to generate study plan.', 'error');
      }
    } catch (e) {
      if (loadingEl) loadingEl.style.display = 'none';
      this.showToast('Network error while requesting AI planner.', 'error');
    }
  }

  displayAIPlanResult(plan) {
    const resultContainer = document.getElementById('ai-plan-result');
    if (!resultContainer) return;

    this.pendingPlanSessions = plan.sessions || [];

    resultContainer.innerHTML = `
      <div class="glass-card" style="border: 1px solid var(--accent-purple);box-shadow:var(--shadow-glow);margin-top:2rem;position:relative;overflow:hidden;">
        <div style="position:absolute;top:0;left:0;right:0;height:4px;background:var(--gradient-cosmic);"></div>
        
        <div style="display:flex;justify-content:space-between;align-items:flex-start;border-bottom:1px solid var(--border-color);padding-bottom:1.25rem;margin-bottom:1.5rem;flex-wrap:wrap;gap:1rem;">
          <div>
            <div style="display:flex;align-items:center;gap:0.75rem;">
              <span style="font-size:1.8rem;">✨</span>
              <div>
                <h3 style="font-size:1.3rem;font-weight:800;letter-spacing:-0.02em;">Generated Study Timetable</h3>
                <span style="display:inline-block;padding:0.2rem 0.65rem;border-radius:var(--radius-full);background:rgba(139,92,246,0.18);border:1px solid rgba(139,92,246,0.35);color:var(--accent-purple);font-size:0.75rem;font-weight:700;margin-top:0.25rem;">
                  ⚡ Engine: ${plan.provider_name}
                </span>
              </div>
            </div>
            <p style="font-size:0.92rem;color:var(--text-secondary);margin-top:0.6rem;max-width:700px;line-height:1.5;">${this.escapeHtml(plan.summary)}</p>
          </div>
          
          <div style="display:flex;gap:0.75rem;align-items:center;">
            <button class="btn-primary action-btn" style="padding:0.75rem 1.5rem;font-size:0.95rem;box-shadow:var(--shadow-glow);" onclick="app.acceptProposedPlan()">
              🚀 Accept &amp; Sync to Calendar
            </button>
          </div>
        </div>

        ${plan.conflict_warnings && plan.conflict_warnings.length > 0 ? `
          <div style="background:rgba(244,63,94,0.12);border:1px solid var(--accent-rose);border-radius:var(--radius-md);padding:1rem 1.25rem;margin-bottom:1.5rem;font-size:0.88rem;color:var(--accent-rose);display:flex;align-items:flex-start;gap:0.75rem;">
            <span style="font-size:1.2rem;">⚠️</span>
            <div>
              <strong style="display:block;margin-bottom:0.25rem;">Schedule Advisory:</strong>
              ${plan.conflict_warnings.join('<br>')}
            </div>
          </div>
        ` : ''}

        <div style="margin-bottom:1.75rem;background:rgba(255,255,255,0.02);border:1px solid var(--border-color);border-radius:var(--radius-md);padding:1.25rem;">
          <h4 style="font-size:0.96rem;font-weight:700;margin-bottom:0.75rem;color:var(--accent-cyan);display:flex;align-items:center;gap:0.5rem;">
            <span>💡</span> Cognitive Retention Strategy
          </h4>
          <div style="display:grid;grid-template-columns:repeat(auto-fit, minmax(280px, 1fr));gap:0.75rem;">
            ${(plan.recommendations || []).map(r => `
              <div style="display:flex;align-items:flex-start;gap:0.5rem;font-size:0.86rem;color:var(--text-secondary);">
                <span style="color:var(--accent-purple);">✓</span>
                <span>${this.escapeHtml(r)}</span>
              </div>
            `).join('')}
          </div>
        </div>

        <div>
          <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem;">
            <h4 style="font-size:1.05rem;font-weight:800;">Proposed Study Sessions (${this.pendingPlanSessions.length})</h4>
            <span style="font-size:0.82rem;color:var(--accent-cyan);font-weight:600;">Total Planned: ${plan.total_planned_hours} Hours</span>
          </div>

          <div style="display:flex;flex-direction:column;gap:0.85rem;">
            ${(plan.sessions || []).map((s, idx) => `
              <div style="background:rgba(255,255,255,0.03);border:1px solid var(--border-color);border-left:4px solid var(--accent-purple);border-radius:var(--radius-md);padding:1rem 1.25rem;display:flex;justify-content:space-between;align-items:center;transition:all 0.2s;" onmouseover="this.style.borderColor='var(--accent-purple)'" onmouseout="this.style.borderColor='var(--border-color)'">
                <div>
                  <div style="display:flex;align-items:center;gap:0.6rem;">
                    <span style="font-weight:700;font-size:0.98rem;color:var(--text-primary);">${this.escapeHtml(s.activity)}</span>
                    <span class="badge" style="background:rgba(139,92,246,0.15);color:var(--accent-purple);padding:0.2rem 0.55rem;border-radius:var(--radius-sm);font-size:0.75rem;font-weight:700;">
                      ${this.escapeHtml(s.subject_name)}
                    </span>
                    <span class="badge" style="background:rgba(255,255,255,0.06);padding:0.2rem 0.55rem;border-radius:var(--radius-sm);font-size:0.72rem;font-weight:600;color:${s.priority === 'High' ? 'var(--accent-rose)' : 'var(--accent-amber)'};">
                      ${s.priority}
                    </span>
                  </div>
                  
                  <div style="font-size:0.82rem;color:var(--text-muted);margin-top:0.35rem;display:flex;gap:1rem;flex-wrap:wrap;">
                    <span>📅 ${s.date}</span>
                    <span>⏰ ${s.start_time} - ${s.end_time} (${s.duration_minutes}m)</span>
                    <span style="color:var(--text-secondary);">🎯 ${this.escapeHtml(s.reason)}</span>
                  </div>
                </div>

                <div style="display:flex;align-items:center;gap:0.75rem;">
                  <span class="badge" style="background:rgba(6,182,212,0.12);border:1px solid rgba(6,182,212,0.25);padding:0.35rem 0.75rem;border-radius:var(--radius-sm);font-size:0.75rem;font-weight:600;color:var(--accent-cyan);">
                    ☕ ${this.escapeHtml(s.break_after || '15m break')}
                  </span>
                </div>
              </div>
            `).join('')}
          </div>
        </div>
      </div>
    `;
    resultContainer.style.display = 'block';
    resultContainer.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  async acceptProposedPlan() {
    if (!this.pendingPlanSessions || this.pendingPlanSessions.length === 0) return;

    try {
      const res = await fetch('/api/ai/plan/accept', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ sessions: this.pendingPlanSessions })
      });
      if (res.ok) {
        this.showToast('AI Plan accepted and synced to your Timetable!', 'success');
        await this.loadAllData();
        this.switchView('timetable');
      } else {
        this.showToast('Failed to save timetable sessions.', 'error');
      }
    } catch (e) {
      this.showToast('Network error while saving plan.', 'error');
    }
  }

  // --- TIMETABLE CALENDAR ---
  renderTimetable() {
    const container = document.getElementById('timetable-grid');
    if (!container) return;

    if (this.studySessions.length === 0) {
      container.innerHTML = `
        <div style="text-align:center;padding:4rem;color:var(--text-muted);">
          <div style="font-size:3rem;margin-bottom:1rem;">🗓️</div>
          <h3>Your calendar is currently empty</h3>
          <p style="margin-top:0.5rem;">Use the AI Planner or add manual sessions to build your weekly schedule.</p>
          <button class="btn-primary action-btn" style="margin-top:1.25rem;" onclick="app.switchView('ai-planner')">✨ Generate AI Study Plan</button>
        </div>
      `;
      return;
    }

    // Group sessions by date
    const byDate = {};
    this.studySessions.forEach(s => {
      if (!byDate[s.date]) byDate[s.date] = [];
      byDate[s.date].push(s);
    });

    const sortedDates = Object.keys(byDate).sort();

    container.innerHTML = `
      <div style="display:grid;grid-template-columns:repeat(auto-fit, minmax(280px, 1fr));gap:1.5rem;">
        ${sortedDates.map(date => {
          const sessions = byDate[date];
          return `
            <div class="glass-card" style="border-top:3px solid var(--accent-purple);">
              <h3 style="font-size:1.05rem;font-weight:700;margin-bottom:1rem;display:flex;align-items:center;gap:0.5rem;">
                <span>📅</span> ${date}
              </h3>
              <div style="display:flex;flex-direction:column;gap:0.75rem;">
                ${sessions.map(s => {
                  const sub = this.subjects.find(sub => sub.id === s.subject_id);
                  const color = sub ? sub.color : '#8b5cf6';
                  return `
                    <div style="background:rgba(255,255,255,0.03);border:1px solid var(--border-color);border-left:4px solid ${color};border-radius:var(--radius-sm);padding:0.75rem;">
                      <div style="display:flex;justify-content:space-between;align-items:center;">
                        <span style="font-weight:600;font-size:0.88rem;">${this.escapeHtml(s.activity)}</span>
                        <input type="checkbox" ${s.is_completed ? 'checked' : ''} onchange="app.toggleSessionComplete('${s.id}')" style="accent-color:var(--accent-purple);">
                      </div>
                      <div style="font-size:0.75rem;color:var(--text-muted);margin-top:0.3rem;">
                        ${s.start_time} - ${s.end_time} (${s.duration_minutes}m)
                      </div>
                      ${s.reason ? `<div style="font-size:0.72rem;color:var(--text-secondary);margin-top:0.25rem;">${this.escapeHtml(s.reason)}</div>` : ''}
                      <div style="display:flex;justify-content:flex-end;margin-top:0.5rem;">
                        <button class="btn-danger action-btn" style="padding:0.15rem 0.45rem;font-size:0.7rem;" onclick="app.deleteSession('${s.id}')">Remove</button>
                      </div>
                    </div>
                  `;
                }).join('')}
              </div>
            </div>
          `;
        }).join('')}
      </div>
    `;
  }

  async toggleSessionComplete(sessionId) {
    const s = this.studySessions.find(item => item.id === sessionId);
    if (!s) return;
    const nextVal = !s.is_completed;
    try {
      const res = await fetch(`/api/planner/sessions/${sessionId}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ is_completed: nextVal })
      });
      if (res.ok) {
        s.is_completed = nextVal;
        this.showToast('Session updated!', 'success');
        await this.loadAllData();
      }
    } catch (e) {
      this.showToast('Error updating session', 'error');
    }
  }

  async deleteSession(sessionId) {
    try {
      const res = await fetch(`/api/planner/sessions/${sessionId}`, { method: 'DELETE' });
      if (res.ok) {
        this.showToast('Session removed from timetable', 'info');
        await this.loadAllData();
      }
    } catch (e) {
      this.showToast('Error deleting session', 'error');
    }
  }

  // --- FOCUS TIMER ---
  setupFocusTimer() {
    const startBtn = document.getElementById('timer-start-btn');
    const resetBtn = document.getElementById('timer-reset-btn');
    const modeBtns = document.querySelectorAll('.timer-mode-btn');

    if (startBtn) {
      startBtn.addEventListener('click', () => this.toggleTimer());
    }

    if (resetBtn) {
      resetBtn.addEventListener('click', () => this.resetTimer());
    }

    modeBtns.forEach(b => {
      b.addEventListener('click', () => {
        modeBtns.forEach(btn => btn.classList.remove('active'));
        b.classList.add('active');
        const mode = b.getAttribute('data-mode');
        this.setTimerMode(mode);
      });
    });

    const subSelect = document.getElementById('timer-subject-select');
    if (subSelect) {
      subSelect.addEventListener('change', (e) => {
        this.selectedFocusSubjectId = e.target.value;
      });
    }
  }

  setTimerMode(mode) {
    this.timerMode = mode;
    this.pauseTimer();
    if (mode === 'pomodoro') {
      this.timerTotalSeconds = 25 * 60;
    } else if (mode === 'short_break') {
      this.timerTotalSeconds = 5 * 60;
    } else if (mode === 'long_break') {
      this.timerTotalSeconds = 15 * 60;
    }
    this.timerSeconds = this.timerTotalSeconds;
    this.updateTimerDisplay();
  }

  toggleTimer() {
    if (this.timerIsRunning) {
      this.pauseTimer();
    } else {
      this.startTimer();
    }
  }

  startTimer() {
    this.timerIsRunning = true;
    const startBtn = document.getElementById('timer-start-btn');
    if (startBtn) {
      startBtn.textContent = 'Pause Focus';
      startBtn.style.background = 'var(--accent-amber)';
    }

    this.timerInterval = setInterval(() => {
      if (this.timerSeconds > 0) {
        this.timerSeconds--;
        this.updateTimerDisplay();
      } else {
        this.completeTimer();
      }
    }, 1000);
  }

  pauseTimer() {
    this.timerIsRunning = false;
    clearInterval(this.timerInterval);
    const startBtn = document.getElementById('timer-start-btn');
    if (startBtn) {
      startBtn.textContent = 'Start Focus';
      startBtn.style.background = 'var(--gradient-cosmic)';
    }
  }

  resetTimer() {
    this.pauseTimer();
    this.timerSeconds = this.timerTotalSeconds;
    this.updateTimerDisplay();
  }

  updateTimerDisplay() {
    const mins = Math.floor(this.timerSeconds / 60);
    const secs = this.timerSeconds % 60;
    const display = document.getElementById('timer-display');
    if (display) {
      display.textContent = `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
    }

    const ring = document.getElementById('timer-progress-ring');
    if (ring) {
      const pct = (this.timerSeconds / this.timerTotalSeconds);
      const circumference = 2 * Math.PI * 90; // r=90
      ring.style.strokeDashoffset = circumference * (1 - pct);
    }
  }

  async completeTimer() {
    this.pauseTimer();
    const durationMins = Math.round(this.timerTotalSeconds / 60);
    this.showToast(`🎉 Focus session completed (${durationMins}m)! Recorded to analytics.`, 'success');

    // Record session to backend
    try {
      await fetch('/api/focus/sessions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          subject_id: this.selectedFocusSubjectId,
          duration_minutes: durationMins,
          mode: this.timerMode
        })
      });
      await this.loadAllData();
    } catch (e) {
      console.warn('Failed to save focus session', e);
    }

    this.resetTimer();
  }

  // --- PROGRESS & ACHIEVEMENTS ---
  async renderProgress() {
    try {
      const [sumRes, subRes, achRes] = await Promise.all([
        fetch('/api/progress/summary'),
        fetch('/api/progress/subjects'),
        fetch('/api/achievements')
      ]);

      const summary = sumRes.ok ? await sumRes.json() : {};
      const subProg = subRes.ok ? await subRes.json() : [];
      const achs = achRes.ok ? await achRes.json() : [];

      const streakEl = document.getElementById('prog-streak-val');
      const totalMinsEl = document.getElementById('prog-total-mins');
      const compPctEl = document.getElementById('prog-comp-val');
      if (streakEl) streakEl.textContent = `${summary.streak_days || 0} Days`;
      if (totalMinsEl) totalMinsEl.textContent = `${summary.total_study_minutes || 0} Mins`;
      if (compPctEl) compPctEl.textContent = `${Math.round(summary.completion_percentage || 0)}%`;

      // Render Subject Progress Cards
      const subProgContainer = document.getElementById('prog-subjects-container');
      if (subProgContainer) {
        subProgContainer.innerHTML = subProg.map(s => `
          <div class="glass-card" style="margin-bottom:1rem;border-left:4px solid ${s.color};">
            <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem;">
              <span style="font-weight:600;">${this.escapeHtml(s.name)}</span>
              <span style="font-size:0.85rem;color:var(--text-muted);">${s.completed_tasks} / ${s.total_tasks} Tasks Done (${Math.round(s.progress_percentage)}%)</span>
            </div>
            <div style="width:100%;height:8px;background:rgba(255,255,255,0.06);border-radius:var(--radius-full);overflow:hidden;">
              <div style="width:${s.progress_percentage}%;height:100%;background:${s.color};border-radius:var(--radius-full);"></div>
            </div>
          </div>
        `).join('');
      }

      // Render Achievements
      const achContainer = document.getElementById('prog-achievements-container');
      if (achContainer) {
        if (achs.length === 0) {
          achContainer.innerHTML = `<p style="color:var(--text-muted);">Complete study tasks and focus sessions to unlock milestone badges!</p>`;
        } else {
          achContainer.innerHTML = achs.map(a => `
            <div class="glass-card" style="display:flex;align-items:center;gap:1rem;padding:1rem;">
              <div style="font-size:2rem;">🏆</div>
              <div>
                <h4 style="font-weight:700;font-size:0.95rem;">${this.escapeHtml(a.title)}</h4>
                <p style="font-size:0.78rem;color:var(--text-secondary);">${this.escapeHtml(a.description)}</p>
                <span style="font-size:0.7rem;color:var(--accent-cyan);">Unlocked ${a.unlocked_at ? a.unlocked_at.substr(0,10) : 'Recently'}</span>
              </div>
            </div>
          `).join('');
        }
      }
    } catch (e) {
      console.warn('Error loading progress view:', e);
    }
  }

  // --- MODALS & FORMS ---
  setupModals() {
    // New Subject Modal
    const newSubBtn = document.getElementById('btn-new-subject');
    if (newSubBtn) newSubBtn.addEventListener('click', () => this.openNewSubjectModal());

    const saveSubBtn = document.getElementById('modal-save-subject');
    if (saveSubBtn) {
      saveSubBtn.addEventListener('click', () => this.saveSubject());
    }

    // New Task Modal
    const newTaskBtn = document.getElementById('btn-new-task');
    if (newTaskBtn) newTaskBtn.addEventListener('click', () => this.openNewTaskModal());

    const saveTaskBtn = document.getElementById('modal-save-task');
    if (saveTaskBtn) {
      saveTaskBtn.addEventListener('click', () => this.saveTask());
    }

    // Close buttons
    document.querySelectorAll('.modal-close').forEach(btn => {
      btn.addEventListener('click', () => this.closeAllModals());
    });
  }

  openAuthModal(mode) {
    const modal = document.getElementById('auth-modal');
    if (!modal) return;
    const title = document.getElementById('auth-modal-title');
    const submitBtn = document.getElementById('auth-submit-btn');
    const nameGroup = document.getElementById('auth-name-group');
    const emailGroup = document.getElementById('auth-email-group');
    const switchText = document.getElementById('auth-switch-text');

    modal.dataset.mode = mode;
    if (mode === 'signup') {
      if (title) title.textContent = 'Create Your Free Account';
      if (submitBtn) submitBtn.textContent = 'Create Account';
      if (nameGroup) nameGroup.style.display = 'block';
      if (emailGroup) emailGroup.style.display = 'block';
      if (switchText) switchText.innerHTML = 'Already have an account? <a href="#" onclick="app.openAuthModal(\'login\');return false;" style="color:var(--accent-purple);">Log in</a>';
    } else {
      if (title) title.textContent = 'Welcome Back to StudyMate 3D';
      if (submitBtn) submitBtn.textContent = 'Sign In';
      if (nameGroup) nameGroup.style.display = 'none';
      if (emailGroup) emailGroup.style.display = 'none';
      if (switchText) switchText.innerHTML = 'New here? <a href="#" onclick="app.openAuthModal(\'signup\');return false;" style="color:var(--accent-purple);">Create account</a>';
    }

    modal.style.display = 'flex';

    // Submit handler
    if (!submitBtn.dataset.bound) {
      submitBtn.dataset.bound = 'true';
      submitBtn.addEventListener('click', () => this.handleAuthSubmit());
    }
  }

  async handleAuthSubmit() {
    const modal = document.getElementById('auth-modal');
    const mode = modal.dataset.mode;
    const name = document.getElementById('auth-name')?.value;
    const email = document.getElementById('auth-email')?.value;
    const username = document.getElementById('auth-username')?.value;
    const password = document.getElementById('auth-password')?.value;

    if (mode === 'signup') {
      try {
        const res = await fetch('/api/auth/signup', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ full_name: name, email, username, password })
        });
        const data = await res.json();
        if (res.ok) {
          this.currentUser = data.user;
          this.closeAllModals();
          this.showToast('Account created successfully!', 'success');
          this.showApp();
          await this.loadAllData();
        } else {
          this.showToast(data.error || 'Signup failed', 'error');
        }
      } catch (e) {
        this.showToast('Error during signup', 'error');
      }
    } else {
      try {
        const res = await fetch('/api/auth/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ username_or_email: username, password })
        });
        const data = await res.json();
        if (res.ok) {
          this.currentUser = data.user;
          this.closeAllModals();
          this.showToast('Welcome back!', 'success');
          this.showApp();
          await this.loadAllData();
        } else {
          this.showToast(data.error || 'Invalid credentials', 'error');
        }
      } catch (e) {
        this.showToast('Error during login', 'error');
      }
    }
  }

  openNewSubjectModal() {
    document.getElementById('subject-modal-id').value = '';
    document.getElementById('subject-name').value = '';
    document.getElementById('subject-desc').value = '';
    document.getElementById('subject-color').value = '#8b5cf6';
    document.getElementById('subject-hours').value = '10';
    document.getElementById('subject-difficulty').value = 'Medium';
    document.getElementById('subject-modal-title').textContent = 'Create New Subject';
    document.getElementById('subject-modal').style.display = 'flex';
  }

  openEditSubjectModal(subId) {
    const sub = this.subjects.find(s => s.id === subId);
    if (!sub) return;
    document.getElementById('subject-modal-id').value = sub.id;
    document.getElementById('subject-name').value = sub.name;
    document.getElementById('subject-desc').value = sub.description || '';
    document.getElementById('subject-color').value = sub.color || '#8b5cf6';
    document.getElementById('subject-hours').value = sub.target_hours || 10;
    document.getElementById('subject-difficulty').value = sub.difficulty || 'Medium';
    document.getElementById('subject-modal-title').textContent = 'Edit Subject';
    document.getElementById('subject-modal').style.display = 'flex';
  }

  async saveSubject() {
    const id = document.getElementById('subject-modal-id').value;
    const name = document.getElementById('subject-name').value.trim();
    const description = document.getElementById('subject-desc').value.trim();
    const color = document.getElementById('subject-color').value;
    const target_hours = parseFloat(document.getElementById('subject-hours').value) || 10;
    const difficulty = document.getElementById('subject-difficulty').value;

    if (!name) {
      this.showToast('Please enter a subject name', 'warning');
      return;
    }

    const payload = { name, description, color, target_hours, difficulty };
    try {
      let res;
      if (id) {
        res = await fetch(`/api/subjects/${id}`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
      } else {
        res = await fetch('/api/subjects', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
      }

      if (res.ok) {
        this.closeAllModals();
        this.showToast(`Subject ${id ? 'updated' : 'created'} successfully!`, 'success');
        await this.loadAllData();
      } else {
        this.showToast('Error saving subject', 'error');
      }
    } catch (e) {
      this.showToast('Network error', 'error');
    }
  }

  async deleteSubject(id) {
    if (!confirm('Are you sure you want to delete this subject?')) return;
    try {
      const res = await fetch(`/api/subjects/${id}`, { method: 'DELETE' });
      if (res.ok) {
        this.showToast('Subject deleted', 'info');
        await this.loadAllData();
      }
    } catch (e) {
      this.showToast('Error deleting subject', 'error');
    }
  }

  openNewTaskModal() {
    this.populateSubjectSelects();
    document.getElementById('task-modal-id').value = '';
    document.getElementById('task-title').value = '';
    document.getElementById('task-desc').value = '';
    document.getElementById('task-due').value = '';
    document.getElementById('task-duration').value = '45';
    document.getElementById('task-priority').value = 'Medium';
    document.getElementById('task-modal-title').textContent = 'Create Study Task';
    document.getElementById('task-modal').style.display = 'flex';
  }

  openEditTaskModal(taskId) {
    const task = this.tasks.find(t => t.id === taskId);
    if (!task) return;
    this.populateSubjectSelects();
    document.getElementById('task-modal-id').value = task.id;
    document.getElementById('task-title').value = task.title;
    document.getElementById('task-subject').value = task.subject_id;
    document.getElementById('task-desc').value = task.description || '';
    document.getElementById('task-due').value = task.due_date || '';
    document.getElementById('task-duration').value = task.estimated_minutes || 45;
    document.getElementById('task-priority').value = task.priority || 'Medium';
    document.getElementById('task-modal-title').textContent = 'Edit Task';
    document.getElementById('task-modal').style.display = 'flex';
  }

  async saveTask() {
    const id = document.getElementById('task-modal-id').value;
    const title = document.getElementById('task-title').value.trim();
    const subject_id = document.getElementById('task-subject').value;
    const description = document.getElementById('task-desc').value.trim();
    const due_date = document.getElementById('task-due').value;
    const estimated_minutes = parseInt(document.getElementById('task-duration').value) || 45;
    const priority = document.getElementById('task-priority').value;

    if (!title) {
      this.showToast('Task title is required', 'warning');
      return;
    }

    const payload = { title, subject_id, description, due_date, estimated_minutes, priority };
    try {
      let res;
      if (id) {
        res = await fetch(`/api/tasks/${id}`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
      } else {
        res = await fetch('/api/tasks', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
      }

      if (res.ok) {
        this.closeAllModals();
        this.showToast(`Task ${id ? 'updated' : 'created'}!`, 'success');
        await this.loadAllData();
      } else {
        this.showToast('Error saving task', 'error');
      }
    } catch (e) {
      this.showToast('Network error', 'error');
    }
  }

  async deleteTask(id) {
    if (!confirm('Are you sure you want to delete this task?')) return;
    try {
      const res = await fetch(`/api/tasks/${id}`, { method: 'DELETE' });
      if (res.ok) {
        this.showToast('Task deleted', 'info');
        await this.loadAllData();
      }
    } catch (e) {
      this.showToast('Error deleting task', 'error');
    }
  }

  populateSubjectSelects() {
    const selects = [
      document.getElementById('task-subject'),
      document.getElementById('timer-subject-select')
    ];
    selects.forEach(sel => {
      if (!sel) return;
      const current = sel.value;
      sel.innerHTML = '<option value="">-- Select Subject --</option>' +
        this.subjects.map(s => `<option value="${s.id}">${this.escapeHtml(s.name)}</option>`).join('');
      if (current) sel.value = current;
    });
  }

  closeAllModals() {
    document.querySelectorAll('.modal-backdrop').forEach(m => m.style.display = 'none');
  }

  // --- TOAST NOTIFICATIONS ---
  showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = 'toast';
    const icon = type === 'success' ? '✅' : (type === 'error' ? '❌' : (type === 'warning' ? '⚠️' : 'ℹ️'));
    toast.innerHTML = `<span>${icon}</span> <span>${this.escapeHtml(message)}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateY(10px)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 3500);
  }

  escapeHtml(str) {
    if (!str) return '';
    return str.replace(/[&<>'"]/g, tag => ({
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      "'": '&#39;',
      '"': '&quot;'
    }[tag] || tag));
  }
}

// Global initialization
window.addEventListener('DOMContentLoaded', () => {
  window.app = new StudyMateApp();
});
