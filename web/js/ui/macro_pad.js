/**
 * @file macro_pad.js
 * @brief 2D XY Performance Macro Pad for PF-02
 * Bauchpresse (Gut Squeeze) × Schleimhautfeuchte (Fluid Moisture)
 */

export class PfMacroPad {
  /**
   * @param {HTMLElement} container
   * @param {Object} options
   */
  constructor(container, options = {}) {
    this.container = container;
    this.onChange = options.onChange || null; // callback(x, y)
    this.x = options.x ?? 0.50; // Bauchpresse / Gut Squeeze
    this.y = options.y ?? 0.50; // Schleimhautfeuchte / Fluid Moisture
    this.isDragging = false;
    this.ghostTrail = [];

    this._render();
    this._attachEvents();
    this.setValues(this.x, this.y, false);
  }

  _render() {
    this.container.innerHTML = `
      <div class="pf02-macropad-header">
        <span class="pf02-deck-title" style="font-size: 10px;">XY-MAKRO-FELD</span>
        <span class="pf02-deck-subtitle">BAUCHPRESSE &times; SCHLEIMHAUTFEUCHTE</span>
      </div>
      <div class="pf02-macropad-canvas-wrap">
        <canvas class="pf02-macropad-canvas" width="280" height="120"></canvas>
      </div>
      <div class="pf02-macropad-readout">
        <span>X (PRESSE): <strong class="pf02-pad-val-x">0.50</strong></span>
        <span>Y (FEUCHTE): <strong class="pf02-pad-val-y">0.50</strong></span>
      </div>
    `;

    this.canvasWrap = this.container.querySelector('.pf02-macropad-canvas-wrap');
    this.canvas = this.container.querySelector('.pf02-macropad-canvas');
    this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
    this.valXEl = this.container.querySelector('.pf02-pad-val-x');
    this.valYEl = this.container.querySelector('.pf02-pad-val-y');

    this._resize();
  }

  _resize() {
    if (!this.canvasWrap || !this.canvas) return;
    const rect = this.canvasWrap.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    this.width = rect.width || 280;
    this.height = rect.height || 120;
    this.canvas.width = this.width * dpr;
    this.canvas.height = this.height * dpr;
    if (this.ctx) {
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
    this._draw();
  }

  _attachEvents() {
    const handleMove = (clientX, clientY) => {
      const rect = this.canvasWrap.getBoundingClientRect();
      let normX = (clientX - rect.left) / rect.width;
      let normY = 1.0 - ((clientY - rect.top) / rect.height); // Bottom-to-top Y
      normX = Math.max(0.0, Math.min(1.0, normX));
      normY = Math.max(0.0, Math.min(1.0, normY));
      this.setValues(normX, normY, true);
    };

    const onMouseDown = (e) => {
      if (e.button !== 0) return;
      this.isDragging = true;
      handleMove(e.clientX, e.clientY);
      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!this.isDragging) return;
      handleMove(e.clientX, e.clientY);
    };

    const onMouseUp = () => {
      this.isDragging = false;
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };

    const onTouchStart = (e) => {
      if (e.touches.length !== 1) return;
      this.isDragging = true;
      handleMove(e.touches[0].clientX, e.touches[0].clientY);
      window.addEventListener('touchmove', onTouchMove, { passive: false });
      window.addEventListener('touchend', onTouchEnd);
      e.preventDefault();
    };

    const onTouchMove = (e) => {
      if (!this.isDragging || e.touches.length !== 1) return;
      handleMove(e.touches[0].clientX, e.touches[0].clientY);
      e.preventDefault();
    };

    const onTouchEnd = () => {
      this.isDragging = false;
      window.removeEventListener('touchmove', onTouchMove);
      window.removeEventListener('touchend', onTouchEnd);
    };

    this.canvasWrap.addEventListener('mousedown', onMouseDown);
    this.canvasWrap.addEventListener('touchstart', onTouchStart, { passive: false });
    window.addEventListener('resize', () => this._resize());
  }

  setValues(x, y, notify = true) {
    this.x = Math.max(0.0, Math.min(1.0, x));
    this.y = Math.max(0.0, Math.min(1.0, y));

    if (this.valXEl) this.valXEl.textContent = this.x.toFixed(2);
    if (this.valYEl) this.valYEl.textContent = this.y.toFixed(2);

    this.ghostTrail.push({ x: this.x, y: this.y, age: 1.0 });
    if (this.ghostTrail.length > 12) this.ghostTrail.shift();

    this._draw();

    if (notify && typeof this.onChange === 'function') {
      this.onChange(this.x, this.y);
    }
  }

  _draw() {
    if (!this.ctx) return;
    const w = this.width;
    const h = this.height;
    const ctx = this.ctx;

    // Background fill
    ctx.fillStyle = '#0E1112';
    ctx.fillRect(0, 0, w, h);

    // DIN Laboratory Graticule Grid
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.07)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = w / 4; x < w; x += w / 4) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
    }
    for (let y = h / 4; y < h; y += h / 4) {
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
    }
    ctx.stroke();

    // Center Crosshairs
    ctx.strokeStyle = 'rgba(238, 89, 43, 0.25)';
    ctx.beginPath();
    ctx.moveTo(w / 2, 0);
    ctx.lineTo(w / 2, h);
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.stroke();

    const px = this.x * w;
    const py = (1.0 - this.y) * h;

    // Ghost trail
    for (let i = 0; i < this.ghostTrail.length; i++) {
      const g = this.ghostTrail[i];
      const gx = g.x * w;
      const gy = (1.0 - g.y) * h;
      const alpha = (i / this.ghostTrail.length) * 0.4;
      ctx.fillStyle = `rgba(238, 89, 43, ${alpha})`;
      ctx.beginPath();
      ctx.arc(gx, gy, 4, 0, Math.PI * 2);
      ctx.fill();
    }

    // Active crosshair lines
    ctx.strokeStyle = 'rgba(238, 89, 43, 0.7)';
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 3]);
    ctx.beginPath();
    ctx.moveTo(px, 0);
    ctx.lineTo(px, h);
    ctx.moveTo(0, py);
    ctx.lineTo(w, py);
    ctx.stroke();
    ctx.setLineDash([]);

    // Glowing Reticle Puck
    ctx.shadowColor = '#EE592B';
    ctx.shadowBlur = 10;
    ctx.fillStyle = '#EE592B';
    ctx.beginPath();
    ctx.arc(px, py, 6, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#FFFFFF';
    ctx.beginPath();
    ctx.arc(px, py, 2.5, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowBlur = 0;
  }
}
