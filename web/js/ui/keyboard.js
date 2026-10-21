/**
 * @file keyboard.js
 * @brief 16-Key Microtonal & Peristaltic Keyboard Strip for PF-02
 * Tactile acoustic keys, pitch bend ribbon, and scale quantization
 */

export class PfKeyboard {
  /**
   * @param {HTMLElement} container
   * @param {Object} options
   */
  constructor(container, options = {}) {
    this.container = container;
    this.onNoteOn = options.onNoteOn || null;
    this.onNoteOff = options.onNoteOff || null;
    this.onPitchBend = options.onPitchBend || null;

    this.rootNote = options.rootNote || 'C';
    this.scale = options.scale || 'NATURAL_OVERTONE';
    this.pitchBendCents = 0.0;
    this.activeKeys = new Set();
    this.pointerActiveKey = -1;
    this.touchActiveKeys = new Map(); // touchId -> keyIndex
    this.keyboardActiveKeys = new Set();
    this.keys = [];

    this._render();
    this._attachEvents();
    this.updateKeyFrequencies();
  }

  _render() {
    this.container.innerHTML = `
      <div class="pf02-keyboard-header">
        <div style="display: flex; align-items: baseline; gap: 8px;">
          <span class="pf02-deck-title" style="font-size: 10px;">16-TASTEN-KLAVIATUR</span>
          <span class="pf02-deck-subtitle">MIKROTONAL &middot; PERISTALTIK &middot; 14 HZ - 440 HZ</span>
        </div>
        <div class="pf02-keyboard-controls">
          <div class="pf02-select-wrap">
            <span class="pf02-select-label">GRUNDTON:</span>
            <select class="pf02-select pf02-select-root">
              <option value="C" selected>C (65.4 Hz)</option>
              <option value="D">D (73.4 Hz)</option>
              <option value="E">E (82.4 Hz)</option>
              <option value="F">F (87.3 Hz)</option>
              <option value="G">G (98.0 Hz)</option>
              <option value="A">A (110.0 Hz)</option>
            </select>
          </div>
          <div class="pf02-select-wrap">
            <span class="pf02-select-label">SKALA:</span>
            <select class="pf02-select pf02-select-scale">
              <option value="NATURAL_OVERTONE" selected>DARM-OBERTON (1-16)</option>
              <option value="SUB_INFRASONIC">INFRASONIK (7-110 HZ)</option>
              <option value="BUDD_PENTATONIC">VISZERAL PENTATONISCH</option>
              <option value="CHLADNI_MODAL">CHLADNI-MODEN</option>
              <option value="WERCKMEISTER_III">WERCKMEISTER III</option>
              <option value="ULTRAMETRIC_P_ADIC">3-ADISCHE ULTRAMETRIK</option>
            </select>
          </div>
          <div class="pf02-pitchbend-strip" title="Pitch Bend Strip (&plusmn;200 Cents)">
            <div class="pf02-pitchbend-thumb"></div>
          </div>
        </div>
      </div>
      <div class="pf02-keys-strip"></div>
    `;

    this.keysStrip = this.container.querySelector('.pf02-keys-strip');
    this.selectRoot = this.container.querySelector('.pf02-select-root');
    this.selectScale = this.container.querySelector('.pf02-select-scale');
    this.pitchbendStrip = this.container.querySelector('.pf02-pitchbend-strip');
    this.pitchbendThumb = this.container.querySelector('.pf02-pitchbend-thumb');

    // Build 16 key elements
    this.keysStrip.innerHTML = '';
    for (let i = 0; i < 16; i++) {
      const keyEl = document.createElement('div');
      keyEl.className = 'pf02-key';
      keyEl.dataset.index = i;
      keyEl.innerHTML = `
        <span class="pf02-key-note">K${i + 1}</span>
        <span class="pf02-key-freq">--</span>
      `;
      this.keysStrip.appendChild(keyEl);
      this.keys.push({
        element: keyEl,
        index: i,
        midiNote: 36 + i,
        freqHz: 65.41
      });
    }
  }

