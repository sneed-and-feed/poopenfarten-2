#include "source/dsp/DspMath.h"
#include "source/dsp/DarkSnailMath.h"
#include "source/dsp/MucosalExciter.h"
#include "source/dsp/AeroFluidEngine.h"
#include "source/dsp/ModalResonatorMatrix.h"
#include "source/dsp/LorenzAttractor.h"
#include "source/dsp/MasteringChain.h"
#include "source/dsp/SpectralModalExtractor.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>
#include <atomic>
#include <cstdlib>

// ============================================================================
// Zero-Allocation Tracking Guard
// ============================================================================
static std::atomic<bool> gTrackAllocations { false };
static std::atomic<size_t> gAllocationCount { 0 };

void* operator new(size_t size) {
    if (gTrackAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    std::free(ptr);
}

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            std::exit(1); \
        } \
    } while (0)

using namespace pf02::dsp;

// ============================================================================
// Test 1: DspMath Primitives
// ============================================================================
void testDspMath() {
    std::cout << "[TEST] Running testDspMath()...\n";

    // 1. flushDenormal
    float subnormal = 1.0e-40f;
    TEST_ASSERT(flushDenormal(subnormal) == 0.0f, "subnormal should flush to zero");
    TEST_ASSERT(flushDenormal(1.0f) == 1.0f, "normal float should be preserved");
    TEST_ASSERT(flushDenormal(-0.5f) == -0.5f, "negative float should be preserved");

    // 2. safeClamp
    float nanVal = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT(safeClamp(nanVal, 0.0f, 1.0f, 0.5f) == 0.5f, "NaN should yield fallback");
    TEST_ASSERT(safeClamp(1.5f, 0.0f, 1.0f, 0.5f) == 1.0f, "clamp to max");

    // 3. FastSinTable
    for (int deg = 0; deg < 360; deg += 5) {
        float rad = static_cast<float>(deg) * (kPi / 180.0f);
        float ref = std::sin(rad);
        float approx = FastSinTable::sin(rad);
        TEST_ASSERT(std::abs(ref - approx) < 2.0e-5f, "FastSinTable precision error < 2e-5");
    }

    // 4. Householder Scattering Matrix energy conservation
    std::array<float, 16> testModes = {
        0.5f, -0.3f, 0.8f, -0.1f, 0.4f, 0.2f, -0.7f, 0.9f,
        -0.4f, 0.6f, -0.2f, 0.3f, 0.1f, -0.5f, 0.7f, -0.6f
    };
    float normBeforeSq = 0.0f;
    for (float x : testModes) normBeforeSq += x * x;

    applyHouseholderScattering16(testModes, 1.0f);

    float normAfterSq = 0.0f;
    for (float x : testModes) normAfterSq += x * x;

    TEST_ASSERT(std::abs(normBeforeSq - normAfterSq) < 1.0e-5f, "Householder scattering preserves L2 norm");
    std::cout << "  -> Householder norm diff = " << std::abs(normBeforeSq - normAfterSq) << " (PASSED)\n";
}

