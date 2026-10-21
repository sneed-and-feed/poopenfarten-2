#include "PluginEditor.h"

#include <cmath>
#include <cstring>
#include <vector>

#if __has_include(<BinaryData.h>)
    #include <BinaryData.h>
    #define PF02_HAS_BINARY_DATA 1
#elif __has_include("BinaryData.h")
    #include "BinaryData.h"
    #define PF02_HAS_BINARY_DATA 1
#else
    #define PF02_HAS_BINARY_DATA 0
#endif

namespace {

// Minimal German functionalist fallback HTML in case disk and zip assets are missing
static constexpr const char* kEmbeddedFallbackHtml = R"html(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>PF-02 · Darm-Akustischer &amp; Ultrametrischer Triaden-Synthesizer</title>
<style>
  :root {
    --bg-main: #141517;
    --card-bg: #1A1D20;
    --text-main: #ECEBE4;
    --text-dim: #8A8F98;
    --accent-orange: #EE592B;
    --phosphor-green: #24FF6A;
    --border-color: #2E3338;
  }
  body {
    margin: 0; padding: 24px;
    background: var(--bg-main);
    color: var(--text-main);
    font-family: "DIN 1451 Mittelschrift", -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    user-select: none;
  }
  header {
    border-bottom: 2px solid var(--border-color);
    padding-bottom: 12px;
    margin-bottom: 20px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  h1 { margin: 0; font-size: 18px; letter-spacing: 2px; font-weight: 700; }
  p.subtitle { margin: 4px 0 0; font-size: 11px; color: var(--text-dim); letter-spacing: 1px; }
  .badge { background: var(--accent-orange); color: #fff; padding: 4px 10px; border-radius: 2px; font-size: 10px; font-weight: 700; }
  .notice { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 4px; padding: 16px; margin-top: 20px; }
</style>
</head>
<body>
<header>
  <div>
    <h1>PF-02</h1>
    <p class="subtitle">DARM-AKUSTISCHER &amp; ULTRAMETRISCHER TRIADEN-SYNTHESIZER</p>
  </div>
  <div class="badge">AKTIV</div>
</header>
<div class="notice">
  <p><strong>PF-02 Labor-Synthesizer initialisiert.</strong></p>
  <p style="font-size:12px;color:var(--text-dim);">Audio- und Steuerungskanäle sind aktiv. Die grafische Benutzeroberfläche lädt aus dem eingebetteten Ressourcen-Archiv.</p>
</div>
</body>
</html>
)html";

} // namespace

// ============================================================================
// Constructor & Destructor
// ============================================================================
Pf02AudioProcessorEditor::Pf02AudioProcessorEditor(Pf02AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setOpaque(true);

    for (size_t i = 0; i < pf02::parameters::kNumParameters; ++i) {
        pendingParamValues[i].store(0.0f, std::memory_order_relaxed);
        paramDirty[i].store(false, std::memory_order_relaxed);
    }

#if JUCE_WEB_BROWSER
    webComponent = std::make_unique<juce::WebBrowserComponent>(createWebOptions(*this));
    webComponent->setOpaque(true);
    addAndMakeVisible(*webComponent);
    webComponent->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
#endif

    // Register listeners for all APVTS parameters to detect DAW automation
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        processorRef.getAPVTS().addParameterListener(paramIdStr, this);
    }

    // Default 19" 2U rack editor dimensions (1240 x 760, resizable)
    setSize(1240, 760);
    setResizable(true, true);
    setResizeLimits(900, 560, 2560, 1440);

    startTimerHz(60);
}

Pf02AudioProcessorEditor::~Pf02AudioProcessorEditor()
{
    stopTimer();

    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        processorRef.getAPVTS().removeParameterListener(paramIdStr, this);
    }
}

void Pf02AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141517));
}

void Pf02AudioProcessorEditor::resized()
{
    if (webComponent != nullptr) {
        webComponent->setBounds(getLocalBounds());
    }
}

// ============================================================================
// APVTS Listener
// ============================================================================
void Pf02AudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size() && i < pf02::parameters::kNumParameters; ++i) {
        if (parameterID == juce::String(defs[i].idString.data(), defs[i].idString.size())) {
            pendingParamValues[i].store(newValue, std::memory_order_relaxed);
            paramDirty[i].store(true, std::memory_order_release);
            break;
        }
    }
}