  getRootFreq() {
    const rootTable = {
      'C': 65.406,
      'D': 73.416,
      'E': 82.407,
      'F': 87.307,
      'G': 97.999,
      'A': 110.000
    };
    return rootTable[this.rootNote] || 65.406;
  }

  updateKeyFrequencies() {
    const f0 = this.getRootFreq();
    for (let i = 0; i < 16; i++) {
      let freq = f0;
      let label = `K${i + 1}`;
      let isAccident = false;

      switch (this.scale) {
        case 'NATURAL_OVERTONE':
          freq = f0 * (i + 1);
          label = `${i + 1}x`;
          break;
        case 'SUB_INFRASONIC':
          // Subharmonics down to 7 Hz
          freq = Math.max(7.0, (f0 * 2) / (17 - i));
          label = `-${17 - i}`;
          break;
        case 'BUDD_PENTATONIC': {
          const ratios = [1.0, 9/8, 5/4, 3/2, 5/3, 2.0, 9/4, 5/2, 3.0, 10/3, 4.0, 9/2, 5.0, 6.0, 20/3, 8.0];
          freq = f0 * ratios[i];
          const names = ['1', '9/8', '5/4', '3/2', '5/3', '2', '9/4', '5/2', '3', '10/3', '4', '9/2', '5', '6', '20/3', '8'];
          label = names[i];
          break;
        }
        case 'CHLADNI_MODAL': {
          // Annular plate mode dispersion f ~ (m + 2n)^2
          const chladniRatios = [1.0, 2.08, 3.41, 4.98, 6.82, 8.89, 11.19, 13.73, 16.51, 19.51, 22.75, 26.22, 29.93, 33.87, 38.04, 42.45];
          freq = (f0 * 0.25) * chladniRatios[i];
          label = `Ch${i + 1}`;
          break;
        }
        case 'WERCKMEISTER_III': {
          const semitones = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
          freq = f0 * Math.pow(2, semitones[i] / 12);
          const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B', 'C+', 'C#+', 'D+', 'D#+'];
          label = noteNames[i];
          if (label.includes('#')) isAccident = true;
          break;
        }
        case 'ULTRAMETRIC_P_ADIC': {
          // 3-adic norm: |x|_3 = 3^(-v_3(x))
          const v3 = [0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4];
          freq = f0 * (1.0 + v3[i] * 0.75);
          label = `|x|${v3[i]}`;
          break;
        }
      }

      this.keys[i].freqHz = freq;
      // Assign unique canonical MIDI note per key index so microtonal/infrasonic/p-adic
      // keys never collide on noteOff during rollover, while passing exact freqHz to DSP.
      this.keys[i].midiNote = 36 + i;
      const keyEl = this.keys[i].element;
      keyEl.querySelector('.pf02-key-note').textContent = label;
      keyEl.querySelector('.pf02-key-freq').textContent = `${Math.round(freq)}Hz`;
      if (isAccident) {
        keyEl.classList.add('is-accident');
      } else {
        keyEl.classList.remove('is-accident');
      }
    }

    // If keys are currently held when scale/root changes, update active note pitches in insertion order
    if (typeof this.onNoteOn === 'function') {
      for (const idx of this.activeKeys) {
        if (idx >= 0 && idx < 16) {
          this.onNoteOn(this.keys[idx].midiNote, this.keys[idx].freqHz, 0.85);
        }
      }
    }
  }

  _isTouchActiveOnKey(index) {
    for (const kIdx of this.touchActiveKeys.values()) {
      if (kIdx === index) return true;
    }
    return false;
  }

  _isKeyHeldByAnySource(index) {
    return (
      this.pointerActiveKey === index ||
      this.keyboardActiveKeys.has(index) ||
      this._isTouchActiveOnKey(index)
    );
  }