// ============================================================================
// Test 2: DarkSnailMath Invariants
// ============================================================================
void testDarkSnailMath() {
    std::cout << "[TEST] Running testDarkSnailMath()...\n";

    // 1. Skew-Symmetric Bilinear Triad Operator
    SkewSymmetricTriadOperator<16, 16> triadOp;
    std::array<float, 16> u = { 0.2f, 0.4f, -0.1f, 0.5f, 0.3f, -0.6f, 0.1f, 0.8f,
                                -0.2f, 0.3f, 0.4f, -0.5f, 0.2f, -0.1f, 0.7f, -0.3f };
    std::array<float, 16> v = { 0.5f, -0.3f, 0.2f, 0.7f, -0.4f, 0.1f, 0.6f, -0.5f,
                                0.3f, -0.2f, 0.8f, -0.1f, 0.4f, 0.2f, -0.6f, 0.5f };

    // Pointwise orthogonality: <v, B_W(u, v)> == 0
    float orthError = triadOp.verifyPointwiseOrthogonality(u, v);
    TEST_ASSERT(orthError < 1.0e-6f, "Triad operator pointwise orthogonality < 1e-6");
    std::cout << "  -> Triad pointwise orthogonality error = " << orthError << " (PASSED)\n";

    // Skew element antisymmetry: S(W, i, j, k) == -S(W, i, k, j) and S(W, i, j, j) == 0
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            TEST_ASSERT(std::abs(triadOp.skewElement(i, j, j)) < 1.0e-8f, "Diagonal skew element vanishes");
            for (size_t k = 0; k < 4; ++k) {
                float s_jk = triadOp.skewElement(i, j, k);
                float s_kj = triadOp.skewElement(i, k, j);
                TEST_ASSERT(std::abs(s_jk + s_kj) < 1.0e-8f, "Antisymmetry S(j, k) == -S(k, j)");
            }
        }
    }

    // 2. so(16) Lie Algebra Representation
    So16LieAlgebra::Matrix16 rawB {};
    for (size_t i = 0; i < 16; ++i) {
        for (size_t j = 0; j < 16; ++j) {
            rawB[i][j] = static_cast<float>(i * 3 + j * 7) * 0.05f;
        }
    }
    So16LieAlgebra::Matrix16 lieA = So16LieAlgebra::projectToSo16(rawB);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT(lieA[i][i] == 0.0f, "so(16) diagonal must be zero");
        for (size_t j = 0; j < 16; ++j) {
            TEST_ASSERT(std::abs(lieA[i][j] + lieA[j][i]) < 1.0e-7f, "so(16) skew-symmetric A^T = -A");
        }
    }

    // 3. SlowFastTriadCascade Multi-Step Unroll & Spherical Norm Retraction
    SlowFastTriadCascade cascade;
    cascade.setCascadeGain(1.25f);

    for (int K : { 1, 4, 8, 16 }) {
        cascade.setUnrollSteps(K);
        std::array<float, 16> vResult = cascade.processCascade(u, v, triadOp, K);
        float radialDrift = cascade.getLastRadialDrift();
        float orthErr = cascade.getLastTangentOrthError();

        TEST_ASSERT(radialDrift < 1.0e-6f, "Spherical norm retraction radial drift < 1e-6");
        TEST_ASSERT(orthErr < 1.0e-5f, "Tangent step orthogonality error < 1e-5");
        std::cout << "  -> Cascade K=" << std::setw(2) << K 
                  << " Radial Drift=" << radialDrift 
                  << " Tangent Orth=" << orthErr << " (PASSED)\n";
    }

    // 4. Beltrami Triad Flux Detector & Outlier Shielding
    BeltramiTriadFluxDetector fluxDetector;
    for (int i = 0; i < 20; ++i) {
        std::array<float, 16> uC = u;
        std::array<float, 16> uF = v;
        uF[0] += static_cast<float>(i) * 0.1f;
        float zenoFlux = fluxDetector.computeFlux(uC, uF);
        TEST_ASSERT(zenoFlux >= 0.0f && zenoFlux <= 1.0f, "Zeno flux in [0, 1]");
    }
    std::cout << "  -> Zeno flux detector output = " << fluxDetector.getZenoFlux() << " (PASSED)\n";

    // 5. Zeno-Amygdala Adaptive Scheduler
    ZenoAmygdalaScheduler scheduler;
    int kLaminar = scheduler.updateSchedule(0.1f, 0.1f);
    TEST_ASSERT(kLaminar == 1, "Low turbulence selects K=1");
    int kModerate = scheduler.updateSchedule(0.5f, 0.5f);
    TEST_ASSERT(kModerate == 4, "Moderate turbulence selects K=4");
    int kHigh = scheduler.updateSchedule(0.7f, 0.7f);
    TEST_ASSERT(kHigh == 8, "High turbulence selects K=8");
    int kShock = scheduler.updateSchedule(0.9f, 0.9f);
    TEST_ASSERT(kShock == 16, "Shock transition selects K=16");
    std::cout << "  -> Zeno-Amygdala transitions K: 1 -> 4 -> 8 -> 16 (PASSED)\n";

    // 6. Optimal Transport Wasserstein-1
    std::array<float, 16> histP { 0.1f, 0.2f, 0.3f, 0.4f };
    std::array<float, 16> histQ { 0.4f, 0.3f, 0.2f, 0.1f };
    float w1 = OptimalTransportMorpher::computeWasserstein1(histP, histQ);
    TEST_ASSERT(w1 > 0.0f, "Wasserstein-1 distance > 0");
    std::cout << "  -> Wasserstein-1 distance = " << w1 << " (PASSED)\n";
}

