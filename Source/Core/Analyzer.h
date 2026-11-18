#pragma once
// Mix Check AI - analysis engine.
// Pure C++17, no JUCE dependency, so it can be unit-tested on its own.
// Measures: ITU-R BS.1770 loudness (integrated / LRA), true peak, clipping, DC,
// stereo correlation + mono compatibility, 7-band tonal balance, sibilance, noise floor.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace mc
{
constexpr int kBands = 7;
// Sub | Bass | Low-Mid | Mid | Presence | Brilliance | Air
inline constexpr float kBandEdges[kBands + 1] = { 20.f, 60.f, 150.f, 400.f, 2000.f, 5000.f, 10000.f, 20000.f };

struct Result
{
    bool valid = false;
    bool mono = false;
    bool lraValid = false;
    double sampleRate = 0.0;
    double durationSec = 0.0;

    float integratedLufs = -70.f;
    float loudnessRange = 0.f;      // LU
    float maxShortTermLufs = -70.f;
    float truePeakDb = -120.f;      // dBTP
    float samplePeakDb = -120.f;
    float rmsDb = -120.f;
    int   clipEvents = 0;           // runs of >= 3 consecutive full-scale samples
    float dcOffset = 0.f;

    float correlation = 1.f;        // -1..+1
    float sideShare = 0.f;          // 0..1 (side energy / total)
    float monoLossDb = 0.f;         // <= 0, level change when summed to mono

    float noiseFloorDb = -120.f;    // 10th percentile of 100 ms block levels (gated)
    float silenceRatio = 0.f;
    float sibilanceDb = -60.f;      // 97th percentile of per-frame 5-9 kHz vs 300-3k ratio

    std::array<float, kBands> bandDb {};   // share of total power per band, dB (<= 0)
};

class Analyzer
{
public:
    void prepare (double sampleRate) { fs = sampleRate > 8000.0 ? sampleRate : 44100.0; reset(); }
    void setMono (bool m) { mono = m; }
    void reset();
    void process (const float* l, const float* r, int n);
    Result finish() const;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double run (double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };

    static constexpr int kFft = 4096;
    static constexpr int kTpTaps = 12; // taps per phase (4 phases)

    double fs = 44100.0;
    bool mono = false;
    int64_t total = 0;

    // K-weighting
    Biquad shelf[2], hp[2];

    // 100 ms blocks
    int subLen = 4410, subCount = 0;
    double subPow = 0.0, subRaw = 0.0;
    std::vector<double> subPows;
    std::vector<float> subRmsDb;

    // true peak
    double tpCoef[4][kTpTaps] {};
    float tpBuf[2][16] {};
    int tpPos[2] { 0, 0 };
    double tpMax = 0.0, spMax = 0.0;

    // clipping / dc / stereo
    int run[2] { 0, 0 };
    int clipEvents = 0;
    double dcSum = 0, sumLL = 0, sumRR = 0, sumLR = 0, sumMM = 0, sumSS = 0;

    // spectrum
    std::vector<float> fftL, fftR;
    int fftFill = 0;
    std::vector<double> win, specSum;
    std::vector<std::complex<double>> tw, scratch;
    int64_t frames = 0;
    int sibLo = 0, sibHi = 0, refLo = 0, refHi = 0;
    std::vector<float> sibVec;
    std::vector<int> bandLo, bandHi;

    void endSubBlock();
    void doFrame();
    void fft (std::vector<std::complex<double>>& a) const;
    double tpPush (int ch, double x);

    static float percentile (std::vector<float> v, float p)
    {
        if (v.empty()) return 0.f;
        const size_t k = (size_t) std::floor (p * (float) (v.size() - 1) + 0.5f);
        std::nth_element (v.begin(), v.begin() + (long) k, v.end());
        return v[k];
    }
};

// ----------------------------------------------------------------------------
inline void Analyzer::reset()
{
    const double pi = 3.14159265358979323846;

    // Stage 1: high-shelf (+4 dB @ ~1.68 kHz), stage 2: high-pass (~38 Hz)  (BS.1770)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& s : shelf)
        {
            s = Biquad();
            s.b0 = (Vh + Vb * K / Q + K * K) / a0;
            s.b1 = 2.0 * (K * K - Vh) / a0;
            s.b2 = (Vh - Vb * K / Q + K * K) / a0;
            s.a1 = 2.0 * (K * K - 1.0) / a0;
            s.a2 = (1.0 - K / Q + K * K) / a0;
        }
        const double f1 = 38.13547087602444, Q1 = 0.5003270373238773;
        const double K1 = std::tan (pi * f1 / fs);
        const double a01 = 1.0 + K1 / Q1 + K1 * K1;
        for (auto& h : hp)
        {
            h = Biquad();
            h.b0 = 1.0; h.b1 = -2.0; h.b2 = 1.0;
            h.a1 = 2.0 * (K1 * K1 - 1.0) / a01;
            h.a2 = (1.0 - K1 / Q1 + K1 * K1) / a01;
        }
    }

    total = 0; subCount = 0; subPow = subRaw = 0.0;
    subLen = std::max (1, (int) std::lround (fs * 0.1));
    subPows.clear(); subRmsDb.clear();

    // 4x oversampling windowed-sinc, 48 taps (polyphase 4 x 12)
    {
        double h[48], sums[4] = { 0, 0, 0, 0 };
        for (int n = 0; n < 48; ++n)
        {
            const double x = ((double) n - 23.5) / 4.0;
            const double s = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double w = 0.42 - 0.5 * std::cos (2 * pi * n / 47.0) + 0.08 * std::cos (4 * pi * n / 47.0);
            h[n] = s * w;
            sums[n % 4] += h[n];
        }
        for (int p = 0; p < 4; ++p)
            for (int k = 0; k < kTpTaps; ++k)
                tpCoef[p][k] = h[p + 4 * k] / sums[p];
    }
    for (auto& b : tpBuf) for (auto& v : b) v = 0.f;
    tpPos[0] = tpPos[1] = 0; tpMax = spMax = 0.0;

    run[0] = run[1] = 0; clipEvents = 0;
    dcSum = sumLL = sumRR = sumLR = sumMM = sumSS = 0.0;

    fftL.assign (kFft, 0.f); fftR.assign (kFft, 0.f); fftFill = 0; frames = 0;
    win.resize (kFft); specSum.assign (kFft / 2 + 1, 0.0);
    for (int i = 0; i < kFft; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2 * pi * i / kFft);
    tw.resize (kFft / 2);
    for (int k = 0; k < kFft / 2; ++k) tw[(size_t) k] = std::polar (1.0, -2.0 * pi * k / kFft);
    scratch.assign (kFft, {});
    sibVec.clear();

    auto bin = [this] (double f) { return std::clamp ((int) std::lround (f * kFft / fs), 1, kFft / 2); };
    sibLo = bin (5000); sibHi = bin (9000); refLo = bin (300); refHi = bin (3000);
    bandLo.assign (kBands, 0); bandHi.assign (kBands, 0);
    for (int b = 0; b < kBands; ++b)
    {
        bandLo[(size_t) b] = bin (kBandEdges[b]);
        bandHi[(size_t) b] = std::min (bin (kBandEdges[b + 1]) - (b == kBands - 1 ? 0 : 1), kFft / 2);
    }
}

