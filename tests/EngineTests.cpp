// Plain-executable tests for the RockGlue DSP core. No JUCE: the engine is
// header-only so the zero-latency guarantee and each node's behaviour can be
// measured without a host.

#include "DSP/RockGlueEngine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace rockglue;

namespace
{
constexpr double kFs = 48000.0;
constexpr double kPi = 3.14159265358979323846;
int failures = 0;

void check(bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf("%s  %s%s%s\n", ok ? "PASS" : "FAIL", what.c_str(),
                detail.empty() ? "" : " -- ", detail.c_str());
    if (! ok)
        ++failures;
}

void checkNear(double value, double expected, double tol, const std::string& what)
{
    char d[128];
    std::snprintf(d, sizeof(d), "got %.4f, expected %.4f +/- %.4f", value, expected, tol);
    check(std::abs(value - expected) <= tol, what, d);
}

std::vector<float> sine(double freq, int n, double amp = 0.5)
{
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        out[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(2.0 * kPi * freq * i / kFs));
    return out;
}

double rms(const std::vector<float>& x, int from = 0)
{
    double s = 0.0; int n = 0;
    for (size_t i = static_cast<size_t>(from); i < x.size(); ++i, ++n) s += double(x[i]) * x[i];
    return n > 0 ? std::sqrt(s / n) : 0.0;
}

double db(double lin) { return 20.0 * std::log10(std::max(lin, 1e-12)); }

// Steady-state gain of a per-sample stereo processor at one frequency.
template <typename Fn>
double toneGainDb(Fn&& process, double freq, int n = 48000, double amp = 0.5)
{
    auto in = sine(freq, n, amp);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        float l = in[i], r = in[i];
        process(l, r);
        out[i] = l;
    }
    return db(rms(out, n / 2)) - db(rms(in, n / 2));
}

// ---------------------------------------------------------------------------

void testZeroLatency()
{
    // An impulse into every node must come out at sample 0: nothing in the
    // chain may shift the signal in time.
    const int n = 256;
    std::vector<float> l(n, 0.0f), r(n, 0.0f);
    l[0] = r[0] = 0.5f;

    RockGlueEngine engine;
    engine.prepare(kFs);
    engine.setDrumDriveDb(12.0f);
    engine.setDrumParallelMix(1.0f);
    engine.setGrit(1.0f);
    engine.setCarvePocket(1.0f);
    engine.setGlueThresholdDb(-20.0f);

    std::vector<float> ol(n), orr(n);
    ConstStereoBlock in { l.data(), r.data() };
    engine.processFourBus(in, in, in, in, { ol.data(), orr.data() }, n);

    int first = -1;
    for (int i = 0; i < n; ++i)
        if (std::abs(ol[static_cast<size_t>(i)]) > 1e-4f) { first = i; break; }

    check(first == 0, "impulse through all four nodes + glue appears at sample 0",
          "first non-zero sample at " + std::to_string(first));
    check(RockGlueEngine::kLatencySamples == 0, "engine reports 0 samples latency");

    // Same for stereo-mix mode.
    engine.reset();
    std::vector<float> ml = l, mr = r;
    engine.processStereoMix({ ml.data(), mr.data() }, n);
    first = -1;
    for (int i = 0; i < n; ++i)
        if (std::abs(ml[static_cast<size_t>(i)]) > 1e-4f) { first = i; break; }
    check(first == 0, "stereo-mix path is also zero latency", "first non-zero sample at " + std::to_string(first));
}

void testMonoLock()
{
    // A 60 Hz tone on the Side channel only (L = -R) must be gone with Mono
    // Lock engaged and untouched with it off. A 1 kHz side tone must pass.
    auto run = [](bool lock, double freq)
    {
        BassAnchor anchor;
        anchor.prepare(kFs);
        anchor.setMonoLock(lock);
        anchor.setGrit(0.0f);
        const int n = 48000;
        auto in = sine(freq, n);
        std::vector<float> outSide(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            float l = in[i], r = -in[i];
            anchor.processSample(l, r);
            outSide[i] = 0.5f * (l - r);
        }
        return db(rms(outSide, n / 2)) - db(rms(in, n / 2));
    };

    check(run(true, 60.0) < -12.0, "mono lock removes 60 Hz side content", std::to_string(run(true, 60.0)) + " dB");
    checkNear(run(false, 60.0), 0.0, 0.1, "mono lock off leaves 60 Hz side content alone");
    checkNear(run(true, 1000.0), 0.0, 0.3, "mono lock passes 1 kHz side content");
    checkNear(run(true, 130.0), -3.0, 0.5, "side HPF is -3 dB at 130 Hz (Butterworth)");

    // Mid must never be filtered by Mono Lock.
    BassAnchor anchor;
    anchor.prepare(kFs);
    anchor.setMonoLock(true);
    anchor.setGrit(0.0f);
    checkNear(toneGainDb([&](float& l, float& r) { anchor.processSample(l, r); }, 40.0), 0.0, 0.05,
              "mono lock does not touch 40 Hz mid content");
}