// ============================================================================
// Test 3: MucosalExciter Mechanics
// ============================================================================
void testMucosalExciter() {
    std::cout << "[TEST] Running testMucosalExciter()...\n";

    MucosalExciter exciter;
    exciter.prepare(48000.0);

    MucosalExciterParams params;
    params.colonicPressure = 0.0f; // Zero pressure: valve should be closed/relaxed

    float airVel = 0.0f;
    float aperture = 0.0f;
    float outSample = exciter.processSample(params, airVel, aperture);

    TEST_ASSERT(airVel == 0.0f, "Zero drive pressure gives zero air velocity");

    // Apply high pressure to induce breakaway
    params.colonicPressure = 0.85f;
    params.frequencyHz = 65.0f;

    bool breakawayFired = false;
    for (int i = 0; i < 2000; ++i) {
        outSample = exciter.processSample(params, airVel, aperture);
        if (exciter.wasOpeningTransient()) {
            breakawayFired = true;
            TEST_ASSERT(exciter.getOpeningTransientAmp() > 1.0f, "Breakaway transient velocity surge > 1.0 m/s");
        }
    }
    TEST_ASSERT(breakawayFired, "High colonic pressure must trigger Stribeck seal breakaway");
    std::cout << "  -> Stribeck seal breakaway triggered successfully (PASSED)\n";
}

// ============================================================================
// Test 4: AeroFluidEngine & Minnaert Bubbles
// ============================================================================
void testAeroFluidEngine() {
    std::cout << "[TEST] Running testAeroFluidEngine()...\n";

    AeroFluidEngine fluid;
    fluid.prepare(48000.0);

    AeroFluidParams params;
    params.minnaertBubbleDensity = 50.0f;
    params.multiphaseMoisture = 0.60f;
    params.poissonDropletRate = 20.0f;

    bool hadDroplets = false;
    for (int i = 0; i < 5000; ++i) {
        float sample = fluid.processSample(20.0f, 0.001f, 0.4f, params);
        TEST_ASSERT(std::isfinite(sample), "Fluid sample must be finite");
        if (fluid.wasDropletTriggered()) {
            hadDroplets = true;
        }
    }
    TEST_ASSERT(hadDroplets, "Poisson droplet point process must fire droplets");
    TEST_ASSERT(fluid.getBubbleActivity() > 0.0f, "Bubble bank must have non-zero activity");
    std::cout << "  -> Minnaert bubbles & Poisson droplets active (PASSED)\n";
}

// ============================================================================
// Test 5: ModalResonatorMatrix & Manifolds
// ============================================================================
void testModalResonatorMatrix() {
    std::cout << "[TEST] Running testModalResonatorMatrix()...\n";

    ModalResonatorMatrix matrix;
    matrix.prepare(48000.0);

    ModalMatrixParams params;
    params.fundamentalHz = 60.0f;
    params.manifold = GeometricManifold::GlottalColonic;
    params.damping = DampingProfile::MucosalTissue;
    params.triadCouplingGain = 0.85f;
    params.householderCoupling = 0.50f;

    float outL = 0.0f;
    float outR = 0.0f;

    // Impulse excitation
    matrix.processSample(1.0f, params, outL, outR);
    TEST_ASSERT(std::isfinite(outL) && std::isfinite(outR), "Resonator outputs must be finite");

    for (int i = 0; i < 1000; ++i) {
        matrix.processSample(0.0f, params, outL, outR);
        TEST_ASSERT(std::isfinite(outL) && std::isfinite(outR), "Decaying outputs must be finite");
    }

    const auto& energies = matrix.getModalEnergies();
    float totalEnergy = 0.0f;
    for (float e : energies) totalEnergy += e;
    TEST_ASSERT(totalEnergy > 0.0f, "Modal resonators must retain acoustic energy");
    std::cout << "  -> 16-pole matrix total energy = " << totalEnergy << " (PASSED)\n";
}