inline double Analyzer::tpPush (int ch, double x)
{
    tpPos[ch] = (tpPos[ch] + 1) & 15;
    tpBuf[ch][tpPos[ch]] = (float) x;
    double best = std::abs (x);
    for (int p = 0; p < 4; ++p)
    {
        double acc = 0.0;
        for (int k = 0; k < kTpTaps; ++k)
            acc += (double) tpBuf[ch][(tpPos[ch] - k) & 15] * tpCoef[p][k];
        best = std::max (best, std::abs (acc));
    }
    return best;
}

inline void Analyzer::process (const float* l, const float* r, int n)
{
    if (r == nullptr) r = l;
    for (int i = 0; i < n; ++i)
    {
        const double L = l[i], R = mono ? l[i] : r[i];
        const double aL = std::abs (L), aR = std::abs (R);
        spMax = std::max (spMax, std::max (aL, aR));

        run[0] = aL >= 0.999 ? run[0] + 1 : 0;
        if (run[0] == 3) ++clipEvents;
        if (! mono)
        {
            run[1] = aR >= 0.999 ? run[1] + 1 : 0;
            if (run[1] == 3) ++clipEvents;
        }

        tpMax = std::max (tpMax, tpPush (0, L));
        tpMax = std::max (tpMax, mono ? 0.0 : tpPush (1, R));

        const double M = 0.5 * (L + R), S = 0.5 * (L - R);
        dcSum += M; sumLL += L * L; sumRR += R * R; sumLR += L * R; sumMM += M * M; sumSS += S * S;

        const double kl = hp[0].run (shelf[0].run (L));
        const double kr = hp[1].run (shelf[1].run (R));
        subPow += kl * kl + kr * kr;
        subRaw += 0.5 * (L * L + R * R);
        if (++subCount >= subLen) endSubBlock();

        fftL[(size_t) fftFill] = (float) L;
        fftR[(size_t) fftFill++] = (float) R;
        if (fftFill == kFft)
        {
            doFrame();
            std::copy (fftL.begin() + kFft / 2, fftL.end(), fftL.begin());
            std::copy (fftR.begin() + kFft / 2, fftR.end(), fftR.begin());
            fftFill = kFft / 2;
        }
        ++total;
    }
}