void testGrit()
{
    // Grit must add odd harmonics to a 400 Hz mid tone and leave 60 Hz alone.
    BassAnchor anchor;
    anchor.prepare(kFs);
    anchor.setMonoLock(false);
    anchor.setGrit(1.0f);

    const int n = 48000;
    auto in = sine(400.0, n, 0.5);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        float l = in[i], r = in[i];
        anchor.processSample(l, r);
        out[i] = l;
    }

    auto mag = [&](double f)
    {
        double re = 0, im = 0;
        for (int i = n / 2; i < n; ++i)
        {
            re += out[static_cast<size_t>(i)] * std::cos(2 * kPi * f * i / kFs);
            im += out[static_cast<size_t>(i)] * std::sin(2 * kPi * f * i / kFs);
        }
        return std::sqrt(re * re + im * im) / (n / 2);
    };
    const double h1 = mag(400.0), h2 = mag(800.0), h3 = mag(1200.0);
    check(h3 > 0.005, "grit generates a 3rd harmonic", "h3/h1 = " + std::to_string(h3 / h1));
    check(h2 < h3 * 0.1, "grit is odd-order only (2nd harmonic negligible)", "h2/h1 = " + std::to_string(h2 / h1));

    anchor.reset();
    checkNear(toneGainDb([&](float& l, float& r) { anchor.processSample(l, r); }, 60.0), 0.0, 0.3,
              "grit leaves 60 Hz fundamentals essentially alone");
}

void testPocket()
{
    PocketEq eq;
    eq.prepare(kFs);

    eq.setDepth(1.0f);
    checkNear(toneGainDb([&](float& l, float& r) { eq.processGuitar(l, r); }, 2500.0), -2.0, 0.05,
              "guitar bell is -2.0 dB at 2.5 kHz at 100 %");
    eq.reset();
    checkNear(toneGainDb([&](float& l, float& r) { eq.processVocal(l, r); }, 2500.0), 2.0, 0.05,
              "vocal bell is +2.0 dB at 2.5 kHz at 100 %");
    eq.reset();
    checkNear(toneGainDb([&](float& l, float& r) { eq.processGuitar(l, r); }, 100.0), 0.0, 0.05,
              "guitar bell is flat far from 2.5 kHz");

    eq.setDepth(0.5f);
    eq.reset();
    checkNear(toneGainDb([&](float& l, float& r) { eq.processVocal(l, r); }, 2500.0), 1.0, 0.05,
              "carve pocket at 50 % gives +1.0 dB");

    eq.setDepth(0.0f);
    eq.reset();
    checkNear(toneGainDb([&](float& l, float& r) { eq.processGuitar(l, r); }, 2500.0), 0.0, 0.01,
              "carve pocket at 0 % is flat");
}

void testFet()
{
    FetCompressor fet;
    fet.prepare(kFs);
    fet.setMix(1.0f);

    // Below threshold with no drive the compressor should be near unity.
    fet.setDriveDb(0.0f);
    checkNear(toneGainDb([&](float& l, float& r) { fet.processSample(l, r); }, 1000.0, 24000, 0.05), 0.0, 0.5,
              "FET is roughly unity below threshold (input -26 dBFS peak, thr -18 dBFS)");

    // Driving hard must produce real gain reduction and a clamped output.
    fet.reset();
    fet.setDriveDb(30.0f);
    auto in = sine(1000.0, 24000, 0.5);
    float peak = 0.0f, gr = 0.0f;
    for (float x : in)
    {
        float l = x, r = x;
        fet.processSample(l, r);
        peak = std::max(peak, std::abs(l));
        gr = std::max(gr, fet.getGainReductionDb());
    }
    check(gr > 10.0f, "30 dB drive yields >10 dB of FET gain reduction", std::to_string(gr) + " dB");
    check(peak < 1.5f, "FET output stage clamps the peak", std::to_string(peak));

    // Parallel mix at 0 % is bit-identical to the dry signal.
    fet.reset();
    fet.setMix(0.0f);
    bool identical = true;
    for (float x : in)
    {
        float l = x, r = x;
        fet.processSample(l, r);
        if (l != x || r != x) { identical = false; break; }
    }
    check(identical, "parallel mix at 0 % passes the dry drums bit-exact");
}

