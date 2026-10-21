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
      this.keys[i].midiNote = Math.round(69 + 12 * Math.log2(freq / 440));
      const keyEl = this.keys[i].element;
      keyEl.querySelector('.pf02-key-note').textContent = label;
      keyEl.querySelector('.pf02-key-freq').textContent = `${Math.round(freq)}Hz`;
      if (isAccident) {
        keyEl.classList.add('is-accident');
      } else {
        keyEl.classList.remove('is-accident');
      }
    }
  }

  _attachEvents() {
    this.selectRoot.addEventListener('change', (e) => {
      this.rootNote = e.target.value;
      this.updateKeyFrequencies();
    });

    this.selectScale.addEventListener('change', (e) => {
      this.scale = e.target.value;
      this.updateKeyFrequencies();
    });

    // Pitch bend ribbon controller
    let isBending = false;
    const updatePitchBend = (clientX) => {
      const rect = this.pitchbendStrip.getBoundingClientRect();
      const norm = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
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

    // Key press & release handlers
    const pressKey = (index, velocity = 0.85) => {
      if (index < 0 || index >= 16) return;
      const key = this.keys[index];
      if (this.activeKeys.has(index)) return;
      this.activeKeys.add(index);
      key.element.classList.add('is-pressed');
      if (typeof this.onNoteOn === 'function') {
        this.onNoteOn(key.midiNote, key.freqHz, velocity);
      }
    };

    const releaseKey = (index) => {
      if (index < 0 || index >= 16) return;
      const key = this.keys[index];
      if (!this.activeKeys.has(index)) return;
      this.activeKeys.delete(index);
      key.element.classList.remove('is-pressed');
      if (typeof this.onNoteOff === 'function') {
        this.onNoteOff(key.midiNote);
      }
    };

    for (let i = 0; i < 16; i++) {
      const el = this.keys[i].element;
      el.addEventListener('mousedown', (e) => {
        if (e.button !== 0) return;
        pressKey(i, 0.85);
        const onUp = () => {
          releaseKey(i);
          window.removeEventListener('mouseup', onUp);
        };
        window.addEventListener('mouseup', onUp);
      });

      el.addEventListener('touchstart', (e) => {
        pressKey(i, 0.90);
        e.preventDefault();
      });
      el.addEventListener('touchend', () => releaseKey(i));
    }

    // Computer keyboard mapping
    // A W S E D F T G Y H U J K O L P
    const keyMap = {
      'a': 0, 'w': 1, 's': 2, 'e': 3, 'd': 4, 'f': 5,
      't': 6, 'g': 7, 'y': 8, 'h': 9, 'u': 10, 'j': 11,
      'k': 12, 'o': 13, 'l': 14, 'p': 15
    };

    window.addEventListener('keydown', (e) => {
      if (e.repeat || e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
      const idx = keyMap[e.key.toLowerCase()];
      if (idx !== undefined) {
        pressKey(idx, 0.85);
      }
    });

    window.addEventListener('keyup', (e) => {
      const idx = keyMap[e.key.toLowerCase()];
      if (idx !== undefined) {
        releaseKey(idx);
      }
    });
  }
}
