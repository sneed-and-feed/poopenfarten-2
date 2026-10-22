/**
 * @file knobs.js
 * @brief Precision Tactile Knurled Rotary Knob Controller for PF-02
 * German Functionalist Laboratory Instrumentation
 */

export class PfKnob {
  /**
   * @param {HTMLElement} container
   * @param {Object} options
   */
  constructor(container, options = {}) {
    this.container = container;
    this.paramId = options.paramId || options.id || `param_${Math.random().toString(36).substr(2, 8)}`;
    this.label = options.label || 'KNOB';
    this.min = options.min ?? 0.0;
    this.max = options.max ?? 1.0;
    this.step = options.step ?? 0.01;
    this.unit = options.unit || '';
    this.isLog = options.isLog ?? false;
    this.defaultValue = options.value ?? (this.min + (this.max - this.min) / 2);
    this.value = this.defaultValue;
    this.precision = options.precision ?? (this.step < 1 ? 2 : 0);
    this.size = options.size || 'medium'; // 'small', 'medium', 'large'
    this.title = options.title || `${this.label} [${this.min}${this.unit} - ${this.max}${this.unit}]`;
    this.onChange = options.onChange || null;

    this.isDragging = false;
    this.startY = 0;
    this.startValue = 0;

    this.startAngle = -140; // degrees
    this.endAngle = 140;    // degrees
    this.angleRange = this.endAngle - this.startAngle; // 280 deg

    if (this.container && typeof document !== 'undefined') {
      this.container.innerHTML = '';
      this._render();
      this._attachEvents();
    }
    this.setValue(this.value, false);
  }

  _render() {
    this.element = document.createElement('div');
    this.element.className = `pf02-knob-wrapper ${this.size}`;
    this.element.tabIndex = 0;
    this.element.setAttribute('role', 'slider');
    this.element.setAttribute('aria-label', this.label);
    this.element.setAttribute('aria-valuemin', this.min);
    this.element.setAttribute('aria-valuemax', this.max);
    this.element.setAttribute('aria-valuenow', this.value);
    this.element.title = this.title;

    this.element.innerHTML = `
      <div class="pf02-knob-label">${this.label}</div>
      <div class="pf02-knob-assembly">
        <svg class="pf02-knob-scale" viewBox="0 0 100 100">
          <circle class="pf02-knob-track" cx="50" cy="50" r="38" />
          <circle class="pf02-knob-fill" cx="50" cy="50" r="38" />
        </svg>
        <div class="pf02-knob-cap">
          <div class="pf02-knob-indicator"></div>
        </div>
      </div>
      <div class="pf02-knob-value-display">
        <span class="pf02-knob-num">${this.formatValue(this.value)}</span><span class="pf02-knob-u">${this.unit}</span>
      </div>
    `;

    this.container.appendChild(this.element);

    this.cap = this.element.querySelector('.pf02-knob-cap');
    this.fillCircle = this.element.querySelector('.pf02-knob-fill');
    this.numDisplay = this.element.querySelector('.pf02-knob-num');
  }

  _attachEvents() {
    const onMouseDown = (e) => {
      if (e.button !== 0) return;
      this.isDragging = true;
      this.startY = e.clientY;
      this.startValue = this.value;
      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!this.isDragging) return;
      const dy = this.startY - e.clientY;
      const speed = e.shiftKey ? 0.001 : 0.005;
      const range = this.max - this.min;
      let nextVal = this.startValue + dy * range * speed;
      this.setValue(nextVal, true);
    };

    const onMouseUp = () => {
      this.isDragging = false;
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
    };

    // Touch support
    const onTouchStart = (e) => {
      if (e.touches.length !== 1) return;
      this.isDragging = true;
      this.startY = e.touches[0].clientY;
      this.startValue = this.value;
      document.addEventListener('touchmove', onTouchMove, { passive: false });
      document.addEventListener('touchend', onTouchEnd);
      e.preventDefault();
    };