  _activateKey(index, velocity = 0.85) {
    if (index < 0 || index >= 16) return;
    const key = this.keys[index];
    if (this.activeKeys.has(index)) return;
    this.activeKeys.add(index);
    key.element.classList.add('is-pressed');
    if (typeof this.onNoteOn === 'function') {
      this.onNoteOn(key.midiNote, key.freqHz, velocity);
    }
  }

  _deactivateKeyIfReleased(index) {
    if (index < 0 || index >= 16) return;
    if (this._isKeyHeldByAnySource(index)) return;
    const key = this.keys[index];
    if (!this.activeKeys.has(index)) return;
    this.activeKeys.delete(index);
    key.element.classList.remove('is-pressed');
    if (typeof this.onNoteOff === 'function') {
      this.onNoteOff(key.midiNote);
    }
  }

  releaseAllKeys() {
    if (this._cleanupMouseGlissando) {
      this._cleanupMouseGlissando();
    }
    this.pointerActiveKey = -1;
    this.touchActiveKeys.clear();
    this.keyboardActiveKeys.clear();
    const held = Array.from(this.activeKeys);
    for (const idx of held) {
      this._deactivateKeyIfReleased(idx);
    }
  }

  _getKeyIndexFromPoint(clientX, clientY, fallbackTarget = null) {
    if (typeof document !== 'undefined' && typeof document.elementFromPoint === 'function' &&
        Number.isFinite(clientX) && Number.isFinite(clientY) && (clientX !== 0 || clientY !== 0)) {
      const el = document.elementFromPoint(clientX, clientY);
      const keyEl = el && typeof el.closest === 'function' ? el.closest('.pf02-key') : null;
      if (keyEl && this.keysStrip.contains(keyEl)) {
        const idx = parseInt(keyEl.dataset.index, 10);
        if (idx >= 0 && idx < 16) return idx;
      }
      // Horizontal strip hit-test fallback so fast glissando across 1px key borders never drops
      const rect = this.keysStrip.getBoundingClientRect();
      if (rect.width > 0 && clientY >= rect.top - 12 && clientY <= rect.bottom + 12 &&
          clientX >= rect.left && clientX <= rect.right) {
        const normX = (clientX - rect.left) / rect.width;
        const idx = Math.max(0, Math.min(15, Math.floor(normX * 16)));
        return idx;
      }
    }
    if (fallbackTarget && typeof fallbackTarget.closest === 'function') {
      const keyEl = fallbackTarget.closest('.pf02-key');
      if (keyEl && this.keysStrip.contains(keyEl)) {
        const idx = parseInt(keyEl.dataset.index, 10);
        if (idx >= 0 && idx < 16) return idx;
      }
    }
    return -1;
  }

