/**
 * @file pf02_audio_worklet.js
 * @brief Real-Time AudioWorkletProcessor for Poopenfarten II (PF-02)
 *
 * Evaluates the 100% procedural physical modeling voice graph and streams
 * 60 Hz VisualizerTelemetryFrame records to the main thread CRT visualizer.
 *
 * ZERO static audio tables! ZERO Base64 PCM arrays!
 */

import { Pf02DspEngine } from './pf02_dsp_engine.js';

const BaseAudioWorkletProcessor = typeof AudioWorkletProcessor !== 'undefined'
  ? AudioWorkletProcessor
  : class {
      constructor() {
        this.port = {
          postMessage() {},
          onmessage: null
        };
      }
    };

export class Pf02AudioWorkletProcessor extends BaseAudioWorkletProcessor {
  constructor(options) {
    super(options);

    const fs = typeof sampleRate !== 'undefined' ? sampleRate : 48000.0;
    this.engine = new Pf02DspEngine(fs);

    // 60 Hz Telemetry Decimation Counter
    this.telemetryInterval = Math.max(64, Math.floor(fs / 60.0)); // ~800 samples at 48k
    this.samplesUntilTelemetry = 0;

    // Handle messages from the UI thread
    this.port.onmessage = (event) => {
      const data = event.data;
      if (!data) return;

      switch (data.type) {
        case 'paramChange':
          this.engine.setParameter(data.paramId, data.value);
          break;

        case 'noteOn':
          this.engine.noteOn(data.note, data.velocity, data.freqHz);
          break;

        case 'noteOff':
          this.engine.noteOff(data.note);
          break;

        case 'pitchBend':
          this.engine.pitchBend(data.semitones);
          break;

        case 'allNotesOff':
          this.engine.allNotesOff();
          break;

        case 'kineticBurst':
          this._handleKineticBurst(data.burstType, data.velocity);
          break;

        case 'setPreset':
          if (data.params) {
            for (const [k, v] of Object.entries(data.params)) {
              this.engine.setParameter(k, v);
            }
          }
          break;

        default:
          break;
      }
    };
  }

  _handleKineticBurst(burstType, velocity = 1.0) {
    this.engine.triggerKineticBurst(burstType, velocity);
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    if (!output || output.length === 0) return true;

    const outL = output[0];
    const outR = output.length > 1 ? output[1] : outL;
    const numSamples = outL.length;

    // Render procedural block
    const { left, right } = this.engine.processBlock(numSamples);

    for (let i = 0; i < numSamples; ++i) {
      outL[i] = left[i];
      outR[i] = right[i];
    }

    // Stream 60 Hz Telemetry Frame to UI thread
    this.samplesUntilTelemetry += numSamples;
    if (this.samplesUntilTelemetry >= this.telemetryInterval) {
      this.samplesUntilTelemetry = 0;
      const frame = this.engine.getTelemetry();
      this.port.postMessage({
        type: 'telemetryFrame',
        frame: frame
      });
    }

    return true;
  }
}

if (typeof registerProcessor !== 'undefined') {
  registerProcessor('pf02-audio-worklet', Pf02AudioWorkletProcessor);
}