// ============================================================================
// Web Options & Bridge Initialization
// ============================================================================
juce::WebBrowserComponent::Options Pf02AudioProcessorEditor::createWebOptions(Pf02AudioProcessorEditor& editor)
{
#if JUCE_WINDOWS
    // Configure WebView2 arguments for low-latency host DAW embedding:
    // - Mute browser audio output (C++ DSP engine handles all audio synthesis)
    // - Disable Web MIDI in Chromium (prevents WinMM device contention with DAW MIDI inputs)
    // - Disable background Chromium features that create unneeded threads / network queries
    // - Disable CalculateNativeWinOcclusion to eliminate global SetWinEventHook desktop dragging lag
    _wputenv_s(
        L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
        L"--mute-audio "
        L"--disable-audio-output "
        L"--disable-web-midi "
        L"--disable-features=Translate,OptimizationHints,MediaRouter,InterestFeedContentSuggestions,CalculateNativeWinOcclusion"
    );
#endif

    // Pre-populate initial parameter values into JSON dictionary
    juce::String initialParamsJson = "{\n";
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const auto& def = defs[i];
        const juce::String paramIdStr(def.idString.data(), def.idString.size());
        if (auto* rawVal = editor.processorRef.getAPVTS().getRawParameterValue(paramIdStr)) {
            const float val = rawVal->load(std::memory_order_relaxed);
            initialParamsJson += "  \"" + paramIdStr + "\": " + juce::String(val, 6) + ",\n";
            const juce::String webPropStr(def.webProperty.data(), def.webProperty.size());
            if (webPropStr != paramIdStr) {
                initialParamsJson += "  \"" + webPropStr + "\": " + juce::String(val, 6) + ",\n";
            }
        }
    }
    initialParamsJson += "  \"currentProgram\": " + juce::String(editor.processorRef.getCurrentProgram()) + "\n";
    initialParamsJson += "}";

    // Injected user script bridging postMessage and native JUCE events
    const juce::String userScript =
        "window.__IS_JUCE__ = true;\n"
        "window.__HAS_LIVE_TELEMETRY__ = true;\n"
        "window.__JUCE_INITIAL_PARAMS__ = " + initialParamsJson + ";\n"
        R"js(
(function() {
  var webviewListeners = [];
  if (typeof window.chrome === 'undefined') window.chrome = {};
  if (typeof window.chrome.webview === 'undefined') {
    window.chrome.webview = {
      addEventListener: function(type, listener) {
        if (type === 'message') webviewListeners.push(listener);
      },
      removeEventListener: function(type, listener) {
        webviewListeners = webviewListeners.filter(function(l) { return l !== listener; });
      },
      postMessage: function(msg) {
        if (window.__JUCE__ && window.__JUCE__.backend) {
          var evtType = (msg && msg.type) ? msg.type : 'message';
          window.__JUCE__.backend.emitEvent(evtType, msg);
        }
      }
    };
  } else {
    var origAdd = window.chrome.webview.addEventListener ? window.chrome.webview.addEventListener.bind(window.chrome.webview) : null;
    if (origAdd) {
      window.chrome.webview.addEventListener = function(type, listener, options) {
        if (type === 'message') webviewListeners.push(listener);
        return origAdd(type, listener, options);
      };
    }
    var origPost = window.chrome.webview.postMessage ? window.chrome.webview.postMessage.bind(window.chrome.webview) : null;
    if (origPost) {
      window.chrome.webview.postMessage = function(msg) {
        if (window.__JUCE__ && window.__JUCE__.backend) {
          var evtType = (msg && msg.type) ? msg.type : 'message';
          window.__JUCE__.backend.emitEvent(evtType, msg);
        } else {
          origPost(msg);
        }
      };
    }
  }

  window.__pf02_dispatchToWebview = function(msg) {
    var ev = { data: msg };
    for (var i = 0; i < webviewListeners.length; ++i) {
      try { webviewListeners[i](ev); } catch (e) { console.error(e); }
    }
  };

  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.addEventListener('telemetryFrame', function(frame) {
      window.__pf02_dispatchToWebview({ type: 'telemetryFrame', frame: frame });
    });
    window.__JUCE__.backend.addEventListener('paramUpdate', function(data) {
      window.__pf02_dispatchToWebview({ type: 'paramUpdate', paramId: data.paramId, value: data.value });
    });
  }

  window.addEventListener('contextmenu', function(e) { if (!e.defaultPrevented) e.preventDefault(); }, false);
})();
)js";

    auto options = juce::WebBrowserComponent::Options{}