inline void Analyzer::endSubBlock()
{
    subPows.push_back (subPow / subCount);
    subRmsDb.push_back ((float) (10.0 * std::log10 (subRaw / subCount + 1e-12)));
    subPow = subRaw = 0.0; subCount = 0;
}

inline void Analyzer::fft (std::vector<std::complex<double>>& a) const
{
    const int n = kFft;
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const int step = n / len, half = len / 2;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < half; ++k)
            {
                const auto w = tw[(size_t) (k * step)];
                const auto u = a[(size_t) (i + k)];
                const auto v = a[(size_t) (i + k + half)] * w;
                a[(size_t) (i + k)] = u + v;
                a[(size_t) (i + k + half)] = u - v;
            }
    }
}

inline void Analyzer::doFrame()
{
    double e = 0.0;
    for (int i = 0; i < kFft; ++i) e += (double) fftL[(size_t) i] * fftL[(size_t) i] + (double) fftR[(size_t) i] * fftR[(size_t) i];
    if (e / (2.0 * kFft) < 1e-9) return; // digital silence / below -90 dBFS

    // One complex FFT carries both channels: z = L + iR
    for (int i = 0; i < kFft; ++i)
        scratch[(size_t) i] = { (double) fftL[(size_t) i] * win[(size_t) i], (double) fftR[(size_t) i] * win[(size_t) i] };
    fft (scratch);

    double sibNum = 0, sibDen = 0;
    for (int k = 1; k <= kFft / 2; ++k)
    {
        const auto zk = scratch[(size_t) k];
        const auto zn = std::conj (scratch[(size_t) ((kFft - k) % kFft)]);
        const auto lk = 0.5 * (zk + zn);
        const auto rk = std::complex<double> (0.0, -0.5) * (zk - zn);
        const double p = 0.5 * (std::norm (lk) + std::norm (rk));
        specSum[(size_t) k] += p;
        if (k >= sibLo && k <= sibHi) sibNum += p;
        if (k >= refLo && k <= refHi) sibDen += p;
    }
    ++frames;
    if (sibNum > 0 && sibDen > 0) sibVec.push_back ((float) (10.0 * std::log10 (sibNum / sibDen)));
}