  _attachEvents() {
    this.selectRoot.addEventListener('change', (e) => {
      this.rootNote = e.target.value;
      this.updateKeyFrequencies();
      if (typeof e.target.blur === 'function') e.target.blur();
    });

    this.selectScale.addEventListener('change', (e) => {
      this.scale = e.target.value;
      this.updateKeyFrequencies();
      if (typeof e.target.blur === 'function') e.target.blur();
    });

    // Pitch bend ribbon controller
    let isBending = false;
    const updatePitchBend = (clientX) => {
      const rect = this.pitchbendStrip.getBoundingClientRect();
      const norm = Math.max(0, Math.min(1, (clientX - rect.left) / Math.max(1, rect.width)));
      this.pitchBendCents = (norm - 0.5) * 400.0; // -200 to +200 cents
      this.pitchbendThumb.style.left = `${norm * 100}%`;
      if (typeof this.onPitchBend === 'function') {
        this.onPitchBend(this.pitchBendCents);
      }
    };

    this.pitchbendStrip.addEventListener('mousedown', (e) => {
      isBending = true;
      updatePitchBend(e.clientX);
      const onMove = (ev) => updatePitchBend(ev.clientX);
      const onUp = () => {
        isBending = false;
        // Spring back to center
        this.pitchBendCents = 0.0;
        this.pitchbendThumb.style.left = '50%';
        if (typeof this.onPitchBend === 'function') {
          this.onPitchBend(0.0);
        }
        window.removeEventListener('mousemove', onMove);
        window.removeEventListener('mouseup', onUp);
      };
      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    });

    // Touch support for pitch bend
    this.pitchbendStrip.addEventListener('touchstart', (e) => {
      if (e.touches.length !== 1) return;
      isBending = true;
      updatePitchBend(e.touches[0].clientX);
      const onTouchMove = (ev) => {
        if (ev.touches.length === 1) updatePitchBend(ev.touches[0].clientX);
      };
      const onTouchEnd = () => {
        isBending = false;
        this.pitchBendCents = 0.0;
        this.pitchbendThumb.style.left = '50%';
        if (typeof this.onPitchBend === 'function') this.onPitchBend(0.0);
        window.removeEventListener('touchmove', onTouchMove);
        window.removeEventListener('touchend', onTouchEnd);
      };
      window.addEventListener('touchmove', onTouchMove);
      window.addEventListener('touchend', onTouchEnd);
    });

    // Mouse / Pointer Glissando Rollover across all 16 keys
    const startMouseGlissando = (initialIndex, e) => {
      if (initialIndex < 0 || initialIndex >= 16) return;
      if (this._cleanupMouseGlissando) {
        this._cleanupMouseGlissando();
      }
      const prevIdx = this.pointerActiveKey;
      this.pointerActiveKey = initialIndex;
      // Activate new key BEFORE releasing old key for true Mono-Legato glissando
      this._activateKey(initialIndex, 0.85);
      if (prevIdx !== -1 && prevIdx !== initialIndex) {
        this._deactivateKeyIfReleased(prevIdx);
      }

      const onMouseUp = () => {
        const oldIdx = this.pointerActiveKey;
        this.pointerActiveKey = -1;
        if (oldIdx !== -1) {
          this._deactivateKeyIfReleased(oldIdx);
        }
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
        this._cleanupMouseGlissando = null;
      };

      const onMouseMove = (ev) => {
        if (ev && ev.buttons !== undefined && (ev.buttons & 1) === 0) {
          onMouseUp();
          return;
        }
        const hoveredIdx = this._getKeyIndexFromPoint(ev.clientX, ev.clientY, ev.target);
        if (hoveredIdx !== this.pointerActiveKey) {
          const oldIdx = this.pointerActiveKey;
          this.pointerActiveKey = hoveredIdx;
          if (hoveredIdx !== -1) {
            this._activateKey(hoveredIdx, 0.85);
          }
          if (oldIdx !== -1) {
            this._deactivateKeyIfReleased(oldIdx);
          }
        }
      };

      this._cleanupMouseGlissando = () => {
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
        this._cleanupMouseGlissando = null;
      };

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    };

    const syncTouches = (touchList, velocity = 0.90) => {
      const nextMap = new Map();
      for (let t = 0; t < touchList.length; t++) {
        const touch = touchList[t];
        const idx = this._getKeyIndexFromPoint(touch.clientX, touch.clientY, touch.target);
        if (idx !== -1) {
          nextMap.set(touch.identifier, idx);
        }
      }
      const prevMap = this.touchActiveKeys;
      this.touchActiveKeys = nextMap;

      // Activate newly touched keys first (legato rollover)
      for (const idx of nextMap.values()) {
        this._activateKey(idx, velocity);
      }
      // Deactivate keys no longer touched
      for (const oldIdx of prevMap.values()) {
        this._deactivateKeyIfReleased(oldIdx);
      }
    };

    for (let i = 0; i < 16; i++) {
      const el = this.keys[i].element;
      el.addEventListener('mousedown', (e) => {
        if (e.button !== 0) return;
        e.preventDefault();
        startMouseGlissando(i, e);
      });

      el.addEventListener('mouseenter', (e) => {
        if ((e.buttons & 1) === 1 && this.pointerActiveKey !== -1 && this.pointerActiveKey !== i) {
          const oldIdx = this.pointerActiveKey;
          this.pointerActiveKey = i;
          this._activateKey(i, 0.85);
          this._deactivateKeyIfReleased(oldIdx);
        }
      });

      el.addEventListener('touchstart', (e) => {
        e.preventDefault();
        if (e.touches && e.touches.length > 0) {
          syncTouches(e.touches, 0.90);
        } else {
          // Fallback for synthetic TouchEvent without touches array
          this.touchActiveKeys.set('synth', i);
          this._activateKey(i, 0.90);
        }
      });

      el.addEventListener('touchmove', (e) => {
        e.preventDefault();
        if (e.touches) syncTouches(e.touches, 0.90);
      });

      el.addEventListener('touchend', (e) => {
        e.preventDefault();
        if (e.touches && e.touches.length > 0) {
          syncTouches(e.touches, 0.90);
        } else {
          this.touchActiveKeys.delete('synth');
          syncTouches([], 0.90);
          this._deactivateKeyIfReleased(i);
        }
      });

      el.addEventListener('touchcancel', (e) => {
        e.preventDefault();
        const prevTouched = Array.from(new Set(this.touchActiveKeys.values()));
        this.touchActiveKeys.clear();
        for (const oldIdx of prevTouched) {
          this._deactivateKeyIfReleased(oldIdx);
        }
        this._deactivateKeyIfReleased(i);
      });
    }

    // Computer keyboard mapping (supports both physical e.code and e.key for N-key rollover)
    // A W S E D F T G Y H U J K O L P
    const keyMap = {
      'a': 0, 'w': 1, 's': 2, 'e': 3, 'd': 4, 'f': 5,
      't': 6, 'g': 7, 'y': 8, 'h': 9, 'u': 10, 'j': 11,
      'k': 12, 'o': 13, 'l': 14, 'p': 15
    };
    const codeMap = {
      'KeyA': 0, 'KeyW': 1, 'KeyS': 2, 'KeyE': 3, 'KeyD': 4, 'KeyF': 5,
      'KeyT': 6, 'KeyG': 7, 'KeyY': 8, 'KeyH': 9, 'KeyU': 10, 'KeyJ': 11,
      'KeyK': 12, 'KeyO': 13, 'KeyL': 14, 'KeyP': 15
    };

    const resolveKeyboardIndex = (e) => {
      if (e.code && codeMap[e.code] !== undefined) return codeMap[e.code];
      if (e.key && typeof e.key === 'string') {
        const k = e.key.toLowerCase();
        if (keyMap[k] !== undefined) return keyMap[k];
      }
      return undefined;
    };

    window.addEventListener('keydown', (e) => {
      if (e.repeat || e.ctrlKey || e.altKey || e.metaKey) return;
      const tag = e.target && e.target.tagName ? e.target.tagName.toUpperCase() : '';
      if (tag === 'INPUT' || tag === 'TEXTAREA' || (e.target && e.target.isContentEditable)) return;

      const idx = resolveKeyboardIndex(e);
      if (idx !== undefined) {
        e.preventDefault();
        this.keyboardActiveKeys.add(idx);
        this._activateKey(idx, 0.85);
      }
    });

    window.addEventListener('keyup', (e) => {
      const idx = resolveKeyboardIndex(e);
      if (idx !== undefined) {
        this.keyboardActiveKeys.delete(idx);
        this._deactivateKeyIfReleased(idx);
      }
    });

    window.addEventListener('blur', () => {
      this.releaseAllKeys();
    });

    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
          this.releaseAllKeys();
        }
      });
    }
  }
}