#if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory).getChildFile("Pf02_WebView2"))
                .withBackgroundColour(juce::Colour(0xff141517)))
#endif
        .withUserScript(userScript)
        .withNativeIntegrationEnabled()
        .withResourceProvider([&editor](const juce::String& url) {
            return editor.getResource(url);
        })
        .withEventListener("paramChange", [&editor](const juce::var& data) {
            editor.handleParamChangeFromWeb(data);
        })
        .withEventListener("kineticBurst", [&editor](const juce::var& data) {
            editor.handleKineticBurstFromWeb(data);
        })
        .withEventListener("noteOn", [&editor](const juce::var& data) {
            editor.handleNoteOnFromWeb(data);
        })
        .withEventListener("noteOff", [&editor](const juce::var& data) {
            editor.handleNoteOffFromWeb(data);
        })
        .withEventListener("pitchBend", [&editor](const juce::var& data) {
            editor.handlePitchBendFromWeb(data);
        })
        .withEventListener("macroChange", [&editor](const juce::var& data) {
            editor.handleMacroChangeFromWeb(data);
        })
        .withEventListener("presetChange", [&editor](const juce::var& data) {
            editor.handlePresetChangeFromWeb(data);
        })
        .withEventListener("requestSync", [&editor](const juce::var& /*data*/) {
            editor.syncAllParametersToWeb();
        });

    return options;
}

// ============================================================================
// Resource Provider (Local Files & Embedded Zip Extraction)
// ============================================================================
std::optional<juce::WebBrowserComponent::Resource> Pf02AudioProcessorEditor::getResource(const juce::String& url)
{
    juce::String path = url;

    // Strip virtual hostname prefixes
    if (path.startsWithIgnoreCase("https://juce.backend/")) path = path.substring(21);
    else if (path.startsWithIgnoreCase("http://juce.backend/")) path = path.substring(20);
    else if (path.startsWithIgnoreCase("juce://juce.backend/")) path = path.substring(20);
    else if (path.startsWithIgnoreCase("https://juce.backend")) path = path.substring(20);
    else if (path.startsWithIgnoreCase("http://juce.backend")) path = path.substring(19);
    else if (path.startsWithIgnoreCase("juce://juce.backend")) path = path.substring(19);

    const int queryIdx = path.indexOfChar('?');
    if (queryIdx >= 0) path = path.substring(0, queryIdx);
    const int hashIdx = path.indexOfChar('#');
    if (hashIdx >= 0) path = path.substring(0, hashIdx);

    while (path.startsWithChar('/') || path.startsWithChar('\\') || path.startsWith("./")) {
        if (path.startsWithChar('/') || path.startsWithChar('\\')) path = path.substring(1);
        else if (path.startsWith("./")) path = path.substring(2);
    }

    if (path.startsWithIgnoreCase("web/")) path = path.substring(4);

    while (path.startsWithChar('/') || path.startsWithChar('\\')) path = path.substring(1);

    if (path.isEmpty()) path = "index.html";

    juce::String mimeType = "application/octet-stream";
    if (path.endsWithIgnoreCase(".html") || path.endsWithIgnoreCase(".htm")) mimeType = "text/html; charset=utf-8";
    else if (path.endsWithIgnoreCase(".css")) mimeType = "text/css; charset=utf-8";
    else if (path.endsWithIgnoreCase(".js") || path.endsWithIgnoreCase(".mjs")) mimeType = "text/javascript; charset=utf-8";
    else if (path.endsWithIgnoreCase(".json")) mimeType = "application/json; charset=utf-8";
    else if (path.endsWithIgnoreCase(".svg")) mimeType = "image/svg+xml";
    else if (path.endsWithIgnoreCase(".png")) mimeType = "image/png";
    else if (path.endsWithIgnoreCase(".jpg") || path.endsWithIgnoreCase(".jpeg")) mimeType = "image/jpeg";
    else if (path.endsWithIgnoreCase(".woff2")) mimeType = "font/woff2";
    else if (path.endsWithIgnoreCase(".woff")) mimeType = "font/woff";
    else if (path.endsWithIgnoreCase(".ttf")) mimeType = "font/ttf";
    else if (path.endsWithIgnoreCase(".wasm")) mimeType = "application/wasm";

    // 1. Search local disk (for live development and testing)
    auto checkDiskFile = [&](const juce::File& file) -> std::optional<juce::WebBrowserComponent::Resource> {
        if (file.existsAsFile()) {
            juce::MemoryBlock mb;
            if (file.loadFileAsData(mb)) {
                std::vector<std::byte> bytes(mb.getSize());
                std::memcpy(bytes.data(), mb.getData(), mb.getSize());
                return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType };
            }
        }
        return std::nullopt;
    };

    const juce::File cwd = juce::File::getCurrentWorkingDirectory();
    if (auto res = checkDiskFile(cwd.getChildFile("web").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("poopenfarten-2/web").getChildFile(path))) return res;

    // Search relative to executable
    auto dir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile).getParentDirectory();
    for (int depth = 0; depth < 5; ++depth) {
        if (auto res = checkDiskFile(dir.getChildFile("web").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("poopenfarten-2/web").getChildFile(path))) return res;
        dir = dir.getParentDirectory();
    }

    // 2. Unpack from embedded binary zip archive (Pf02WebAssets)