void testVca()
{
    VcaBusCompressor vca;
    vca.prepare(kFs);
    vca.setMakeupDb(0.0f);
    vca.setAutoRelease(false);

    // Signal well above threshold: 2:1 -> half the overshoot comes back.
    vca.setThresholdDb(-20.0f);
    const int n = 48000;
    auto in = sine(1000.0, n, 0.5);   // -9 dBFS rms -> 11 dB over -> ~5.5 dB GR
    std::vector<float> out(in.size());
    float gr = 0.0f;
    for (size_t i = 0; i < in.size(); ++i)
    {
        float l = in[i], r = in[i];
        vca.processSample(l, r);
        out[i] = l;
        if (i > in.size() / 2) gr = std::max(gr, vca.getGainReductionDb());
    }
    const double reduction = db(rms(in, n / 2)) - db(rms(out, n / 2));
    checkNear(reduction, 5.5, 0.5, "VCA at 2:1 reduces an 11 dB overshoot by ~5.5 dB");
    checkNear(gr, 5.5, 0.5, "VCA reports the same GR to the meter");

    // Below threshold: unity.
    vca.reset();
    vca.setThresholdDb(10.0f);
    checkNear(toneGainDb([&](float& l, float& r) { vca.processSample(l, r); }, 1000.0), 0.0, 0.01,
              "VCA is unity below threshold");

    // Attack time: 30 ms to ~63 % of the final reduction.
    vca.reset();
    vca.setThresholdDb(-20.0f);
    const int at63 = static_cast<int>(0.030 * kFs);
    float grAt63 = 0.0f, grFinal = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float l = 0.5f, r = 0.5f;   // DC step so the detector sees a constant level
        vca.processSample(l, r);
        if (i == at63) grAt63 = vca.getGainReductionDb();
        grFinal = vca.getGainReductionDb();
    }
    checkNear(grAt63 / grFinal, 0.632, 0.05, "VCA attack reaches 63 % of GR after 30 ms");

    // Makeup gain is applied as a plain gain.
    vca.reset();
    vca.setThresholdDb(10.0f);
    vca.setMakeupDb(6.0f);
    checkNear(toneGainDb([&](float& l, float& r) { vca.processSample(l, r); }, 1000.0), 6.0, 0.01,
              "makeup gain of +6 dB is +6 dB");
}

void testEngineRoutingAndMeters()
{
    RockGlueEngine engine;
    engine.prepare(kFs);
    engine.setDrumDriveDb(0.0f);
    engine.setDrumParallelMix(0.0f);
    engine.setGrit(0.0f);
    engine.setMonoLock(false);
    engine.setCarvePocket(0.0f);
    engine.setGlueThresholdDb(10.0f);
    engine.setGlueMakeupDb(0.0f);

    // With everything neutral, the four buses just sum.
    const int n = 512;
    std::vector<float> a(n, 0.1f), b(n, 0.2f), c(n, 0.3f), d(n, 0.4f), ol(n), orr(n);
    engine.processFourBus({ a.data(), a.data() }, { b.data(), b.data() }, { c.data(), c.data() }, { d.data(), d.data() },
                          { ol.data(), orr.data() }, n);
    checkNear(ol[n - 1], 1.0, 1e-4, "neutral four-bus path sums the buses");

    // A missing bus is silent, not garbage.
    engine.processFourBus({ a.data(), a.data() }, {}, {}, {}, { ol.data(), orr.data() }, n);
    checkNear(ol[n - 1], 0.1, 1e-4, "disconnected buses contribute nothing");

    // Width meter: mono -> 0, L-only -> ~0.71 (side == mid), anti-phase -> 1.
    std::vector<float> zero(n, 0.0f);
    engine.processFourBus({ a.data(), a.data() }, {}, {}, {}, { ol.data(), orr.data() }, n);
    checkNear(engine.getMeterFrame().width, 0.0, 1e-3, "width meter reads 0 for mono");
    engine.processFourBus({ a.data(), zero.data() }, {}, {}, {}, { ol.data(), orr.data() }, n);
    checkNear(engine.getMeterFrame().width, 0.7071, 1e-3, "width meter reads 0.71 for hard-left");
    std::vector<float> neg(n, -0.1f);
    engine.processFourBus({ a.data(), neg.data() }, {}, {}, {}, { ol.data(), orr.data() }, n);
    checkNear(engine.getMeterFrame().correlation, -1.0, 1e-3, "correlation meter reads -1 for anti-phase");

    // Output is always finite, even when slammed.
    engine.setDrumDriveDb(30.0f);
    engine.setDrumParallelMix(1.0f);
    engine.setGrit(1.0f);
    engine.setGlueThresholdDb(-20.0f);
    engine.setGlueMakeupDb(12.0f);
    std::vector<float> loud(n, 0.99f);
    engine.processFourBus({ loud.data(), loud.data() }, { loud.data(), neg.data() }, { loud.data(), loud.data() },
                          { loud.data(), loud.data() }, { ol.data(), orr.data() }, n);
    bool finite = true;
    for (int i = 0; i < n; ++i)
        finite = finite && std::isfinite(ol[static_cast<size_t>(i)]) && std::isfinite(orr[static_cast<size_t>(i)]);
    check(finite, "output stays finite under full drive on every bus");
}
} // namespace

int main()
{
    testZeroLatency();
    testMonoLock();
    testGrit();
    testPocket();
    testFet();
    testVca();
    testEngineRoutingAndMeters();

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
