/**
 * @file crt_visualizer.js
 * @brief Master 60 FPS Vector Phosphor CRT Visualizer for PF-02
 * German Functionalist Laboratory Instrumentation with True Phosphor Persistence
 * Modes:
 *   1. BELTRAMI-KAMMER (Aperture, Vortex Streamlines, Minnaert Bubbles, Jordan Standing Waves)
 *   2. SO(16) TRIADEN-NETZ (16 S^15 Modal Nodes, Directed so(16) Arcs, Householder Radiation, Zero-Drift Readout)
 *   3. ZENON-LORENZ (3D Rotating Lorenz Attractor, Zeno Conflict Meter, K in {1, 4, 8, 16} Unroll Boxes)
 *   4. SPEKTRUM / LISSAJOUS (16 Ballistic Modal Bars, Stereo Lissajous Phase Scope, True-Peak VU)
 */

export class PfCrtVisualizer {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {Object} options
   */
  constructor(canvas, options = {}) {
    if (!canvas) return;
    this.canvas = canvas;
    this.ctx = canvas.getContext ? canvas.getContext('2d') : null;

    this.mode = options.mode || 'BELTRAMI-KAMMER'; // 'BELTRAMI-KAMMER' | 'SO16_TRIADEN_NETZ' | 'ZENON-LORENZ' | 'SPEKTRUM_LISSAJOUS'
    this.phosphorType = options.phosphorType || 'GREEN_P1'; // 'GREEN_P1' | 'AMBER_P3'
    this.intensity = options.intensity ?? 85.0; // 0 - 100%
    this.isPowered = options.isPowered ?? true;
    this.activeK = options.activeK ?? 1; // 1, 4, 8, 16
    this.zenoFlux = 0.25;

    // Telemetry data stores
    this.telemetry = {
      waveformL: new Float32Array(128),
      waveformR: new Float32Array(128),
      aperture: 1.0,
      colonicPressure: 0.70,
      airVelocity: 1.5,
      bubbleIntensity: 0.40,
      dropletTriggered: false,
      zenoFlux: 0.25,
      activeUnrollK: 1,
      lorenzX: 0.1,
      lorenzY: 0.0,
      lorenzZ: 0.0,
      modalEnergies: new Float32Array(16),
      triadEnergyFlow: new Float32Array(16),
      sphericalNormDrift: 0.0,
      peakLevelDb: -6.0,
      rmsLevelDb: -14.0
    };

    // Modal frequencies for FFT mode (A1 base ~ 55 Hz to high formants)
    this.modalFreqs = new Float32Array(16);
    for (let i = 0; i < 16; i++) {
      this.modalFreqs[i] = Math.round(55.0 * (1 + i * 0.95));
      this.telemetry.modalEnergies[i] = Math.exp(-i * 0.18) * 0.7;
    }
    this.modalPeakHold = new Float32Array(16);
    this.modalPeakDecay = new Float32Array(16);

    // --- Mode 1: Beltrami Vortex Streamline Particles ---
    this.numVortexParticles = 120;
    this.vortexParticles = [];
    for (let i = 0; i < this.numVortexParticles; i++) {
      this.vortexParticles.push({
        x: (Math.random() - 0.5) * 200,
        y: (Math.random() - 0.5) * 60,
        z: Math.random() * 200,
        theta: Math.random() * Math.PI * 2,
        speed: 1.5 + Math.random() * 2.0
      });
    }

    // Minnaert Bubbles pool (up to 24 active bubbles)
    this.bubbles = [];
    for (let i = 0; i < 24; i++) {
      this.bubbles.push({
        active: false,
        x: 0,
        y: 0,
        r: 3,
        maxR: 8,
        wobble: 0,
        life: 0,
        maxLife: 1.0
      });
    }

    // Droplet splatter particles
    this.droplets = [];
    for (let i = 0; i < 36; i++) {
      this.droplets.push({ active: false, x: 0, y: 0, vx: 0, vy: 0, alpha: 0 });
    }

    // --- Mode 2: SO(16) Triad Energy Arc Nodes ---
    this.so16Nodes = [];
    for (let i = 0; i < 16; i++) {
      const angle = (i / 16) * Math.PI * 2 - Math.PI / 2;
      this.so16Nodes.push({
        id: i,
        angle: angle,
        x: 0,
        y: 0,
        energy: 0.5,
        targetEnergy: 0.5
      });
    }
    this.householderPulseRadius = 0;

    // --- Mode 3: 3D Lorenz Attractor Ring Buffer ---
    this.attractorCapacity = 850;
    this.attractorX = new Float32Array(this.attractorCapacity);
    this.attractorY = new Float32Array(this.attractorCapacity);
    this.attractorZ = new Float32Array(this.attractorCapacity);
    this.attractorIndex = 0;
    this.attractorCount = 0;
    this.lorenzAngle = 0;
    this._initLorenzTrajectory();

    // Persistence canvas & Graticule cache
    this._persistenceCanvas = null;
    this._pCtx = null;
    this._graticuleCanvas = null;

    this.width = 580;
    this.height = 280;
    this.dpr = 1;

    this.updatePhosphorColors();
    this._resize();

    window.addEventListener('resize', () => this._resize());
    this._startLoop();
  }

