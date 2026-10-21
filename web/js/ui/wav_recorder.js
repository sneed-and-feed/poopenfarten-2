/**
 * @file wav_recorder.js
 * @brief Onboard Lossless 16-Bit Stereo Master WAV Recorder for PF-02
 * Renders standard RIFF WAVE format files from live audio streams
 */

export class PfWavRecorder {
  /**
   * @param {AudioContext} audioCtx
   * @param {Object} options
   */
  constructor(audioCtx, options = {}) {
    this.audioCtx = audioCtx;
    this.onStateChange = options.onStateChange || null;

    this.isRecording = false;
    this.recordedBuffersL = [];
    this.recordedBuffersR = [];
    this.totalSamples = 0;
    this.recordStartTime = 0;
    this.sampleRate = audioCtx ? audioCtx.sampleRate : 48000;

    this.gainNode = null;
    this.processorNode = null;
  }

  attachSource(sourceNode) {
    if (!this.audioCtx || !sourceNode) return;
    this.sampleRate = this.audioCtx.sampleRate;

    // Use a ScriptProcessorNode (or AudioWorklet) for transparent recording taps
    const bufferSize = 4096;
    if (this.audioCtx.createScriptProcessor) {
      this.processorNode = this.audioCtx.createScriptProcessor(bufferSize, 2, 2);
    } else {
      // Fallback
      return;
    }

    this.processorNode.onaudioprocess = (e) => {
      if (!this.isRecording) return;
      const inputL = e.inputBuffer.getChannelData(0);
      const inputR = e.inputBuffer.getChannelData(1);

      const copyL = new Float32Array(inputL.length);
      const copyR = new Float32Array(inputR.length);
      copyL.set(inputL);
      copyR.set(inputR);

      this.recordedBuffersL.push(copyL);
      this.recordedBuffersR.push(copyR);
      this.totalSamples += inputL.length;
    };

    sourceNode.connect(this.processorNode);
    this.processorNode.connect(this.audioCtx.destination);
  }

  start() {
    if (this.isRecording) return;
    this.recordedBuffersL = [];
    this.recordedBuffersR = [];
    this.totalSamples = 0;
    this.isRecording = true;
    this.recordStartTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());

    if (typeof this.onStateChange === 'function') {
      this.onStateChange(true, 0);
    }
  }

  stop() {
    if (!this.isRecording) return null;
    this.isRecording = false;
    const duration = ((typeof performance !== 'undefined' ? performance.now() : Date.now()) - this.recordStartTime) / 1000;

    if (typeof this.onStateChange === 'function') {
      this.onStateChange(false, duration);
    }

    return this.exportWavBlob();
  }

  exportWavBlob() {
    if (this.totalSamples === 0) return null;

    // Merge channel buffers
    const mergedL = new Float32Array(this.totalSamples);
    const mergedR = new Float32Array(this.totalSamples);
    let offset = 0;
    for (let i = 0; i < this.recordedBuffersL.length; i++) {
      mergedL.set(this.recordedBuffersL[i], offset);
      mergedR.set(this.recordedBuffersR[i], offset);
      offset += this.recordedBuffersL[i].length;
    }

    // Interleave to 16-bit PCM: L, R, L, R
    const numChannels = 2;
    const bytesPerSample = 2; // 16-bit
    const blockAlign = numChannels * bytesPerSample;
    const byteRate = this.sampleRate * blockAlign;
    const dataSize = this.totalSamples * blockAlign;
    const buffer = new ArrayBuffer(44 + dataSize);
    const view = new DataView(buffer);

    // RIFF identifier
    this._writeString(view, 0, 'RIFF');
    // file length minus RIFF header
    view.setUint32(4, 36 + dataSize, true);
    // RIFF type
    this._writeString(view, 8, 'WAVE');
    // format chunk identifier
    this._writeString(view, 12, 'fmt ');
    // format chunk length
    view.setUint32(16, 16, true);
    // sample format (1 = PCM)
    view.setUint16(20, 1, true);
    // channel count
    view.setUint16(22, numChannels, true);
    // sample rate
    view.setUint32(24, this.sampleRate, true);
    // byte rate
    view.setUint32(28, byteRate, true);
    // block align
    view.setUint16(32, blockAlign, true);
    // bits per sample
    view.setUint16(34, 16, true);
    // data chunk identifier
    this._writeString(view, 36, 'data');
    // data chunk length
    view.setUint32(40, dataSize, true);

    // Write interleaved 16-bit samples
    let writeOffset = 44;
    for (let i = 0; i < this.totalSamples; i++) {
      // Channel L
      let sL = Math.max(-1.0, Math.min(1.0, mergedL[i]));
      let valL = sL < 0 ? sL * 0x8000 : sL * 0x7FFF;
      view.setInt16(writeOffset, valL, true);
      writeOffset += 2;

      // Channel R
      let sR = Math.max(-1.0, Math.min(1.0, mergedR[i]));
      let valR = sR < 0 ? sR * 0x8000 : sR * 0x7FFF;
      view.setInt16(writeOffset, valR, true);
      writeOffset += 2;
    }

    return new Blob([buffer], { type: 'audio/wav' });
  }

  downloadWav(filename = 'pf02_recording.wav') {
    const blob = this.exportWavBlob();
    if (!blob) return;

    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  _writeString(view, offset, string) {
    for (let i = 0; i < string.length; i++) {
      view.setUint8(offset + i, string.charCodeAt(i));
    }
  }
}
