// Build:  g++ -std=c++17 -O2 -Wall -Wextra -I../Source/Core core_test.cpp -o core_test && ./core_test
#include "Advisor.h"
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

static mc::Result run (const std::vector<float>& l, const std::vector<float>& r, double fs)
{
    mc::Analyzer a; a.prepare (fs);
    a.process (l.data(), r.data(), (int) l.size());
    return a.finish();
}

int main()
{
    const double fs = 44100.0, pi = 3.14159265358979323846;
    const int n = (int) (fs * 10);

    { // EBU TECH 3341-style check: 1 kHz stereo sine, -23 dBFS peak -> about -23 LUFS
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = (float) (std::pow (10.0, -23.0 / 20.0) * std::sin (2 * pi * 1000.0 * i / fs));
        auto r = run (s, s, fs);
        std::printf ("   LUFS = %.2f, TP = %.2f, corr = %.2f\n", r.integratedLufs, r.truePeakDb, r.correlation);
        CHECK (r.valid && std::fabs (r.integratedLufs + 23.0f) < 0.3f, "1 kHz -23 dBFS reads about -23 LUFS");
        CHECK (std::fabs (r.truePeakDb + 23.0f) < 0.3f, "true peak of -23 dBFS sine ~ -23 dBTP");
        CHECK (r.correlation > 0.99f && r.monoLossDb > -0.1f, "identical channels: corr 1, no mono loss");
    }
    { // inter-sample peak: fs/4 sine at 45 degrees -> samples +-0.7071, true peak 0 dBTP
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = (float) std::sin (pi / 2 * i + pi / 4);
        auto r = run (s, s, fs);
        std::printf ("   sample peak = %.2f, true peak = %.2f\n", r.samplePeakDb, r.truePeakDb);
        CHECK (std::fabs (r.samplePeakDb + 3.01f) < 0.1f, "sample peak -3.0 dB");
        CHECK (r.truePeakDb > -0.4f && r.truePeakDb < 0.3f, "true peak detects inter-sample 0 dBTP");
    }
    { // out-of-phase channels
        std::vector<float> a ((size_t) n), b ((size_t) n);
        for (int i = 0; i < n; ++i) { a[(size_t) i] = (float) (0.3 * std::sin (2 * pi * 440 * i / fs)); b[(size_t) i] = -a[(size_t) i]; }
        auto r = run (a, b, fs);
        CHECK (r.correlation < -0.99f && r.monoLossDb < -40.f, "polarity-inverted stereo: corr -1, huge mono loss");
        auto rep = mc::evaluate (r, false, 0);
        bool found = false;
        for (auto& f : rep.findings) if (f.sev == mc::Severity::Issue && f.cat.en == "Stereo & Phase") found = true;
        CHECK (found, "advisor flags phase problem");
    }
    { // 100 Hz sine -> all energy in Bass band
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = (float) (0.3 * std::sin (2 * pi * 100.0 * i / fs));
        auto r = run (s, s, fs);
        CHECK (r.bandDb[1] > -1.0f && r.bandDb[3] < -25.f, "100 Hz lands in the 60-150 Hz band");
    }
    { // clipped square-ish signal
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = (float) std::clamp (3.0 * std::sin (2 * pi * 200.0 * i / fs), -1.0, 1.0);
        auto r = run (s, s, fs);
        auto rep = mc::evaluate (r, false, 0);
        CHECK (r.clipEvents > 100, "clipping events counted");
        CHECK (rep.overall <= 79.f, "clipping caps overall score at 79");
    }
    { // noisy 'vocal' with loud hiss -> noise + sibilance should be flagged in vocal mode
        std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.01f);
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = nd (rng) + (float) (0.2 * std::sin (2 * pi * 300.0 * i / fs));
        auto r = run (s, s, fs);
        auto rep = mc::evaluate (r, true, 0);
        std::printf ("   vocal score = %.0f, noise floor = %.1f dBFS, findings = %zu\n", rep.overall, r.noiseFloorDb, rep.findings.size());
        CHECK (rep.valid && ! rep.findings.empty() && rep.cats.size() == 5, "vocal report has 5 categories + findings");
    }
    { // silence -> invalid
        std::vector<float> s ((size_t) n, 0.f);
        auto r = run (s, s, fs);
        CHECK (! r.valid, "digital silence is reported as invalid");
    }
    { // print a sample report
        std::mt19937 rng (7); std::normal_distribution<float> nd (0.f, 0.15f);
        std::vector<float> a ((size_t) n), b ((size_t) n);
        for (int i = 0; i < n; ++i) { a[(size_t) i] = nd (rng) * 0.6f + (float) (0.25 * std::sin (2 * pi * 90 * i / fs)); b[(size_t) i] = nd (rng) * 0.6f + (float) (0.25 * std::sin (2 * pi * 90 * i / fs)); }
        auto rep = mc::evaluate (run (a, b, fs), false, 0);
        std::printf ("\n--- sample report: score %.0f (%s) %s | profile %s\n%s\n", rep.overall, rep.grade.c_str(), rep.verdict.en.c_str(), rep.profileName.en.c_str(), rep.summary.en.c_str());
        for (auto& f : rep.findings) std::printf ("  [%d] %-16s %s\n", (int) f.sev, f.cat.en.c_str(), f.title.en.c_str());
    }
    std::printf ("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