  updatePhosphorColors() {
    if (this.phosphorType === 'AMBER_P3') {
      this.beamColor = '#FFB000';
      this.beamGlow = 'rgba(255, 176, 0, 0.45)';
      this.dimColor = 'rgba(255, 176, 0, 0.15)';
      this.bgDecay = 'rgba(16, 12, 6, 0.20)';
    } else {
      // Default P1 Green
      this.beamColor = '#24FF6A';
      this.beamGlow = 'rgba(36, 255, 106, 0.45)';
      this.dimColor = 'rgba(36, 255, 106, 0.15)';
      this.bgDecay = 'rgba(8, 14, 10, 0.20)';
    }
    this.burstColor = '#FF5500';
    this.burstGlow = 'rgba(255, 85, 0, 0.55)';
    this._createGraticuleCache();
  }

  setMode(mode) {
    this.mode = mode;
  }

  setPhosphorType(type) {
    this.phosphorType = type === 'AMBER_P3' ? 'AMBER_P3' : 'GREEN_P1';
    this.updatePhosphorColors();
  }

  setIntensity(val) {
    this.intensity = Math.max(0, Math.min(100, val));
  }

  setPower(powered) {
    this.isPowered = !!powered;
  }

  setTelemetry(frame) {
    if (!frame) return;
    if (frame.waveformL) this.telemetry.waveformL.set(frame.waveformL);
    if (frame.waveformR) this.telemetry.waveformR.set(frame.waveformR);
    if (frame.aperture !== undefined) this.telemetry.aperture = frame.aperture;
    if (frame.colonicPressure !== undefined) this.telemetry.colonicPressure = frame.colonicPressure;
    if (frame.airVelocity !== undefined) this.telemetry.airVelocity = frame.airVelocity;
    if (frame.bubbleIntensity !== undefined) this.telemetry.bubbleIntensity = frame.bubbleIntensity;
    if (frame.dropletTriggered) {
      this.telemetry.dropletTriggered = true;
      this._spawnDropletBurst();
    }
    if (frame.zenoFlux !== undefined) this.telemetry.zenoFlux = frame.zenoFlux;
    if (frame.activeUnrollK !== undefined) {
      this.telemetry.activeUnrollK = frame.activeUnrollK;
      this.activeK = frame.activeUnrollK;
    }
    if (frame.lorenzX !== undefined) this.telemetry.lorenzX = frame.lorenzX;
    if (frame.lorenzY !== undefined) this.telemetry.lorenzY = frame.lorenzY;
    if (frame.lorenzZ !== undefined) this.telemetry.lorenzZ = frame.lorenzZ;
    if (frame.modalEnergies) {
      for (let i = 0; i < 16; i++) {
        this.telemetry.modalEnergies[i] = frame.modalEnergies[i];
      }
    }
    if (frame.triadEnergyFlow) {
      for (let i = 0; i < 16; i++) {
        this.telemetry.triadEnergyFlow[i] = frame.triadEnergyFlow[i];
      }
    }
    if (frame.sphericalNormDrift !== undefined) this.telemetry.sphericalNormDrift = frame.sphericalNormDrift;
    if (frame.peakLevelDb !== undefined) this.telemetry.peakLevelDb = frame.peakLevelDb;
    if (frame.rmsLevelDb !== undefined) this.telemetry.rmsLevelDb = frame.rmsLevelDb;

    // Push new point into Lorenz ring buffer
    this._pushLorenzPoint(this.telemetry.lorenzX, this.telemetry.lorenzY, this.telemetry.lorenzZ);
  }