inline Result Analyzer::finish() const
{
    Result r;
    r.sampleRate = fs; r.mono = mono;
    r.durationSec = (double) total / fs;
    if (total < (int64_t) (fs * 0.5) || subPows.size() < 4) return r;

    auto lufs = [] (double p) { return p > 1e-12 ? -0.691 + 10.0 * std::log10 (p) : -120.0; };
    const double absT = std::pow (10.0, (-70.0 + 0.691) / 10.0);

    // momentary (400 ms, 75% overlap) and short-term (3 s) power series
    std::vector<double> mom, st;
    for (size_t i = 3; i < subPows.size(); ++i)
        mom.push_back ((subPows[i] + subPows[i - 1] + subPows[i - 2] + subPows[i - 3]) / 4.0);
    for (size_t i = 29; i < subPows.size(); ++i)
    {
        double s = 0; for (size_t k = 0; k < 30; ++k) s += subPows[i - k];
        st.push_back (s / 30.0);
    }

    auto gatedMean = [&] (const std::vector<double>& blocks, double relDb, double& out)
    {
        double sum = 0; int c = 0;
        for (double b : blocks) if (b > absT) { sum += b; ++c; }
        if (c == 0) return false;
        const double thr = (sum / c) * std::pow (10.0, relDb / 10.0);
        double s2 = 0; int c2 = 0;
        for (double b : blocks) if (b > absT && b > thr) { s2 += b; ++c2; }
        if (c2 == 0) return false;
        out = s2 / c2; return true;
    };

    double meanP = 0;
    if (! gatedMean (mom, -10.0, meanP)) return r; // silence
    r.integratedLufs = (float) lufs (meanP);

    if (! st.empty())
    {
        double m2 = 0;
        for (double b : st) r.maxShortTermLufs = std::max (r.maxShortTermLufs, (float) lufs (b));
        if (gatedMean (st, -20.0, m2))
        {
            // collect short-term values above the -20 LU relative gate
            double sum = 0; int c = 0;
            for (double b : st) if (b > absT) { sum += b; ++c; }
            const double rel = (sum / c) * std::pow (10.0, -2.0);
            std::vector<float> v;
            for (double b : st) if (b > absT && b > rel) v.push_back ((float) lufs (b));
            if (v.size() >= 2)
            {
                r.loudnessRange = std::max (0.f, percentile (v, 0.95f) - percentile (v, 0.10f));
                r.lraValid = true;
            }
        }
    }

    r.truePeakDb = (float) (tpMax > 1e-9 ? 20.0 * std::log10 (tpMax) : -120.0);
    r.samplePeakDb = (float) (spMax > 1e-9 ? 20.0 * std::log10 (spMax) : -120.0);
    r.rmsDb = (float) (10.0 * std::log10 ((sumLL + sumRR) * 0.5 / (double) total + 1e-12));
    r.clipEvents = clipEvents;
    r.dcOffset = (float) (dcSum / (double) total);

    r.correlation = (sumLL > 0 && sumRR > 0) ? (float) std::clamp (sumLR / std::sqrt (sumLL * sumRR), -1.0, 1.0) : 1.f;
    r.sideShare = (sumMM + sumSS) > 0 ? (float) (sumSS / (sumMM + sumSS)) : 0.f;
    r.monoLossDb = (float) std::min (0.0, 10.0 * std::log10 ((sumMM + 1e-12) / ((sumLL + sumRR) * 0.5 + 1e-12)));

    // noise floor / silence from 100 ms block levels
    std::vector<float> lv; int quiet = 0;
    for (float d : subRmsDb) { if (d > -90.f) lv.push_back (d); if (d < -55.f) ++quiet; }
    r.noiseFloorDb = lv.empty() ? -120.f : percentile (lv, 0.10f);
    r.silenceRatio = (float) quiet / (float) subRmsDb.size();

    r.sibilanceDb = sibVec.empty() ? -60.f : percentile (sibVec, 0.97f);

    // tonal balance
    double tot = 0; std::array<double, kBands> bp {};
    for (int b = 0; b < kBands; ++b)
        for (int k = bandLo[(size_t) b]; k <= bandHi[(size_t) b]; ++k) bp[(size_t) b] += specSum[(size_t) k];
    for (double v : bp) tot += v;
    for (int b = 0; b < kBands; ++b)
        r.bandDb[(size_t) b] = tot > 0 ? (float) (10.0 * std::log10 (std::max (bp[(size_t) b] / tot, 1e-9))) : -90.f;

    r.valid = tot > 0;
    return r;
}
} // namespace mc