    const onTouchMove = (e) => {
      if (!this.isDragging || e.touches.length !== 1) return;
      const dy = this.startY - e.touches[0].clientY;
      const range = this.max - this.min;
      let nextVal = this.startValue + dy * range * 0.005;
      this.setValue(nextVal, true);
      e.preventDefault();
    };

    const onTouchEnd = () => {
      this.isDragging = false;
      document.removeEventListener('touchmove', onTouchMove);
      document.removeEventListener('touchend', onTouchEnd);
    };

    // Wheel support
    const onWheel = (e) => {
      e.preventDefault();
      const delta = -Math.sign(e.deltaY);
      const step = (e.shiftKey ? this.step * 0.1 : this.step) || (this.max - this.min) * 0.01;
      this.setValue(this.value + delta * step, true);
    };

    // Double-click to reset
    this.element.addEventListener('dblclick', () => {
      this.setValue(this.defaultValue, true);
    });

    // Keyboard support
    this.element.addEventListener('keydown', (e) => {
      const step = (e.shiftKey ? this.step * 0.1 : this.step) || (this.max - this.min) * 0.01;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
        this.setValue(this.value + step, true);
        e.preventDefault();
      } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
        this.setValue(this.value - step, true);
        e.preventDefault();
      } else if (e.key === 'Home') {
        this.setValue(this.min, true);
        e.preventDefault();
      } else if (e.key === 'End') {
        this.setValue(this.max, true);
        e.preventDefault();
      }
    });

    this.element.addEventListener('mousedown', onMouseDown);
    this.element.addEventListener('touchstart', onTouchStart, { passive: false });
    this.element.addEventListener('wheel', onWheel, { passive: false });
  }

  formatValue(v) {
    if (this.precision === 0) return Math.round(v).toString();
    return v.toFixed(this.precision);
  }

  valueToNorm(val) {
    const clamped = Math.max(this.min, Math.min(this.max, val));
    if (this.isLog && this.min > 0) {
      return (Math.log(clamped) - Math.log(this.min)) / (Math.log(this.max) - Math.log(this.min));
    }
    return (clamped - this.min) / (this.max - this.min);
  }

  normToValue(norm) {
    const clamped = Math.max(0, Math.min(1, norm));
    if (this.isLog && this.min > 0) {
      return Math.exp(Math.log(this.min) + clamped * (Math.log(this.max) - Math.log(this.min)));
    }
    return this.min + clamped * (this.max - this.min);
  }

  setValue(val, notify = true) {
    let clamped = Math.max(this.min, Math.min(this.max, val));
    if (this.step > 0) {
      const steps = Math.round((clamped - this.min) / this.step);
      clamped = this.min + steps * this.step;
      clamped = Math.max(this.min, Math.min(this.max, clamped));
    }
    this.value = clamped;

    const norm = this.valueToNorm(this.value);
    const angle = this.startAngle + norm * this.angleRange;

    if (this.cap) {
      this.cap.style.transform = `rotate(${angle}deg)`;
    }

    if (this.fillCircle) {
      // 2 * pi * 38 = 238.76104 total circumference; 280deg sweep = 185.70303
      const totalDash = 2 * Math.PI * 38;
      const arcFraction = this.angleRange / 360;
      const arcLength = totalDash * arcFraction;
      const offset = arcLength * (1 - norm);
      this.fillCircle.style.strokeDasharray = `${arcLength.toFixed(3)} 1000`;
      this.fillCircle.style.strokeDashoffset = `${offset.toFixed(3)}`;
      this.fillCircle.style.opacity = norm <= 0.0005 ? '0' : '1';
    }

    if (this.numDisplay) {
      this.numDisplay.textContent = this.formatValue(this.value);
    }

    if (this.element) {
      this.element.setAttribute('aria-valuenow', this.value);
    }

    if (notify && typeof this.onChange === 'function') {
      this.onChange(this.value, this.paramId);
    }
  }

  getValue() {
    return this.value;
  }
}
