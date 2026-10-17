// AntiGravity Quantum Particle Mesh Background Engine
// Creates glowing, blinking, shape-shifting particles that drift, react to mouse, form constellations & geometric shapes

(function() {
  const canvas = document.createElement('canvas');
  canvas.id = 'antigravity-bg';
  canvas.style.position = 'fixed';
  canvas.style.top = '0';
  canvas.style.left = '0';
  canvas.style.width = '100vw';
  canvas.style.height = '100vh';
  canvas.style.pointerEvents = 'none';
  canvas.style.zIndex = '0';
  canvas.style.opacity = '0.9';
  document.body.prepend(canvas);

  const ctx = canvas.getContext('2d');
  let width = (canvas.width = window.innerWidth);
  let height = (canvas.height = window.innerHeight);

  // Resize handling
  window.addEventListener('resize', () => {
    width = canvas.width = window.innerWidth;
    height = canvas.height = window.innerHeight;
    initParticles();
  });

  // Track cursor for quantum gravity interaction
  const mouse = { x: -1000, y: -1000, radius: 160 };
  window.addEventListener('mousemove', (e) => {
    mouse.x = e.clientX;
    mouse.y = e.clientY;
  });
  window.addEventListener('mouseleave', () => {
    mouse.x = -1000;
    mouse.y = -1000;
  });

  // Particle configuration
  const PARTICLE_COUNT = Math.min(130, Math.floor((width * height) / 10000));
  let particles = [];

  // Morphing Shapes definitions (Anti-Gravity shape modes)
  // 0: Constellation Mesh (floating organic network)
  // 1: Hexagonal / Crystal Lattice
  // 2: Fibonacci Orbit / Vortex
  // 3: DNA Double Helix
  let currentShapeMode = 0;
  let shapeTransition = 0; // 0 to 1
  let shapeTargetMode = 0;
  let shapeTimer = 0;

  class Particle {
    constructor(index) {
      this.index = index;
      this.x = Math.random() * width;
      this.y = Math.random() * height;
      this.vx = (Math.random() - 0.5) * 0.7;
      this.vy = (Math.random() - 0.5) * 0.7;
      this.baseRadius = 1.2 + Math.random() * 2.2;
      this.radius = this.baseRadius;

      // Color palette: Cyan, Violet, Purple, Amber, Mint
      const colors = [
        'rgba(139, 92, 246, ',  // Purple
        'rgba(6, 182, 212, ',   // Cyan
        'rgba(168, 85, 247, ',  // Violet
        'rgba(236, 72, 153, ',  // Rose
        'rgba(16, 185, 129, '   // Mint
      ];
      this.baseColor = colors[Math.floor(Math.random() * colors.length)];

      // Blinking parameters
      this.alpha = 0.2 + Math.random() * 0.8;
      this.blinkSpeed = 0.015 + Math.random() * 0.035;
      this.blinkDirection = Math.random() > 0.5 ? 1 : -1;
      this.phase = Math.random() * Math.PI * 2;

      // Shape target positions
      this.targetX = this.x;
      this.targetY = this.y;
    }

    calculateShapeTarget(mode, time) {
      const centerX = width * 0.55; // Centered slightly towards content
      const centerY = height * 0.5;
      const count = particles.length;
      const i = this.index;

      if (mode === 0) {
        // Mode 0: Free anti-gravity float
        return null;
      } else if (mode === 1) {
        // Mode 1: Quantum Ring / Orbital Halo
        const angle = (i / count) * Math.PI * 2 + time * 0.0005;
        const r = Math.min(width, height) * 0.28 + Math.sin(i * 3 + time * 0.002) * 25;
        return {
          x: centerX + Math.cos(angle) * r,
          y: centerY + Math.sin(angle) * r
        };
      } else if (mode === 2) {
        // Mode 2: Fibonacci Galaxy Spiral
        const phi = 1.6180339887;
        const angle = i * phi * Math.PI * 2 + time * 0.0008;
        const r = Math.sqrt(i / count) * Math.min(width, height) * 0.38;
        return {
          x: centerX + Math.cos(angle) * r,
          y: centerY + Math.sin(angle) * r
        };
      } else if (mode === 3) {
        // Mode 3: Dynamic Wave / Geometric Lattice
        const cols = Math.ceil(Math.sqrt(count * (width / height)));
        const col = i % cols;
        const row = Math.floor(i / cols);
        const spacingX = width / (cols + 1);
        const spacingY = height / (Math.ceil(count / cols) + 1);
        const wave = Math.sin(col * 0.5 + time * 0.002) * 35;
        return {
          x: (col + 1) * spacingX,
          y: (row + 1) * spacingY + wave
        };
      }
      return null;
    }

    update(time) {
      // Dynamic Blinking effect
      this.phase += this.blinkSpeed;
      this.alpha = 0.25 + 0.65 * (0.5 + 0.5 * Math.sin(this.phase));

      // Calculate Target for Shape Morphing
      const target = this.calculateShapeTarget(currentShapeMode, time);

      if (target) {
        // Smoothly gravitate towards target geometric shape
        this.x += (target.x - this.x) * 0.025;
        this.y += (target.y - this.y) * 0.025;
      } else {
        // Natural anti-gravity drift
        this.x += this.vx;
        this.y += this.vy;

        // Wrap boundaries
        if (this.x < 0) this.x = width;
        if (this.x > width) this.x = 0;
        if (this.y < 0) this.y = height;
        if (this.y > height) this.y = 0;
      }

      // Mouse interactive anti-gravity repulsion
      const dx = this.x - mouse.x;
      const dy = this.y - mouse.y;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < mouse.radius && dist > 0) {
        const force = (mouse.radius - dist) / mouse.radius;
        const angle = Math.atan2(dy, dx);
        this.x += Math.cos(angle) * force * 5;
        this.y += Math.sin(angle) * force * 5;
      }
    }

    draw() {
      // Glowing particle circle with blur shadow
      ctx.beginPath();
      ctx.arc(this.x, this.y, this.radius, 0, Math.PI * 2);
      ctx.fillStyle = this.baseColor + this.alpha + ')';
      ctx.shadowBlur = 10;
      ctx.shadowColor = this.baseColor + '0.8)';
      ctx.fill();
      ctx.shadowBlur = 0;
    }
  }

  function initParticles() {
    particles = [];
    for (let i = 0; i < PARTICLE_COUNT; i++) {
      particles.push(new Particle(i));
    }
  }

  initParticles();

  // Morph shape periodically every 9 seconds
  setInterval(() => {
    currentShapeMode = (currentShapeMode + 1) % 4;
  }, 9000);

  // Animation Loop
  let lastTime = 0;
  function animate(timestamp) {
    requestAnimationFrame(animate);

    ctx.clearRect(0, 0, width, height);

    // Draw connecting constellation lines between nearby blinking dots
    const maxDistance = 110;
    for (let i = 0; i < particles.length; i++) {
      for (let j = i + 1; j < particles.length; j++) {
        const dx = particles[i].x - particles[j].x;
        const dy = particles[i].y - particles[j].y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist < maxDistance) {
          const lineAlpha = (1 - dist / maxDistance) * 0.18 * Math.min(particles[i].alpha, particles[j].alpha);
          ctx.beginPath();
          ctx.moveTo(particles[i].x, particles[i].y);
          ctx.lineTo(particles[j].x, particles[j].y);
          ctx.strokeStyle = `rgba(139, 92, 246, ${lineAlpha})`;
          ctx.lineWidth = 0.8;
          ctx.stroke();
        }
      }
    }

    // Update and draw all quantum particles
    for (let i = 0; i < particles.length; i++) {
      particles[i].update(timestamp);
      particles[i].draw();
    }
  }

  requestAnimationFrame(animate);
})();