// ============================================================================
// Test 6: LorenzAttractor 3D RK4
// ============================================================================
void testLorenzAttractor() {
    std::cout << "[TEST] Running testLorenzAttractor()...\n";

    LorenzAttractor lorenz;
    lorenz.prepare(48000.0);
    lorenz.setRateHz(1.0f);
    lorenz.setDepth(0.50f);

    float initialX = lorenz.getX();
    for (int i = 0; i < 1000; ++i) {
        lorenz.step();
        TEST_ASSERT(std::isfinite(lorenz.getX()), "Lorenz X must be finite");
        TEST_ASSERT(std::isfinite(lorenz.getY()), "Lorenz Y must be finite");
        TEST_ASSERT(std::isfinite(lorenz.getZ()), "Lorenz Z must be finite");
    }
    TEST_ASSERT(lorenz.getX() != initialX, "Lorenz state must evolve dynamically");

    const auto& mults = lorenz.getFrequencyMultipliers();
    TEST_ASSERT(mults[0] > 0.5f && mults[0] < 2.0f, "Detune multiplier within safe musical range");
    std::cout << "  -> Lorenz trajectory X=" << lorenz.getX() 
              << " Y=" << lorenz.getY() << " Z=" << lorenz.getZ() << " (PASSED)\n";
}

// ============================================================================
// Test 7: MasteringChain & Mono-Sum Immunity
// ============================================================================
void testMasteringChain() {
    std::cout << "[TEST] Running testMasteringChain()...\n";

    MasteringChain master;
    master.prepare(48000.0);

    MasteringChainParams params;
    params.dimensionDEnable = true;
    params.dimensionDMix = 100.0f; // 100% wet to strictly test mono sum immunity
    params.colonicSaturationDrive = 50.0f;

    // Test across a frequency sweep that mono sum (|WetL + WetR|) NEVER cancels to zero
    bool passedAllFreqs = true;
    for (float freq = 50.0f; freq <= 5000.0f; freq += 150.0f) {
        master.reset();
        float monoSumEnergy = 0.0f;
        for (int i = 0; i < 500; ++i) {
            float inSample = std::sin(static_cast<float>(i) * (kTwoPi * freq / 48000.0f));
            float outL = 0.0f, outR = 0.0f;
            master.process(inSample, inSample, params, outL, outR);

            if (i > 300) { // Steady state
                float monoSum = 0.5f * (outL + outR);
                monoSumEnergy += monoSum * monoSum;
            }
        }
        if (monoSumEnergy < 1.0e-5f) {
            passedAllFreqs = false;
            std::cerr << "Mono cancellation failure at freq " << freq << " Hz!\n";
        }
    }
    TEST_ASSERT(passedAllFreqs, "Roland Dimension D chorus must have strictly non-canceling mono sum");
    std::cout << "  -> Roland Dimension D mono-sum phase cancellation immunity verified (PASSED)\n";

    // Test Hermite BoundedSaturator & true-peak limiter with extreme +40 dBFS input
    for (int i = 0; i < 200; ++i) {
        float extremeInput = 100.0f; // +40 dBFS overload
        float outL = 0.0f, outR = 0.0f;
        master.process(extremeInput, extremeInput, params, outL, outR);
        TEST_ASSERT(std::abs(outL) <= MasteringChain::kPeakCeiling, "Peak output strictly <= 1.05");
        TEST_ASSERT(std::abs(outR) <= MasteringChain::kPeakCeiling, "Peak output strictly <= 1.05");
    }
    std::cout << "  -> BoundedSaturator & true-peak brickwall containment verified (PASSED)\n";
}

// ============================================================================
// Test 8: SpectralModalExtractor
// ============================================================================
void testSpectralModalExtractor() {
    std::cout << "[TEST] Running testSpectralModalExtractor()...\n";

    SpectralModalExtractor extractor;
    extractor.prepare(48000.0);

    // Generate a synthetic test burst for Chladni Ring manifold at 80 Hz
    std::vector<float> testBurst(4096, 0.0f);
    extractor.generateTestBurst(testBurst.data(), testBurst.size(), 80.0f, GeometricManifold::ChladniRing);

    ExtractedModalParameters result = extractor.analyzeBuffer(testBurst.data(), testBurst.size());
    TEST_ASSERT(result.bestManifold == GeometricManifold::ChladniRing, "Must correctly identify ChladniRing manifold");
    TEST_ASSERT(std::abs(result.fundamentalHz - 80.0f) < 5.0f, "Fundamental frequency within 5 Hz");
    TEST_ASSERT(result.manifoldConfidence > 0.50f, "Confidence > 50%");

    std::cout << "  -> Extracted f0 = " << result.fundamentalHz 
              << " Hz, Manifold = " << static_cast<int>(result.bestManifold) 
              << " Confidence = " << result.manifoldConfidence * 100.0f << "% (PASSED)\n";
}