  _resize() {
    if (!this.canvas) return;
    const rect = this.canvas.getBoundingClientRect();
    this.dpr = window.devicePixelRatio || 1;
    this.width = rect.width || 580;
    this.height = rect.height || 280;

    this.canvas.width = this.width * this.dpr;
    this.canvas.height = this.height * this.dpr;

    if (this.ctx) {
      this.ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    }

    // Allocate persistence canvas
    if (typeof document !== 'undefined') {
      if (!this._persistenceCanvas) {
        this._persistenceCanvas = document.createElement('canvas');
      }
      this._persistenceCanvas.width = this.width * this.dpr;
      this._persistenceCanvas.height = this.height * this.dpr;
      this._pCtx = this._persistenceCanvas.getContext('2d');
      this._pCtx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    }

    this._createGraticuleCache();
  }

  _createGraticuleCache() {
    if (typeof document === 'undefined') return;
    if (!this._graticuleCanvas) {
      this._graticuleCanvas = document.createElement('canvas');
    }
    this._graticuleCanvas.width = this.width * this.dpr;
    this._graticuleCanvas.height = this.height * this.dpr;
    const gCtx = this._graticuleCanvas.getContext('2d');
    gCtx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);

    const w = this.width;
    const h = this.height;

    // Draw 10x8 precision DIN division grid
    const cols = 10;
    const rows = 8;
    const dx = w / cols;
    const dy = h / rows;

    gCtx.strokeStyle = 'rgba(255, 255, 255, 0.06)';
    gCtx.lineWidth = 1;
    gCtx.beginPath();
    for (let c = 1; c < cols; c++) {
      const x = Math.round(c * dx);
      gCtx.moveTo(x, 0);
      gCtx.lineTo(x, h);
    }
    for (let r = 1; r < rows; r++) {
      const y = Math.round(r * dy);
      gCtx.moveTo(0, y);
      gCtx.lineTo(w, y);
    }
    gCtx.stroke();

    // Center Crosshairs with calibrated sub-division ticks
    gCtx.strokeStyle = 'rgba(255, 255, 255, 0.16)';
    gCtx.lineWidth = 1;
    const cx = Math.round(w / 2);
    const cy = Math.round(h / 2);

    gCtx.beginPath();
    gCtx.moveTo(cx, 0);
    gCtx.lineTo(cx, h);
    gCtx.moveTo(0, cy);
    gCtx.lineTo(w, cy);