#if PF02_HAS_BINARY_DATA
    int zipSize = 0;
    const char* zipData = nullptr;

    #if defined(BinaryData_web_assets_pf02_zip)
    if (BinaryData::web_assets_pf02_zipSize > 0 && BinaryData::web_assets_pf02_zip != nullptr) {
        zipData = BinaryData::web_assets_pf02_zip;
        zipSize = BinaryData::web_assets_pf02_zipSize;
    }
    #endif

    if (zipData == nullptr) {
        zipData = BinaryData::getNamedResource("web_assets_pf02_zip", zipSize);
    }

    if (zipData != nullptr && zipSize > 0) {
        juce::MemoryInputStream memStream(zipData, static_cast<size_t>(zipSize), false);
        juce::ZipFile zip(memStream);

        juce::String normalizedPath = path.replaceCharacter('\\', '/');
        while (normalizedPath.startsWithChar('/') || normalizedPath.startsWith("./")) {
            if (normalizedPath.startsWithChar('/')) normalizedPath = normalizedPath.substring(1);
            else if (normalizedPath.startsWith("./")) normalizedPath = normalizedPath.substring(2);
        }
        if (normalizedPath.startsWithIgnoreCase("web/")) normalizedPath = normalizedPath.substring(4);

        int entryIndex = zip.getIndexOfFileName(normalizedPath);
        if (entryIndex < 0) {
            for (int i = 0; i < zip.getNumEntries(); ++i) {
                const auto* entry = zip.getEntry(i);
                if (entry != nullptr) {
                    juce::String name = entry->filename.replaceCharacter('\\', '/');
                    while (name.startsWithChar('/') || name.startsWith("./")) {
                        if (name.startsWithChar('/')) name = name.substring(1);
                        else if (name.startsWith("./")) name = name.substring(2);
                    }
                    if (name.startsWithIgnoreCase("web/")) name = name.substring(4);

                    if (name.equalsIgnoreCase(normalizedPath)) {
                        entryIndex = i;
                        break;
                    }
                }
            }
        }

        if (entryIndex >= 0) {
            const auto* entry = zip.getEntry(entryIndex);
            if (entry != nullptr) {
                std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(*entry));
                if (stream != nullptr) {
                    juce::MemoryBlock mb;
                    stream->readIntoMemoryBlock(mb, -1);
                    std::vector<std::byte> data(mb.getSize());
                    std::memcpy(data.data(), mb.getData(), mb.getSize());
                    return juce::WebBrowserComponent::Resource { std::move(data), mimeType };
                }
            }
        }
    }
#endif

    // 3. Fallback embedded HTML
    if (path.equalsIgnoreCase("index.html")) {
        const size_t len = std::strlen(kEmbeddedFallbackHtml);
        std::vector<std::byte> bytes(len);
        std::memcpy(bytes.data(), kEmbeddedFallbackHtml, len);
        return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType };
    }

    return std::nullopt;
}

