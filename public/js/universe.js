// Three.js Interactive Study Universe Engine (Upgraded Celestial Shaders & Glow)
// Renders central glowing study planet with atmospheric glow, dynamic subject satellites, and task asteroids

class StudyUniverse {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    if (!this.container) return;

    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.centralPlanet = null;
    this.glowAtmosphere = null;
    this.stars = null;
    this.subjectMeshes = [];
    this.taskMeshes = [];
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2(-1000, -1000);
    this.hoveredObject = null;
    this.tooltip = document.getElementById('universe-tooltip');
    this.isDragging = false;
    this.previousMousePosition = { x: 0, y: 0 };
    this.rotationGroup = null;
    this.animationFrameId = null;

    this.init();
  }

  init() {
    try {
      const width = this.container.clientWidth || 800;
      const height = this.container.clientHeight || 520;

      // 1. Scene
      this.scene = new THREE.Scene();
      this.scene.fog = new THREE.FogExp2(0x070913, 0.008);

      // 2. Camera
      this.camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 1000);
      this.camera.position.set(0, 26, 48);
      this.camera.lookAt(0, 0, 0);

      // 3. Renderer with antialiasing and tone mapping
      this.renderer = new THREE.WebGLRenderer({
        canvas: document.getElementById('universe-canvas'),
        antialias: true,
        alpha: true
      });
      this.renderer.setSize(width, height);
      this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

      // 4. Lighting Setup
      const ambientLight = new THREE.AmbientLight(0xffffff, 0.9);
      this.scene.add(ambientLight);

      const centerLight = new THREE.PointLight(0xa855f7, 3.5, 120);
      centerLight.position.set(0, 0, 0);
      this.scene.add(centerLight);

      const cyanRimLight = new THREE.DirectionalLight(0x06b6d4, 1.8);
      cyanRimLight.position.set(30, 45, 25);
      this.scene.add(cyanRimLight);

      const roseFillLight = new THREE.DirectionalLight(0xec4899, 1.2);
      roseFillLight.position.set(-30, -20, -20);
      this.scene.add(roseFillLight);

      // Group for interactive orbital rotation
      this.rotationGroup = new THREE.Group();
      this.scene.add(this.rotationGroup);

      // 5. Central Study Planet with Atmosphere
      this.createCentralPlanet();

      // 6. Deep Cosmic Starfield
      this.createStarfield();

      // 7. Event Listeners
      this.setupEvents();

      // 8. Animation Loop
      this.animate();
    } catch (e) {
      console.warn("WebGL not supported or error initializing 3D universe:", e);
      if (this.container) {
        this.container.innerHTML = `
          <div style="display:flex;align-items:center;justify-content:center;height:100%;color:#94a3b8;flex-direction:column;gap:1rem;">
            <div style="font-size:3rem;">🪐</div>
            <p style="font-size:1.15rem;font-weight:700;">Study Universe Active</p>
            <p style="font-size:0.85rem;">Displaying real-time academic orbits.</p>
          </div>
        `;
      }
    }
  }

  createCentralPlanet() {
    // Planet Core
    const geometry = new THREE.SphereGeometry(6.2, 48, 48);
    const material = new THREE.MeshPhongMaterial({
      color: 0x4f46e5,
      emissive: 0x2e1065,
      shininess: 60,
      flatShading: false
    });
    this.centralPlanet = new THREE.Mesh(geometry, material);
    this.centralPlanet.userData = { type: 'core', title: 'Academic Study Core' };
    this.rotationGroup.add(this.centralPlanet);

    // Glowing Atmospheric Aura
    const glowGeo = new THREE.SphereGeometry(6.9, 32, 32);
    const glowMat = new THREE.MeshBasicMaterial({
      color: 0x8b5cf6,
      transparent: true,
      opacity: 0.22,
      side: THREE.BackSide
    });
    this.glowAtmosphere = new THREE.Mesh(glowGeo, glowMat);
    this.rotationGroup.add(this.glowAtmosphere);

    // Orbital Ring 1 (Cyan)
    const ringGeo1 = new THREE.RingGeometry(8.5, 8.8, 64);
    const ringMat1 = new THREE.MeshBasicMaterial({
      color: 0x06b6d4,
      side: THREE.DoubleSide,
      transparent: true,
      opacity: 0.75
    });
    const ring1 = new THREE.Mesh(ringGeo1, ringMat1);
    ring1.rotation.x = Math.PI / 2.2;
    this.rotationGroup.add(ring1);

    // Orbital Ring 2 (Purple)
    const ringGeo2 = new THREE.RingGeometry(11.5, 11.75, 64);
    const ringMat2 = new THREE.MeshBasicMaterial({
      color: 0xec4899,
      side: THREE.DoubleSide,
      transparent: true,
      opacity: 0.45
    });
    const ring2 = new THREE.Mesh(ringGeo2, ringMat2);
    ring2.rotation.x = Math.PI / 2.5;
    ring2.rotation.y = 0.3;
    this.rotationGroup.add(ring2);
  }

  createStarfield() {
    const starCount = 500;
    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(starCount * 3);
    const colors = new Float32Array(starCount * 3);

    for (let i = 0; i < starCount * 3; i += 3) {
      positions[i] = (Math.random() - 0.5) * 220;
      positions[i + 1] = (Math.random() - 0.5) * 220;
      positions[i + 2] = (Math.random() - 0.5) * 220;

      // Color variation in stars: cyan, white, and violet
      const rChoice = Math.random();
      if (rChoice > 0.6) {
        colors[i] = 0.54; colors[i+1] = 0.36; colors[i+2] = 0.96; // violet
      } else if (rChoice > 0.3) {
        colors[i] = 0.02; colors[i+1] = 0.71; colors[i+2] = 0.83; // cyan
      } else {
        colors[i] = 1.0; colors[i+1] = 1.0; colors[i+2] = 1.0;   // pure star
      }
    }

    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

    const material = new THREE.PointsMaterial({
      size: 1.1,
      vertexColors: true,
      transparent: true,
      opacity: 0.85
    });
    this.stars = new THREE.Points(geometry, material);
    this.scene.add(this.stars);
  }

  updateData(subjects, tasks) {
    if (!this.rotationGroup) return;

    // Clear previous meshes
    this.subjectMeshes.forEach(m => this.rotationGroup.remove(m));
    this.taskMeshes.forEach(m => this.rotationGroup.remove(m));
    this.subjectMeshes = [];
    this.taskMeshes = [];

    // 1. If student has subjects, render them
    const displaySubjects = (subjects && subjects.length > 0) ? subjects : [
      { id: 'sub_demo1', name: 'Core Sciences', color: '#8b5cf6', target_hours: 10, difficulty: 'Medium' },
      { id: 'sub_demo2', name: 'Mathematics', color: '#06b6d4', target_hours: 8, difficulty: 'Hard' }
    ];

    const subRadius = 15;
    displaySubjects.forEach((sub, idx) => {
      const angle = (idx / displaySubjects.length) * Math.PI * 2;
      const x = Math.cos(angle) * subRadius;
      const z = Math.sin(angle) * subRadius;
      const y = Math.sin(idx * 2) * 2.5;

      let colorHex = 0x8b5cf6;
      if (sub.color) {
        colorHex = parseInt(sub.color.replace('#', '0x')) || 0x8b5cf6;
      }

      // Subject satellite
      const geo = new THREE.DodecahedronGeometry(2.2, 0);
      const mat = new THREE.MeshStandardMaterial({
        color: colorHex,
        roughness: 0.25,
        metalness: 0.65,
        emissive: colorHex,
        emissiveIntensity: 0.2
      });
      const mesh = new THREE.Mesh(geo, mat);
      mesh.position.set(x, y, z);
      mesh.userData = {
        type: 'subject',
        id: sub.id,
        name: sub.name,
        target_hours: sub.target_hours,
        difficulty: sub.difficulty
      };

      this.rotationGroup.add(mesh);
      this.subjectMeshes.push(mesh);
    });

    // 2. Render Task Asteroids
    const pendingTasks = (tasks && tasks.length > 0) 
      ? tasks.filter(t => t.status !== 'Completed')
      : [
        { id: 'tsk_demo1', title: 'Review Syllabus & Chapters', priority: 'High', due_date: 'Soon', estimated_minutes: 45 },
        { id: 'tsk_demo2', title: 'Practice Problem Set 1', priority: 'Medium', due_date: 'Tomorrow', estimated_minutes: 60 }
      ];

    const taskRadius = 23;
    pendingTasks.forEach((t, idx) => {
      const angle = (idx / (pendingTasks.length || 1)) * Math.PI * 2 + 0.45;
      const x = Math.cos(angle) * taskRadius;
      const z = Math.sin(angle) * taskRadius;
      const y = ((idx % 3) - 1) * 4.0;

      const geo = new THREE.OctahedronGeometry(1.4, 0);
      const color = t.priority === 'High' ? 0xf43f5e : (t.priority === 'Medium' ? 0xf59e0b : 0x10b981);
      const mat = new THREE.MeshStandardMaterial({
        color: color,
        roughness: 0.35,
        metalness: 0.4,
        emissive: color,
        emissiveIntensity: 0.3
      });
      const mesh = new THREE.Mesh(geo, mat);
      mesh.position.set(x, y, z);
      mesh.userData = {
        type: 'task',
        id: t.id,
        title: t.title,
        priority: t.priority,
        due_date: t.due_date,
        estimated_minutes: t.estimated_minutes
      };

      this.rotationGroup.add(mesh);
      this.taskMeshes.push(mesh);
    });
  }

  setupEvents() {
    const canvas = document.getElementById('universe-canvas');
    if (!canvas) return;

    window.addEventListener('resize', () => this.onResize());

    canvas.addEventListener('mousedown', (e) => {
      this.isDragging = true;
      this.previousMousePosition = { x: e.clientX, y: e.clientY };
    });

    window.addEventListener('mouseup', () => {
      this.isDragging = false;
    });

    canvas.addEventListener('mousemove', (e) => {
      const rect = canvas.getBoundingClientRect();
      this.mouse.x = ((e.clientX - rect.left) / canvas.clientWidth) * 2 - 1;
      this.mouse.y = -((e.clientY - rect.top) / canvas.clientHeight) * 2 + 1;

      if (this.isDragging && this.rotationGroup) {
        const deltaX = e.clientX - this.previousMousePosition.x;
        const deltaY = e.clientY - this.previousMousePosition.y;

        this.rotationGroup.rotation.y += deltaX * 0.007;
        this.rotationGroup.rotation.x += deltaY * 0.007;

        this.previousMousePosition = { x: e.clientX, y: e.clientY };
      }

      this.checkHover(e);
    });

    // Zoom controls
    canvas.addEventListener('wheel', (e) => {
      e.preventDefault();
      this.camera.position.z += e.deltaY * 0.035;
      this.camera.position.z = Math.max(18, Math.min(85, this.camera.position.z));
    });

    // Click on objects to inspect or edit
    canvas.addEventListener('click', () => {
      if (this.hoveredObject) {
        const data = this.hoveredObject.userData;
        if (data.type === 'subject') {
          window.app?.openEditSubjectModal?.(data.id);
        } else if (data.type === 'task') {
          window.app?.openEditTaskModal?.(data.id);
        }
      }
    });

    // Reset View button
    const resetBtn = document.getElementById('reset-universe-btn');
    if (resetBtn) {
      resetBtn.addEventListener('click', () => this.resetView());
    }
  }

  resetView() {
    if (this.rotationGroup) {
      this.rotationGroup.rotation.set(0, 0, 0);
    }
    this.camera.position.set(0, 26, 48);
    this.camera.lookAt(0, 0, 0);
  }

  checkHover(e) {
    if (!this.camera || !this.scene) return;

    this.raycaster.setFromCamera(this.mouse, this.camera);
    const intersects = this.raycaster.intersectObjects([...this.subjectMeshes, ...this.taskMeshes]);

    if (intersects.length > 0) {
      const obj = intersects[0].object;
      this.hoveredObject = obj;
      document.body.style.cursor = 'pointer';

      if (this.tooltip) {
        const data = obj.userData;
        let content = '';
        if (data.type === 'subject') {
          content = `<strong style="font-size:0.95rem;color:#ffffff;">🪐 ${data.name}</strong><br><span style="color:#06b6d4;font-size:0.8rem;">Target: ${data.target_hours}h/wk • ${data.difficulty}</span>`;
        } else if (data.type === 'task') {
          content = `<strong style="font-size:0.95rem;color:#ffffff;">☄️ ${data.title}</strong><br><span style="color:#f59e0b;font-size:0.8rem;">${data.priority} Priority • Due ${data.due_date || 'N/A'} (${data.estimated_minutes}m)</span>`;
        }
        this.tooltip.innerHTML = content;
        this.tooltip.style.display = 'block';
        this.tooltip.style.left = `${e.clientX + 16}px`;
        this.tooltip.style.top = `${e.clientY + 16}px`;
      }
    } else {
      this.hoveredObject = null;
      document.body.style.cursor = 'default';
      if (this.tooltip) {
        this.tooltip.style.display = 'none';
      }
    }
  }

  onResize() {
    if (!this.container || !this.renderer || !this.camera) return;
    const width = this.container.clientWidth;
    const height = this.container.clientHeight;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  animate() {
    this.animationFrameId = requestAnimationFrame(() => this.animate());

    // Ambient orbit
    if (this.rotationGroup && !this.isDragging) {
      this.rotationGroup.rotation.y += 0.0025;
    }

    // Atmospheric breathing pulse
    if (this.glowAtmosphere) {
      const scale = 1.0 + Math.sin(Date.now() * 0.0015) * 0.02;
      this.glowAtmosphere.scale.set(scale, scale, scale);
    }

    // Floating motion for satellites and asteroids
    const time = Date.now() * 0.002;
    this.subjectMeshes.forEach((mesh, i) => {
      mesh.position.y += Math.sin(time + i) * 0.012;
      mesh.rotation.x += 0.012;
      mesh.rotation.y += 0.012;
    });

    this.taskMeshes.forEach((mesh, i) => {
      mesh.rotation.y += 0.018;
      mesh.rotation.z += 0.014;
    });

    if (this.stars) {
      this.stars.rotation.y += 0.0004;
    }

    this.renderer.render(this.scene, this.camera);
  }

  destroy() {
    if (this.animationFrameId) {
      cancelAnimationFrame(this.animationFrameId);
    }
    if (this.renderer) {
      this.renderer.dispose();
    }
  }
}

window.StudyUniverse = StudyUniverse;