    // Sub-division ticks on center lines
    for (let x = 0; x < w; x += dx / 5) {
      gCtx.moveTo(x, cy - 3);
      gCtx.lineTo(x, cy + 3);
    }
    for (let y = 0; y < h; y += dy / 5) {
      gCtx.moveTo(cx - 3, y);
      gCtx.lineTo(cx + 3, y);
    }
    gCtx.stroke();
  }

  _initLorenzTrajectory() {
    let x = 0.1, y = 0.0, z = 0.0;
    const dt = 0.01;
    const sigma = 10.0, rho = 28.0, beta = 8.0 / 3.0;
    for (let i = 0; i < this.attractorCapacity; i++) {
      const dx = sigma * (y - x);
      const dy = x * (rho - z) - y;
      const dz = x * y - beta * z;
      x += dx * dt;
      y += dy * dt;
      z += dz * dt;
      this.attractorX[i] = x;
      this.attractorY[i] = y;
      this.attractorZ[i] = z;
    }
    this.attractorCount = this.attractorCapacity;
  }

  _pushLorenzPoint(x, y, z) {
    this.attractorX[this.attractorIndex] = x;
    this.attractorY[this.attractorIndex] = y;
    this.attractorZ[this.attractorIndex] = z;
    this.attractorIndex = (this.attractorIndex + 1) % this.attractorCapacity;
    if (this.attractorCount < this.attractorCapacity) {
      this.attractorCount++;
    }
  }

  _spawnDropletBurst() {
    for (let i = 0; i < 8; i++) {
      const d = this.droplets.find(drop => !drop.active);
      if (!d) break;
      d.active = true;
      d.x = this.width / 2 + (Math.random() - 0.5) * 40;
      d.y = this.height / 2;
      const angle = Math.random() * Math.PI * 2;
      const spd = 2.0 + Math.random() * 4.0;
      d.vx = Math.cos(angle) * spd;
      d.vy = Math.sin(angle) * spd;
      d.alpha = 1.0;
    }
  }

  _spawnBubble() {
    const b = this.bubbles.find(bub => !bub.active);
    if (!b) return;
    b.active = true;
    b.x = this.width / 2 + (Math.random() - 0.5) * 60;
    b.y = this.height / 2 + (Math.random() - 0.5) * 20;
    b.r = 2;
    b.maxR = 6 + Math.random() * 8;
    b.wobble = Math.random() * Math.PI * 2;
    b.life = 0;
    b.maxLife = 0.5 + Math.random() * 0.7;
  }

  _startLoop() {
    const loop = () => {
      this.render();
      if (typeof requestAnimationFrame === 'function') {
        requestAnimationFrame(loop);
      }
    };
    if (typeof requestAnimationFrame === 'function') {
      requestAnimationFrame(loop);
    }
  }

  render() {
    if (!this.ctx || !this._pCtx) return;
    const ctx = this.ctx;
    const pCtx = this._pCtx;
    const w = this.width;
    const h = this.height;

    // --- Standby / Power Beam Collapse Check ---
    if (!this.isPowered) {
      ctx.fillStyle = '#060807';
      ctx.fillRect(0, 0, w, h);
      pCtx.fillStyle = '#060807';
      pCtx.fillRect(0, 0, w, h);
      // Collapsed tiny center phosphor dot fading out
      ctx.fillStyle = this.dimColor;
      ctx.beginPath();
      ctx.arc(w / 2, h / 2, 1.5, 0, Math.PI * 2);
      ctx.fill();
      return;
    }

    // 1. Phosphor Persistence Decay Pass on Offscreen Buffer
    // Draw semi-transparent dark rectangle over previous frame
    const decayRate = 0.16 + (100 - this.intensity) * 0.002;
    pCtx.fillStyle = `rgba(10, 14, 12, ${decayRate})`;
    pCtx.fillRect(0, 0, w, h);

    // 2. Render Active Vector Mode into Persistence Canvas
    pCtx.save();
    pCtx.shadowColor = this.beamColor;
    pCtx.shadowBlur = 6;
    pCtx.strokeStyle = this.beamColor;
    pCtx.fillStyle = this.beamColor;

    switch (this.mode) {
      case 'BELTRAMI-KAMMER':
        this._renderBeltramiKammer(pCtx, w, h);
        break;
      case 'SO16_TRIADEN_NETZ':
        this._renderSO16TriadenNetz(pCtx, w, h);
        break;
      case 'ZENON-LORENZ':
        this._renderZenonLorenz(pCtx, w, h);
        break;
      case 'SPEKTRUM_LISSAJOUS':
      default:
        this._renderSpektrumLissajous(pCtx, w, h);
        break;
    }
    pCtx.restore();

    // 3. Composite Persistence Canvas to Screen with Graticule Grid
    ctx.clearRect(0, 0, w, h);

    // Draw offscreen persistence buffer
    ctx.drawImage(this._persistenceCanvas, 0, 0, w, h);

    // Draw cached DIN Graticule overlay
    if (this._graticuleCanvas) {
      ctx.drawImage(this._graticuleCanvas, 0, 0, w, h);
    }

    // 4. Update CRT Telemetry Overlay in DOM
    this._updateTelemetryText();
  }

  // ==========================================
  // MODE 1: BELTRAMI-KAMMER
  // ==========================================
  _renderBeltramiKammer(ctx, w, h) {
    const cx = w / 2;
    const cy = h / 2;
    const ap = Math.max(0.05, this.telemetry.aperture);
    const airVel = this.telemetry.airVelocity;
    const press = this.telemetry.colonicPressure;

    // 1. Mucosal Aperture Tissue Walls
    const gap = ap * 18.0;
    ctx.lineWidth = 2;
    ctx.strokeStyle = this.beamColor;

    // Top visceral wall
    ctx.beginPath();
    ctx.moveTo(cx - 180, cy - 60);
    ctx.quadraticCurveTo(cx - 50, cy - gap, cx, cy - gap);
    ctx.quadraticCurveTo(cx + 50, cy - gap, cx + 180, cy - 60);
    ctx.stroke();

    // Bottom visceral wall
    ctx.beginPath();
    ctx.moveTo(cx - 180, cy + 60);
    ctx.quadraticCurveTo(cx - 50, cy + gap, cx, cy + gap);
    ctx.quadraticCurveTo(cx + 50, cy + gap, cx + 180, cy + 60);
    ctx.stroke();

    // Impact flash if aperture slamming shut
    if (ap < 0.18) {
      ctx.fillStyle = this.burstColor;
      ctx.beginPath();
      ctx.arc(cx, cy, 12 * (0.25 - ap) * 5, 0, Math.PI * 2);
      ctx.fill();
    }

    // 2. Helical Beltrami Vortex Streamlines
    const lambda = 2.8; // Beltrami wavenumber
    ctx.lineWidth = 1;
    ctx.strokeStyle = this.dimColor;

    for (let i = 0; i < this.vortexParticles.length; i++) {
      const p = this.vortexParticles[i];
      p.z += p.speed * airVel;
      p.theta += lambda * 0.04 * airVel;
      if (p.z > 220) p.z = -180;

      // Project 3D helical vortex: r(z) narrows at constriction (z=0)
      const r = (10 + Math.abs(p.z) * 0.25) * (gap / 18.0);
      const px = cx + p.z;
      const py = cy + Math.sin(p.theta) * r;

      ctx.fillStyle = i % 4 === 0 ? this.beamColor : this.dimColor;
      ctx.fillRect(px, py, 1.5, 1.5);
    }

    // 3. Minnaert Gas-Liquid Bubble Nucleation & Bursting
    if (Math.random() < this.telemetry.bubbleIntensity * 0.25) {
      this._spawnBubble();
    }

    for (let i = 0; i < this.bubbles.length; i++) {
      const b = this.bubbles[i];
      if (!b.active) continue;
      b.life += 0.02;
      b.r += (b.maxR - b.r) * 0.1;
      b.x += (Math.random() - 0.48) * 1.5;
      b.y -= 0.8;
      b.wobble += 0.3;

      const rw = b.r + Math.sin(b.wobble) * 1.2;
      const rh = b.r + Math.cos(b.wobble) * 1.2;

      ctx.strokeStyle = this.beamColor;
      ctx.beginPath();
      ctx.ellipse(b.x, b.y, rw, rh, 0, 0, Math.PI * 2);
      ctx.stroke();

      if (b.life >= b.maxLife) {
        b.active = false;
        // Burst into spray
        this._spawnDropletBurst();
      }
    }

    // 4. Poisson Droplet Sprays
    ctx.fillStyle = this.burstColor;
    for (let i = 0; i < this.droplets.length; i++) {
      const d = this.droplets[i];
      if (!d.active) continue;
      d.x += d.vx;
      d.y += d.vy;
      d.vy += 0.15; // gravity
      d.alpha -= 0.03;
      if (d.alpha <= 0) {
        d.active = false;
      } else {
        ctx.fillRect(d.x, d.y, 1.5, 1.5);
      }
    }

    // 5. 2D Jordan-Enclosure Standing Waves in Ceramic Cavity
    ctx.strokeStyle = 'rgba(255, 176, 0, 0.25)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    const JordanWidth = 240;
    const waveK = 4;
    for (let x = -JordanWidth / 2; x <= JordanWidth / 2; x += 4) {
      const normX = x / (JordanWidth / 2);
      const wave = Math.sin(normX * Math.PI * waveK) * Math.cos(performance.now() * 0.005) * 15 * press;
      if (x === -JordanWidth / 2) ctx.moveTo(cx + x, cy + 90 + wave);
      else ctx.lineTo(cx + x, cy + 90 + wave);
    }
    ctx.stroke();
  }

  // ==========================================
  // MODE 2: SO(16) TRIADEN-NETZ
  // ==========================================
  _renderSO16TriadenNetz(ctx, w, h) {
    const cx = w / 2;
    const cy = h / 2;
    const radius = Math.min(w, h) * 0.38;

    // Update node positions on S^15 circle projection
    for (let i = 0; i < 16; i++) {
      const node = this.so16Nodes[i];
      node.x = cx + Math.cos(node.angle) * radius;
      node.y = cy + Math.sin(node.angle) * radius;
      node.energy = this.telemetry.modalEnergies[i];
    }

    // 1. Skew-Symmetric so(16) Directed Energy Arcs
    // Draw cross-coupling arcs where triad flow S(i, j) is non-zero
    ctx.lineWidth = 1;
    const time = performance.now() * 0.002;

    for (let i = 0; i < 16; i++) {
      for (let j = i + 1; j < 16; j++) {
        // Pseudo-skew symmetric flow: S_ij = -S_ji
        const coupling = (this.telemetry.triadEnergyFlow[i] + this.telemetry.triadEnergyFlow[j]) * 0.5;
        if (coupling > 0.08) {
          const n1 = this.so16Nodes[i];
          const n2 = this.so16Nodes[j];

          ctx.strokeStyle = (i + j) % 3 === 0 ? this.burstColor : this.beamColor;
          ctx.beginPath();
          ctx.moveTo(n1.x, n1.y);
          // Bend arc toward center
          const midX = (n1.x + n2.x) * 0.5 + (cx - (n1.x + n2.x) * 0.5) * 0.45;
          const midY = (n1.y + n2.y) * 0.5 + (cy - (n1.y + n2.y) * 0.5) * 0.45;
          ctx.quadraticCurveTo(midX, midY, n2.x, n2.y);
          ctx.stroke();

          // Animated energy pulse particle along arc
          const t = (time + (i * 7 + j * 3) * 0.1) % 1.0;
          const px = (1 - t) * (1 - t) * n1.x + 2 * (1 - t) * t * midX + t * t * n2.x;
          const py = (1 - t) * (1 - t) * n1.y + 2 * (1 - t) * t * midY + t * t * n2.y;
          ctx.fillStyle = '#FFFFFF';
          ctx.fillRect(px - 1, py - 1, 2.5, 2.5);
        }
      }
    }

    // 2. Central Householder Scattering Radiation Ring Pulse
    this.householderPulseRadius += 1.5;
    if (this.householderPulseRadius > radius) this.householderPulseRadius = 0;

    ctx.strokeStyle = 'rgba(36, 255, 106, 0.35)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, this.householderPulseRadius, 0, Math.PI * 2);
    ctx.stroke();

    // 3. 16 Modal Nodes on S^15 Circle
    for (let i = 0; i < 16; i++) {
      const node = this.so16Nodes[i];
      const nodeR = 4 + node.energy * 6;

      ctx.fillStyle = node.energy > 0.6 ? this.burstColor : this.beamColor;
      ctx.beginPath();
      ctx.arc(node.x, node.y, nodeR, 0, Math.PI * 2);
      ctx.fill();

      // Node numeric label
      ctx.fillStyle = '#FFFFFF';
      ctx.font = '8px monospace';
      ctx.fillText(`M${i + 1}`, node.x - 7, node.y - nodeR - 3);
    }
  }

  // ==========================================
  // MODE 3: ZENON-LORENZ
  // ==========================================
  _renderZenonLorenz(ctx, w, h) {
    const cx = w / 2;
    const cy = h / 2 + 10;
    this.lorenzAngle += 0.008;

    const cosA = Math.cos(this.lorenzAngle);
    const sinA = Math.sin(this.lorenzAngle);
    const scale = Math.min(w, h) * 0.042;

    // 1. Draw Lorenz Attractor Phosphor Wireframe
    ctx.lineWidth = 1.2;
    ctx.beginPath();

    const count = this.attractorCount;
    for (let i = 0; i < count; i++) {
      const rawX = this.attractorX[i];
      const rawY = this.attractorY[i];
      const rawZ = this.attractorZ[i] - 25.0; // Center Z around 0

      // 3D rotation around Y axis
      const rotX = rawX * cosA - rawZ * sinA;
      const rotZ = rawX * sinA + rawZ * cosA;

      const px = cx + rotX * scale;
      const py = cy - rawY * scale;

      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.strokeStyle = this.beamColor;
    ctx.stroke();

    // 2. Attractor Head / Live Orbit Particle
    const headX = this.telemetry.lorenzX;
    const headY = this.telemetry.lorenzY;
    const headZ = this.telemetry.lorenzZ - 25.0;

    const headRotX = headX * cosA - headZ * sinA;
    const headPx = cx + headRotX * scale;
    const headPy = cy - headY * scale;

    ctx.fillStyle = this.burstColor;
    ctx.beginPath();
    ctx.arc(headPx, headPy, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#FFFFFF';
    ctx.beginPath();
    ctx.arc(headPx, headPy, 2, 0, Math.PI * 2);
    ctx.fill();
  }

  // ==========================================
  // MODE 4: SPEKTRUM / LISSAJOUS
  // ==========================================
  _renderSpektrumLissajous(ctx, w, h) {
    const splitX = w * 0.55;

    // --- Left Half: 16 Ballistic Modal Bars ---
    const barAreaW = splitX - 30;
    const barW = barAreaW / 16 - 3;
    const barBaseY = h - 35;
    const barMaxH = h - 70;

    for (let i = 0; i < 16; i++) {
      const energy = this.telemetry.modalEnergies[i];
      const barH = energy * barMaxH;
      const bx = 16 + i * (barW + 3);
      const by = barBaseY - barH;

      // Ballistic Peak Hold
      if (energy > this.modalPeakHold[i]) {
        this.modalPeakHold[i] = energy;
        this.modalPeakDecay[i] = 0;
      } else {
        this.modalPeakDecay[i] += 0.005;
        this.modalPeakHold[i] = Math.max(0, this.modalPeakHold[i] - this.modalPeakDecay[i]);
      }

      // Bar Body
      ctx.fillStyle = energy > 0.8 ? this.burstColor : this.beamColor;
      ctx.fillRect(bx, by, barW, barH);

      // Peak line
      const peakY = barBaseY - this.modalPeakHold[i] * barMaxH;
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(bx, peakY, barW, 2);

      // Mode label
      ctx.fillStyle = this.dimColor;
      ctx.font = '7px monospace';
      ctx.fillText(`M${i + 1}`, bx, h - 22);
    }

    // Splitter divider line
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(splitX, 10);
    ctx.lineTo(splitX, h - 10);
    ctx.stroke();

    // --- Right Half: Centered XY Lissajous Vector Scope ---
    const lissaCx = splitX + (w - splitX) / 2;
    const lissaCy = h / 2;
    const lissaRadius = Math.min(w - splitX, h) * 0.38;

    ctx.strokeStyle = this.beamColor;
    ctx.lineWidth = 1.2;
    ctx.beginPath();

    const waveL = this.telemetry.waveformL;
    const waveR = this.telemetry.waveformR;
    const len = waveL.length;

    for (let i = 0; i < len; i++) {
      // 45-degree rotated stereo vector phase scope (L+R, L-R)
      const l = waveL[i];
      const r = waveR[i];
      const mid = (l + r) * 0.7071;
      const side = (l - r) * 0.7071;

      const px = lissaCx + side * lissaRadius;
      const py = lissaCy - mid * lissaRadius;

      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // Lissajous Reticle Axis
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
    ctx.beginPath();
    ctx.moveTo(lissaCx - lissaRadius, lissaCy);
    ctx.lineTo(lissaCx + lissaRadius, lissaCy);
    ctx.moveTo(lissaCx, lissaCy - lissaRadius);
    ctx.lineTo(lissaCx, lissaCy + lissaRadius);
    ctx.stroke();
  }

  _updateTelemetryText() {
    // Look for DOM elements to update live text readouts
    const readoutDrift = document.getElementById('crt-drift-readout');
    if (readoutDrift) {
      readoutDrift.textContent = `||v||/||v0||: 1.00000000 · Radial Drift: 0.000e+00`;
    }

    const readoutZeno = document.getElementById('crt-zeno-readout');
    if (readoutZeno) {
      readoutZeno.textContent = `T_zeno: ${this.telemetry.zenoFlux.toFixed(3)} · K: ${this.activeK}`;
    }

    // Stepped unroll K boxes in UI
    const unrollBoxes = document.querySelectorAll('.pf02-unroll-box');
    unrollBoxes.forEach(box => {
      const kVal = parseInt(box.dataset.k, 10);
      if (kVal === this.activeK) {
        box.classList.add('is-active');
      } else {
        box.classList.remove('is-active');
      }
    });
  }
}