// ============================================================================
// Test 9: Zero-Heap Allocation Enforcement & Adversarial Stress
// ============================================================================
void testZeroAllocationAndStress() {
    std::cout << "[TEST] Running testZeroAllocationAndStress()...\n";

    // Initialize all components
    MucosalExciter exciter;
    AeroFluidEngine fluid;
    ModalResonatorMatrix matrix;
    LorenzAttractor lorenz;
    MasteringChain master;

    const double sampleRate = 48000.0;
    exciter.prepare(sampleRate);
    fluid.prepare(sampleRate);
    matrix.prepare(sampleRate);
    lorenz.prepare(sampleRate);
    master.prepare(sampleRate);

    MucosalExciterParams exciterParams;
    exciterParams.colonicPressure = 0.75f;
    exciterParams.frequencyHz = 75.0f;

    AeroFluidParams fluidParams;
    ModalMatrixParams matrixParams;
    matrixParams.fundamentalHz = 75.0f;

    MasteringChainParams masterParams;

    // Pre-run 100 samples
    for (int i = 0; i < 100; ++i) {
        float airVel = 0.0f, aperture = 0.0f;
        float exWave = exciter.processSample(exciterParams, airVel, aperture);
        float flWave = fluid.processSample(airVel, aperture, 0.35f, fluidParams);
        lorenz.step();
        matrix.setLorenzModulation(lorenz.getFrequencyMultipliers(), lorenz.getQSpreadFactor(), lorenz.getSpatialPanOffset());
        float resL = 0.0f, resR = 0.0f;
        matrix.processSample(exWave + flWave, matrixParams, resL, resR);
        float outL = 0.0f, outR = 0.0f;
        master.process(resL, resR, masterParams, outL, outR);
    }

    // ENABLE ZERO-ALLOCATION TRACKING
    gAllocationCount.store(0);
    gTrackAllocations.store(true);

    // Process 20,000 continuous samples on real-time audio thread
    for (int i = 0; i < 20000; ++i) {
        float airVel = 0.0f, aperture = 0.0f;
        float exWave = exciter.processSample(exciterParams, airVel, aperture);
        float flWave = fluid.processSample(airVel, aperture, 0.45f, fluidParams);

        lorenz.step();
        matrix.setLorenzModulation(lorenz.getFrequencyMultipliers(), lorenz.getQSpreadFactor(), lorenz.getSpatialPanOffset());

        float resL = 0.0f, resR = 0.0f;
        matrix.processSample(exWave + flWave, matrixParams, resL, resR);

        float outL = 0.0f, outR = 0.0f;
        master.process(resL, resR, masterParams, outL, outR);

        TEST_ASSERT(std::isfinite(outL) && std::isfinite(outR), "Output must be finite");
        TEST_ASSERT(std::abs(outL) <= MasteringChain::kPeakCeiling, "Peak output bounded");
        TEST_ASSERT(std::abs(outR) <= MasteringChain::kPeakCeiling, "Peak output bounded");
    }

    gTrackAllocations.store(false);

    TEST_ASSERT(gAllocationCount.load() == 0, "ZERO audio-thread heap allocations violated!");
    std::cout << "  -> Zero heap allocations confirmed (allocations = " << gAllocationCount.load() << ") (PASSED)\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << "PF-02 · Core DSP Physical Modeling Verification Suite\n";
    std::cout << "=======================================================\n";

    testDspMath();
    testDarkSnailMath();
    testMucosalExciter();
    testAeroFluidEngine();
    testModalResonatorMatrix();
    testLorenzAttractor();
    testMasteringChain();
    testSpectralModalExtractor();
    testZeroAllocationAndStress();

    std::cout << "\n>>> ALL 9 DSP TEST SUITES PASSED CLEANLY (100% SUCCESS) <<<\n";
    return 0;
}