// ============================================================================
// Web Message Bridge Handlers
// ============================================================================
void Pf02AudioProcessorEditor::handleParamChangeFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;

    juce::String id = data.getProperty("paramId", data.getProperty("id", "")).toString();
    const float val = static_cast<float>(data.getProperty("value", 0.0));

    if (id.isEmpty()) return;

    if (id.equalsIgnoreCase("requestSync") || id.equalsIgnoreCase("requestState")) {
        syncAllParametersToWeb();
        return;
    }

    if (id == "param_power_active" || id == "powerActive" || id == "power") {
        processorRef.setPower(val >= 0.5f);
        return;
    }

    if (id == "macro_gut_squeeze" || id == "macroGutSqueeze") {
        processorRef.setMacro(id, val);
        return;
    }

    if (id == "macro_fluid_moisture" || id == "macroFluidMoisture") {
        processorRef.setMacro(id, val);
        return;
    }

    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (const auto& def : defs) {
        const juce::String idStr(def.idString.data(), def.idString.size());
        const juce::String webPropStr(def.webProperty.data(), def.webProperty.size());

        if (id == idStr || id == webPropStr) {
            if (auto* param = processorRef.getAPVTS().getParameter(idStr)) {
                float targetVal = val;
                if (!def.unit.empty() && std::string_view(def.unit) == "%" && def.maxValue <= 1.0f && targetVal > 1.0f) {
                    targetVal *= 0.01f;
                }
                const float norm = (def.type == pf02::parameters::ParameterType::Choice)
                                     ? param->convertTo0to1(std::round(targetVal))
                                     : param->convertTo0to1(targetVal);
                param->setValueNotifyingHost(std::clamp(norm, 0.0f, 1.0f));
            }
            break;
        }
    }
}

void Pf02AudioProcessorEditor::handleKineticBurstFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    const juce::String burstType = data.getProperty("burstType", "impulse").toString();
    const float velocity = static_cast<float>(data.getProperty("velocity", 1.0));
    processorRef.triggerKineticBurst(burstType, velocity);
}

void Pf02AudioProcessorEditor::handleNoteOnFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    const int midiNote = static_cast<int>(data.getProperty("midiNote", 60));
    const float velocity = static_cast<float>(data.getProperty("velocity", 0.8));
    processorRef.noteOn(midiNote, velocity);
}

void Pf02AudioProcessorEditor::handleNoteOffFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    const int midiNote = static_cast<int>(data.getProperty("midiNote", 60));
    processorRef.noteOff(midiNote);
}

void Pf02AudioProcessorEditor::handlePitchBendFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    if (data.hasProperty("semitones")) {
        const float semi = static_cast<float>(data.getProperty("semitones", 0.0));
        processorRef.pitchBend(semi);
    } else if (data.hasProperty("cents")) {
        const float cents = static_cast<float>(data.getProperty("cents", 0.0));
        processorRef.pitchBend(cents * 0.01f);
    }
}

void Pf02AudioProcessorEditor::handleMacroChangeFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    const juce::String macroId = data.getProperty("macroId", "").toString();
    const float val = static_cast<float>(data.getProperty("value", 0.5));
    processorRef.setMacro(macroId, val);
}

void Pf02AudioProcessorEditor::handlePresetChangeFromWeb(const juce::var& data)
{
    if (!data.isObject()) return;
    const int idx = static_cast<int>(data.getProperty("presetIndex", data.getProperty("index", 0)));
    processorRef.setCurrentProgram(idx);
    syncAllParametersToWeb();
}

void Pf02AudioProcessorEditor::sendParameterUpdateToWeb(const juce::String& paramId, float newValue)
{
    if (!webComponent) return;
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("paramId", paramId);
    obj->setProperty("value", newValue);
    webComponent->emitEventIfBrowserIsVisible("paramUpdate", juce::var(obj.get()));
}

void Pf02AudioProcessorEditor::syncAllParametersToWeb()
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (const auto& def : defs) {
        const juce::String paramIdStr(def.idString.data(), def.idString.size());
        if (auto* rawVal = processorRef.getAPVTS().getRawParameterValue(paramIdStr)) {
            sendParameterUpdateToWeb(paramIdStr, rawVal->load(std::memory_order_relaxed));
        }
    }
}

