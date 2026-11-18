#pragma once
// Mix Check AI - target standards.
// These numbers are STARTING POINTS based on common streaming / mastering practice.
// Tune them here (no other code needs to change) to match your own reference tracks.

#include "Analyzer.h"

namespace mc
{
struct Profile
{
    const char* en;
    const char* th;
    float lufs, lufsTol;         // integrated loudness target and tolerance (LU)
    float lraLo, lraHi;          // loudness range window (LU)
    float plrLo, plrHi;          // peak-to-loudness window (dB)
    std::array<float, kBands> band; // target share of total power per band (dB)
};

// Tolerance (+/- dB) of each band for full mixes.
inline const std::array<float, kBands> kMixTol = { 5.f, 3.5f, 3.f, 3.f, 3.5f, 4.5f, 5.5f };

// Order matters: the UI menu is "AI Auto" + these, in this order.
inline const std::array<Profile, 7>& profiles()
{
    static const std::array<Profile, 7> p = {{
        //  name            thai              LUFS  tol  LRA     PLR      Sub     Bass    LowMid  Mid     Pres    Brill   Air
        { "Pop",            "ป๊อป",            -11.f, 3.f, 3.f, 9.f,  7.f, 13.f, { -17.f, -5.5f, -7.f,  -6.5f, -12.f, -19.f, -29.f } },
        { "Hip-Hop",        "ฮิปฮอป",          -10.f, 3.f, 2.f, 8.f,  6.f, 12.f, { -12.f, -4.f,  -8.f,  -8.f,  -14.f, -21.f, -31.f } },
        { "EDM",            "อีดีเอ็ม",         -8.f,  3.f, 2.f, 7.f,  5.f, 10.f, { -11.f, -4.5f, -9.f,  -9.f,  -13.f, -19.f, -28.f } },
        { "Rock",           "ร็อก",            -10.f, 3.f, 3.f, 10.f, 7.f, 13.f, { -20.f, -6.f,  -6.f,  -5.f,  -10.f, -17.f, -27.f } },
        { "Acoustic",       "อะคูสติก",         -14.f, 3.f, 5.f, 14.f, 9.f, 18.f, { -26.f, -8.f,  -5.f,  -4.5f, -11.f, -19.f, -28.f } },
        { "Luk Thung / Mor Lam", "ลูกทุ่ง / หมอลำ", -10.f, 3.f, 3.f, 9.f, 7.f, 13.f, { -22.f, -7.f,  -5.5f, -4.5f, -9.5f, -16.f, -26.f } },
        { "Streaming Safe", "สตรีมมิงมาตรฐาน",  -14.f, 1.5f, 4.f, 10.f, 8.f, 14.f, { -17.f, -5.5f, -7.f,  -6.5f, -12.f, -19.f, -29.f } },
    }};
    return p;
}

// Vocal-only (dry/processed vocal track or stem)
inline const std::array<float, kBands> kVocalBand = { -34.f, -14.f, -5.f, -3.5f, -9.f, -19.f, -30.f };
inline const std::array<float, kBands> kVocalTol  = { 7.f, 4.5f, 4.f, 4.f, 4.5f, 5.5f, 7.f };

constexpr float kVocalLufsLo = -26.f, kVocalLufsHi = -14.f;   // comfortable level for a vocal stem
constexpr float kVocalLraHi = 9.f, kVocalLraLo = 2.f;
constexpr float kVocalPlrLo = 8.f, kVocalPlrHi = 18.f;
constexpr float kVocalSibWarn = 3.f, kVocalSibIssue = 8.f;    // dB (97th percentile)
constexpr float kVocalNoiseWarn = -58.f, kVocalNoiseIssue = -50.f;

// "AI Auto": pick the profile whose spectrum (and dynamics) is closest to the measurement.
inline int autoDetectProfile (const Result& r)
{
    const auto& ps = profiles();
    int best = 0; float bestCost = 1e9f;
    for (int i = 0; i < 6; ++i) // skip "Streaming Safe" (it is a loudness target, not a style)
    {
        float cost = 0.f;
        for (int b = 0; b < kBands; ++b)
        {
            const float d = (r.bandDb[(size_t) b] - ps[(size_t) i].band[(size_t) b]) / kMixTol[(size_t) b];
            cost += d * d;
        }
        if (r.lraValid)
        {
            const float d = r.loudnessRange < ps[(size_t) i].lraLo ? ps[(size_t) i].lraLo - r.loudnessRange
                          : r.loudnessRange > ps[(size_t) i].lraHi ? r.loudnessRange - ps[(size_t) i].lraHi : 0.f;
            cost += (d / 3.f) * (d / 3.f);
        }
        if (cost < bestCost) { bestCost = cost; best = i; }
    }
    return best;
}
} // namespace mc