// ============================================================================
// 60 Hz Telemetry & Coalesced Parameter Push Timer
// ============================================================================
void Pf02AudioProcessorEditor::timerCallback()
{
    // 1. Dispatch coalesced parameter changes
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size() && i < pf02::parameters::kNumParameters; ++i) {
        if (paramDirty[i].exchange(false, std::memory_order_acquire)) {
            const float val = pendingParamValues[i].load(std::memory_order_relaxed);
            const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
            sendParameterUpdateToWeb(paramIdStr, val);
        }
    }

    // 2. Dispatch live 60 Hz visualizer telemetry
    sendTelemetryToWeb();
}

void Pf02AudioProcessorEditor::sendTelemetryToWeb()
{
    if (!webComponent || !webComponent->isVisible()) {
        return;
    }

    bool hasNewFrame = false;
    while (processorRef.popTelemetry(latestTelemetryFrame)) {
        hasNewFrame = true;
    }

    if (!hasNewFrame) {
        return;
    }

    // Check if signal is idle / silent
    float maxAmp = 0.0f;
    for (size_t i = 0; i < 128; ++i) {
        const float aL = std::abs(latestTelemetryFrame.waveformL[i]);
        const float aR = std::abs(latestTelemetryFrame.waveformR[i]);
        if (aL > maxAmp) maxAmp = aL;
        if (aR > maxAmp) maxAmp = aR;
    }

    const bool isSilent = (maxAmp < 0.0001f);
    if (isSilent) {
        silentFrameCounter++;
        // Throttle idle telemetry dispatch to 5 Hz (every 12 frames at 60 Hz)
        if (silentFrameCounter > 12 && (silentFrameCounter % 12 != 0)) {
            return;
        }
    } else {
        silentFrameCounter = 0;
    }

    // Construct telemetry JSON payload
    juce::DynamicObject::Ptr frameObj = new juce::DynamicObject();

    juce::Array<juce::var> waveL;
    waveL.ensureStorageAllocated(128);
    for (size_t i = 0; i < 128; ++i) waveL.add(latestTelemetryFrame.waveformL[i]);
    frameObj->setProperty("waveformL", waveL);

    juce::Array<juce::var> waveR;
    waveR.ensureStorageAllocated(128);
    for (size_t i = 0; i < 128; ++i) waveR.add(latestTelemetryFrame.waveformR[i]);
    frameObj->setProperty("waveformR", waveR);

    frameObj->setProperty("aperture", latestTelemetryFrame.aperture);
    frameObj->setProperty("colonicPressure", latestTelemetryFrame.colonicPressure);
    frameObj->setProperty("airVelocity", latestTelemetryFrame.airVelocity);
    frameObj->setProperty("bubbleIntensity", latestTelemetryFrame.bubbleIntensity);
    frameObj->setProperty("dropletTriggered", latestTelemetryFrame.dropletTriggered);
    frameObj->setProperty("zenoFlux", latestTelemetryFrame.zenoFlux);
    frameObj->setProperty("activeUnrollK", latestTelemetryFrame.activeUnrollK);
    frameObj->setProperty("lorenzX", latestTelemetryFrame.lorenzX);
    frameObj->setProperty("lorenzY", latestTelemetryFrame.lorenzY);
    frameObj->setProperty("lorenzZ", latestTelemetryFrame.lorenzZ);

    juce::Array<juce::var> modalEnergies;
    modalEnergies.ensureStorageAllocated(16);
    for (size_t i = 0; i < 16; ++i) modalEnergies.add(latestTelemetryFrame.modalEnergies[i]);
    frameObj->setProperty("modalEnergies", modalEnergies);

    juce::Array<juce::var> triadFlow;
    triadFlow.ensureStorageAllocated(16);
    for (size_t i = 0; i < 16; ++i) triadFlow.add(latestTelemetryFrame.triadEnergyFlow[i]);
    frameObj->setProperty("triadEnergyFlow", triadFlow);

    frameObj->setProperty("sphericalNormDrift", latestTelemetryFrame.sphericalNormDrift);
    frameObj->setProperty("peakLevelDb", latestTelemetryFrame.peakLevelDb);
    frameObj->setProperty("rmsLevelDb", latestTelemetryFrame.rmsLevelDb);

    webComponent->emitEventIfBrowserIsVisible("telemetryFrame", juce::var(frameObj.get()));
}
