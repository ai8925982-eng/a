#pragma once
// Mix Check AI - "AI Advisor": turns raw measurements into scores, plain-language
// findings (Thai + English) and a prioritised fix list. On-device expert model.

#include "Analyzer.h"
#include "Profiles.h"
#include <cstdio>
#include <string>

namespace mc
{
struct Bi { std::string en, th; };

inline std::string fmt (const char* f) { return f; }
template <typename... A>
std::string fmt (const char* f, A... a)
{
    char b[768];
    std::snprintf (b, sizeof b, f, a...);
    return b;
}
template <typename... A>
Bi bi (const char* en, const char* th, A... a) { return { fmt (en, a...), fmt (th, a...) }; }

enum class Severity { Good, Warn, Issue };

struct Finding
{
    Severity sev = Severity::Good;
    Bi cat, title, detail, fix;
    float impact = 0.f;
};
struct Category { Bi name; float score = 0.f; };
struct Metric { Bi name; std::string value; Severity sev = Severity::Good; };

struct Report
{
    bool valid = false, vocal = false, autoDetected = false;
    float overall = 0.f;
    std::string grade;
    Bi verdict, summary, profileName;
    std::vector<Category> cats;
    std::vector<Finding> findings;
    std::vector<Metric> metrics;
    std::array<float, kBands> target {}, tol {};
};

// ----------------------------------------------------------------------------
inline float devScore (float dev, float tol)
{
    dev = std::fabs (dev);
    if (dev <= tol) return 100.f - 15.f * dev / tol;
    return std::max (0.f, 85.f - 35.f * (dev - tol) / tol);
}
inline float rangeDev (float v, float lo, float hi) { return v < lo ? lo - v : (v > hi ? v - hi : 0.f); }
inline Severity sevFromScore (float s) { return s >= 80.f ? Severity::Good : (s >= 55.f ? Severity::Warn : Severity::Issue); }

struct BandMsg { const char *tEN, *tTH, *dEN, *dTH, *fEN, *fTH; };

// Full-mix messages -------------------------------------------------------
inline const BandMsg kMixHigh[kBands] = {
 { "Too much sub-bass (20-60 Hz)", "ซับเบสมากเกินไป (20-60 Hz)",
   "Energy below 60 Hz eats headroom and disappears on small speakers, so the limiter works hard on sound nobody hears.",
   "พลังงานต่ำกว่า 60 Hz กินเฮดรูมและหายไปบนลำโพงเล็ก ทำให้ลิมิตเตอร์ทำงานหนักกับเสียงที่คนไม่ได้ยิน",
   "High-pass at 25-30 Hz, and tame the kick/808 sub with a low shelf or a multiband compressor.",
   "ใส่ High-pass ที่ 25-30 Hz และลดซับของกระเดื่อง/808 ด้วย Low shelf หรือ Multiband compressor" },
 { "Boomy bass (60-150 Hz)", "เบสอื้ออึง (60-150 Hz)",
   "Kick and bass pile up in the same range, so the mix sounds boomy and tiring.",
   "กระเดื่องกับเบสทับกันในย่านเดียวกัน ทำให้มิกซ์อื้อและฟังแล้วอึดอัด",
   "Cut 2-4 dB around 80-120 Hz on the less important of kick/bass, or sidechain the bass to the kick.",
   "ลด 2-4 dB ที่ 80-120 Hz ของตัวที่สำคัญน้อยกว่า (กระเดื่องหรือเบส) หรือทำ Sidechain เบสกับกระเดื่อง" },
 { "Muddy low-mids (150-400 Hz)", "เสียงขุ่น/อู้ (150-400 Hz)",
   "Too many instruments sit here, so clarity and vocal intelligibility suffer.",
   "เครื่องดนตรีหลายชิ้นอยู่ย่านนี้พร้อมกัน ทำให้ไม่ใสและคำร้องฟังไม่ชัด",
   "Cut 2-4 dB around 200-350 Hz (wide Q) on guitars, keys and pads, and high-pass instruments that don't need low end.",
   "ลด 2-4 dB ที่ 200-350 Hz (Q กว้าง) บนกีตาร์ คีย์ และ Pad และใส่ High-pass ให้เครื่องที่ไม่ต้องการเสียงต่ำ" },
 { "Boxy / honky mids (400 Hz-2 kHz)", "ย่านกลางจัดและเป็นกล่อง (400 Hz-2 kHz)",
   "The mid range dominates, so the mix sounds nasal, small and fatiguing.",
   "ย่านกลางเด่นเกินไป เสียงจึงขึ้นจมูก เล็ก และทำให้เมื่อยหู",
   "Cut 1-3 dB around 800 Hz-1.5 kHz on the busiest instruments and check the vocal isn't boosted there.",
   "ลด 1-3 dB ที่ 800 Hz-1.5 kHz บนเครื่องที่แน่นที่สุด และเช็กว่าเสียงร้องไม่ได้ถูกบูสต์ย่านนี้" },
 { "Harsh upper-mids (2-5 kHz)", "ย่าน 2-5 kHz แข็งและบาดหู",
   "Too much emphasis here causes ear fatigue and harshness at higher volume.",
   "เน้นย่านนี้มากเกินไปทำให้บาดหูและล้าเมื่อฟังดัง",
   "Cut 2-3 dB around 3-4 kHz with a dynamic EQ on vocal/guitar/snare, and prefer a soft clipper over hard limiting.",
   "ลด 2-3 dB ที่ 3-4 kHz ด้วย Dynamic EQ บนเสียงร้อง/กีตาร์/สแนร์ และใช้ Soft clipper แทนการลิมิตแรง ๆ" },
 { "Too bright / sizzly (5-10 kHz)", "สว่างและซ่ามากเกินไป (5-10 kHz)",
   "Hi-hats, cymbals and 'S' sounds stick out and become painful on earbuds.",
   "ไฮแฮต ฉาบ และเสียง S เด่นเกินไป บาดหูเมื่อฟังด้วยหูฟัง",
   "Use a de-esser around 6-8 kHz, lower hi-hat/cymbal level, or shelf down 1-2 dB above 6 kHz.",
   "ใช้ De-esser ที่ 6-8 kHz ลดระดับไฮแฮต/ฉาบ หรือ Shelf ลง 1-2 dB เหนือ 6 kHz" },
 { "Too much air (10-20 kHz)", "ย่านอากาศ (Air) มากเกินไป",
   "The top end is fizzy or hissy and can expose MP3/AAC encoding artifacts.",
   "ย่านสูงสุดซ่าหรือฟู่ และอาจเผยอาร์ติแฟกต์จากการเข้ารหัส MP3/AAC",
   "Shelf down above 12 kHz or reduce the exciter/air boost amount.",
   "Shelf ลงเหนือ 12 kHz หรือลดปริมาณ Exciter/Air boost" } };

inline const BandMsg kMixLow[kBands] = {
 { "Not enough sub-bass (20-60 Hz)", "ซับเบสน้อยไป (20-60 Hz)",
   "The low end feels thin on club systems and good headphones.",
   "ย่านต่ำให้ความรู้สึกบางบนระบบเสียงคลับและหูฟังดี ๆ",
   "Add a gentle boost around 40-55 Hz on the bass/kick, or layer a clean sub.",
   "บูสต์เบา ๆ ที่ 40-55 Hz บนเบส/กระเดื่อง หรือเลเยอร์ซับที่สะอาด" },
 { "Weak bass (60-150 Hz)", "เบสน้อยไป (60-150 Hz)",
   "The mix lacks weight and punch.",
   "มิกซ์ขาดน้ำหนักและความแน่น",
   "Boost bass/kick by 2-3 dB near 70-100 Hz and check bass level against the vocal.",
   "บูสต์เบส/กระเดื่อง 2-3 dB ใกล้ 70-100 Hz และเช็กระดับเบสเทียบกับเสียงร้อง" },
 { "Thin low-mids (150-400 Hz)", "ย่านโลว์-มิดบาง (150-400 Hz)",
   "Body and warmth are missing, so the mix sounds hollow.",
   "ขาดเนื้อเสียงและความอุ่น มิกซ์จึงฟังโหวง",
   "Boost 1-2 dB around 200-300 Hz on rhythm instruments, and check for over-aggressive high-pass filters.",
   "บูสต์ 1-2 dB ที่ 200-300 Hz บนเครื่องจังหวะ และเช็กว่าไม่ได้ใส่ High-pass แรงเกินไป" },
 { "Recessed mids (400 Hz-2 kHz)", "ย่านกลางหายไป (400 Hz-2 kHz)",
   "Vocals and lead instruments sound distant and the mix feels scooped out.",
   "เสียงร้องและเครื่องนำฟังห่างและมิกซ์ดูโหว่",
   "Boost 1-3 dB around 500 Hz-1.5 kHz on the vocal/lead and ease off any 'smile curve' EQ.",
   "บูสต์ 1-3 dB ที่ 500 Hz-1.5 kHz บนเสียงร้อง/เครื่องนำ และลด EQ แบบตัว V ที่ใส่ไว้" },
 { "Lack of presence (2-5 kHz)", "ขาดความชัด (Presence 2-5 kHz)",
   "Vocals lack definition and the mix sounds dull or far away.",
   "เสียงร้องไม่คมชัด มิกซ์ฟังทึบหรือห่าง",
   "Boost 1-2 dB around 3-4 kHz on the vocal/lead and add light saturation.",
   "บูสต์ 1-2 dB ที่ 3-4 kHz บนเสียงร้อง/เครื่องนำ และเติม Saturation เบา ๆ" },
 { "Dull top end (5-10 kHz)", "ย่านสูงทึบ (5-10 kHz)",
   "Sparkle and detail are missing.",
   "ขาดประกายและรายละเอียด",
   "High-shelf +1-2 dB from 6-8 kHz and add a light exciter on vocal or overheads.",
   "High-shelf +1-2 dB จาก 6-8 kHz และเติม Exciter เบา ๆ บนเสียงร้องหรือโอเวอร์เฮด" },
 { "Lack of air (10-20 kHz)", "ขาดความโปร่ง (Air 10-20 kHz)",
   "The mix feels closed-in and less open. Low-bitrate MP3 files can also cause this.",
   "มิกซ์ฟังอึดอัดและไม่โปร่ง ไฟล์ MP3 บิตเรตต่ำก็ทำให้เกิดได้",
   "Gentle wide high-shelf +1-2 dB at 10-12 kHz, and analyse a WAV/FLAC instead of a low-bitrate MP3.",
   "High-shelf กว้าง ๆ +1-2 dB ที่ 10-12 kHz และวิเคราะห์ไฟล์ WAV/FLAC แทน MP3 บิตเรตต่ำ" } };

// Vocal-only messages -------------------------------------------------------
inline const BandMsg kVocHigh[kBands] = {
 { "Rumble / plosives (below 60 Hz)", "เสียงอื้อต่ำ/ลมปะทะไมค์ (ต่ำกว่า 60 Hz)",
   "Low rumble, mic-stand thumps or P/B pops are present. They eat headroom and muddy the mix.",
   "มีเสียงอื้อต่ำ เสียงกระแทกขาตั้งไมค์ หรือลมจากตัว P/B กินเฮดรูมและทำให้มิกซ์ขุ่น",
   "High-pass at 80-100 Hz (12-24 dB/oct) and use a pop filter when recording.",
   "ใส่ High-pass ที่ 80-100 Hz (12-24 dB/oct) และใช้ Pop filter ตอนอัด" },
 { "Boomy / proximity effect (60-150 Hz)", "เสียงอื้อจากการอยู่ใกล้ไมค์ (60-150 Hz)",
   "Too much low body, often from singing close to the mic. The voice gets boomy and fights the bass.",
   "ย่านต่ำมากเกินไป มักเกิดจากร้องใกล้ไมค์ เสียงอื้อและแย่งที่กับเบส",
   "Cut 2-4 dB at 100-150 Hz or raise the high-pass to ~100 Hz.",
   "ลด 2-4 dB ที่ 100-150 Hz หรือเลื่อน High-pass ขึ้นไปราว 100 Hz" },
 { "Muddy / boxy (150-400 Hz)", "เสียงขุ่น/เป็นกล่อง (150-400 Hz)",
   "Room box or mud makes the vocal dull and hides the words.",
   "เสียงสะท้อนห้องหรือความขุ่นทำให้เสียงร้องทึบและคำร้องไม่ชัด",
   "Cut 2-4 dB around 250-350 Hz with a medium Q, and record in a less reflective room.",
   "ลด 2-4 dB ที่ 250-350 Hz ด้วย Q ปานกลาง และอัดในห้องที่สะท้อนน้อยลง" },
 { "Nasal / honky (400 Hz-2 kHz)", "เสียงขึ้นจมูก/แหลมกล่อง (400 Hz-2 kHz)",
   "The voice sounds nasal, pinched or telephone-like.",
   "เสียงร้องฟังขึ้นจมูก ตีบ หรือเหมือนเสียงโทรศัพท์",
   "Cut 2-3 dB around 800 Hz-1.2 kHz and check the mic position.",
   "ลด 2-3 dB ที่ 800 Hz-1.2 kHz และเช็กตำแหน่งไมค์" },
 { "Harsh presence (2-5 kHz)", "ย่าน 2-5 kHz บาดหู",
   "The vocal is piercing and tiring, especially at loud volume.",
   "เสียงร้องแหลมบาดหูและทำให้ล้า โดยเฉพาะเมื่อฟังดัง",
   "Dynamic EQ cut 2-3 dB at 3-4 kHz and soften with gentle saturation.",
   "ใช้ Dynamic EQ ลด 2-3 dB ที่ 3-4 kHz และเติม Saturation เบา ๆ ให้นุ่มลง" },
 { "Sizzly top (5-10 kHz)", "ย่านสูงซ่า (5-10 kHz)",
   "Too much 'S' and breath noise, harsh on earbuds.",
   "เสียง S และลมหายใจมากเกินไป บาดหูบนหูฟัง",
   "De-esser at 5-8 kHz, or shelf down 1-2 dB above 6 kHz.",
   "ใช้ De-esser ที่ 5-8 kHz หรือ Shelf ลง 1-2 dB เหนือ 6 kHz" },
 { "Hissy air (10-20 kHz)", "ย่านอากาศฟู่ (10-20 kHz)",
   "Hiss, noise or an over-done air boost.",
   "เสียงฟู่ นอยส์ หรือบูสต์ Air มากเกินไป",
   "Lower the air shelf or use noise reduction / a low-pass at 14-16 kHz.",
   "ลด Air shelf หรือใช้ Noise reduction / Low-pass ที่ 14-16 kHz" } };

inline const BandMsg kVocLow[kBands] = {
 { "", "", "", "", "", "" }, // too little sub is fine for vocals
 { "Thin low body (60-150 Hz)", "เสียงบาง ขาดความหนา (60-150 Hz)",
   "The voice lacks chest and warmth.", "เสียงร้องขาดความหนาและความอุ่น",
   "Gentle +1-2 dB shelf near 120-200 Hz, and ease off an aggressive high-pass.",
   "บูสต์ Shelf เบา ๆ +1-2 dB ที่ 120-200 Hz และลด High-pass ที่แรงเกินไป" },
 { "Hollow low-mids (150-400 Hz)", "ย่านโลว์-มิดโหวง (150-400 Hz)",
   "The voice lacks warmth and body.", "เสียงร้องขาดความอุ่นและเนื้อเสียง",
   "Boost 1-2 dB around 200-300 Hz.", "บูสต์ 1-2 dB ที่ 200-300 Hz" },
 { "Recessed mids (400 Hz-2 kHz)", "ย่านกลางหาย (400 Hz-2 kHz)",
   "The vocal sounds distant and thin.", "เสียงร้องฟังห่างและบาง",
   "Boost 1-2 dB at 700 Hz-1.5 kHz, or reduce any cuts there.",
   "บูสต์ 1-2 dB ที่ 700 Hz-1.5 kHz หรือลดการตัดย่านนี้" },
 { "Lack of clarity (2-5 kHz)", "ขาดความชัด (2-5 kHz)",
   "Words aren't clear and the vocal sits behind the instruments.",
   "คำร้องไม่ชัดและเสียงร้องจมอยู่หลังเครื่องดนตรี",
   "Boost 1-3 dB around 3-4 kHz, or try parallel compression/saturation.",
   "บูสต์ 1-3 dB ที่ 3-4 kHz หรือลอง Parallel compression/Saturation" },
 { "Dull top (5-10 kHz)", "ย่านสูงทึบ (5-10 kHz)",
   "The vocal lacks sparkle and detail.", "เสียงร้องขาดประกายและรายละเอียด",
   "High-shelf +1-2 dB at 6-8 kHz or add an exciter, and avoid over-de-essing.",
   "High-shelf +1-2 dB ที่ 6-8 kHz หรือเติม Exciter และอย่า De-ess แรงเกินไป" },
 { "Lack of air (10-20 kHz)", "ขาดความโปร่ง (10-20 kHz)",
   "The vocal sounds closed-in. Low-bitrate MP3 sources can also cause this.",
   "เสียงร้องฟังอึดอัด ไฟล์ MP3 บิตเรตต่ำก็ทำให้เป็นแบบนี้ได้",
   "Wide +1-2 dB shelf at 10-12 kHz, and use a lossless source.",
   "Shelf กว้าง +1-2 dB ที่ 10-12 kHz และใช้ไฟล์แบบไม่สูญเสีย (WAV/FLAC)" } };

// ----------------------------------------------------------------------------
namespace detail
{
inline void addFinding (Report& rep, Severity s, Bi cat, Bi title, Bi detail, Bi fix, float impact)
{
    Finding f; f.sev = s; f.cat = std::move (cat); f.title = std::move (title);
    f.detail = std::move (detail); f.fix = std::move (fix); f.impact = impact;
    rep.findings.push_back (std::move (f));
}

inline float evalBands (const Result& r, Report& rep, bool vocal, const Bi& cat, float catWeight)
{
    static const float w[kBands] = { 0.08f, 0.18f, 0.20f, 0.20f, 0.16f, 0.12f, 0.06f };
    float sum = 0.f, wsum = 0.f, worst = 100.f;
    bool anyBad = false;
    for (int b = 0; b < kBands; ++b)
    {
        float dev = r.bandDb[(size_t) b] - rep.target[(size_t) b];
        if (vocal && b == 0 && dev < 0.f) dev = 0.f; // little sub is fine on a vocal
        const float s = devScore (dev, rep.tol[(size_t) b]);
        sum += s * w[b]; wsum += w[b]; worst = std::min (worst, s);

        const float ad = std::fabs (dev);
        if (ad <= rep.tol[(size_t) b]) continue;
        anyBad = true;
        const BandMsg& m = dev > 0 ? (vocal ? kVocHigh[b] : kMixHigh[b]) : (vocal ? kVocLow[b] : kMixLow[b]);
        const std::string meas = fmt (" [%.1f dB vs %.1f dB]", r.bandDb[(size_t) b], rep.target[(size_t) b]);
        addFinding (rep, ad <= 2.f * rep.tol[(size_t) b] ? Severity::Warn : Severity::Issue, cat,
                    { m.tEN, m.tTH }, { std::string (m.dEN) + meas, std::string (m.dTH) + meas }, { m.fEN, m.fTH },
                    (100.f - s) * catWeight * w[b] / 0.2f);
    }
    if (! anyBad)
        addFinding (rep, Severity::Good, cat,
                    { "Tonal balance matches the target", "สมดุลความถี่ตรงตามเป้าหมาย" },
                    { "All seven frequency bands sit inside the target zone.", "ทั้ง 7 ย่านความถี่อยู่ในโซนเป้าหมาย" }, {}, 0.f);
    return 0.7f * (sum / wsum) + 0.3f * worst;
}

inline std::string gradeOf (float s)
{
    return s >= 92.f ? "A+" : s >= 85.f ? "A" : s >= 75.f ? "B" : s >= 60.f ? "C" : s >= 40.f ? "D" : "F";
}
inline Bi verdictOf (float s)
{
    if (s >= 92.f) return { "RELEASE-READY", "พร้อมปล่อยได้เลย" };
    if (s >= 85.f) return { "EXCELLENT - minor polish", "ดีมาก เก็บรายละเอียดอีกนิด" };
    if (s >= 75.f) return { "GOOD - a few fixes", "ดี แก้อีกไม่กี่จุด" };
    if (s >= 60.f) return { "NEEDS WORK", "ต้องปรับปรุง" };
    if (s >= 40.f) return { "MAJOR ISSUES", "มีปัญหาหลายจุด" };
    return { "NOT READY", "ยังไม่พร้อม" };
}

inline void finalise (Report& rep, const std::vector<float>& weights)
{
    float o = 0.f, ws = 0.f;
    for (size_t i = 0; i < rep.cats.size(); ++i) { o += rep.cats[i].score * weights[i]; ws += weights[i]; }
    rep.overall = std::clamp (o / ws, 0.f, 100.f);

    bool hardIssue = false;
    for (auto& f : rep.findings) if (f.sev == Severity::Issue && f.impact > 4.f && (f.cat.en == "Peak & Headroom" || f.cat.en == "Technical" || f.cat.en == "Cleanliness" || f.cat.en == "Level & Headroom")) hardIssue = true;
    if (hardIssue) rep.overall = std::min (rep.overall, 79.f); // clipping / true-peak overs cap the score

    rep.grade = gradeOf (rep.overall);
    rep.verdict = verdictOf (rep.overall);

    std::stable_sort (rep.findings.begin(), rep.findings.end(), [] (const Finding& a, const Finding& b)
    {
        auto key = [] (const Finding& f) { return f.sev == Severity::Good ? -1000.f : (f.sev == Severity::Issue ? 1000.f : 0.f) + f.impact; };
        return key (a) > key (b);
    });

    // AI summary: top three priorities
    std::string en = fmt ("Score %.0f/100 (%s) against the %s standard. ", rep.overall, rep.grade.c_str(), rep.profileName.en.c_str());
    std::string th = fmt ("ได้ %.0f/100 (เกรด %s) เมื่อเทียบกับมาตรฐาน %s ", rep.overall, rep.grade.c_str(), rep.profileName.th.c_str());
    int n = 0;
    for (auto& f : rep.findings)
    {
        if (f.sev == Severity::Good) continue;
        if (n == 0) { en += "Fix these first: "; th += "แก้จุดเหล่านี้ก่อน: "; }
        en += fmt ("%s%d) %s", n ? "  " : "", n + 1, f.title.en.c_str());
        th += fmt ("%s%d) %s", n ? "  " : "", n + 1, f.title.th.c_str());
        if (++n == 3) break;
    }
    if (n == 0) { en += "No significant problems found. Great work!"; th += "ไม่พบปัญหาสำคัญ ยอดเยี่ยมมาก!"; }
    else { en += ". Re-analyse after each change."; th += " แก้แล้ววิเคราะห์ซ้ำเพื่อดูคะแนนที่ดีขึ้น"; }
    rep.summary = { en, th };
    rep.valid = true;
}
} // namespace detail

// ----------------------------------------------------------------------------
inline Report evaluate (const Result& r, bool vocal, int profileChoice /*0 = AI Auto*/)
{
    using detail::addFinding;
    Report rep; rep.vocal = vocal;
    if (! r.valid) return rep;

    const float lufs = r.integratedLufs, tp = r.truePeakDb;
    const float plr = tp - lufs;
    const bool longEnough = r.durationSec >= 30.0;

    // ------------------------------------------------------------ FULL MIX
    if (! vocal)
    {
        int idx = profileChoice - 1;
        if (profileChoice <= 0) { idx = autoDetectProfile (r); rep.autoDetected = true; }
        const Profile& p = profiles()[(size_t) idx];
        rep.profileName = { p.en, p.th };
        rep.target = p.band; rep.tol = kMixTol;

        const Bi cLoud { "Loudness", "ความดัง" }, cPeak { "Peak & Headroom", "พีคและเฮดรูม" }, cDyn { "Dynamics", "ไดนามิก" },
                 cTone { "Tonal Balance", "สมดุลความถี่" }, cSt { "Stereo & Phase", "สเตอริโอและเฟส" }, cTech { "Technical", "คุณภาพไฟล์" };

        // Loudness
        const float dL = lufs - p.lufs;
        const float sLoud = devScore (dL, p.lufsTol);
        if (dL < -p.lufsTol)
            addFinding (rep, std::fabs (dL) > 2 * p.lufsTol ? Severity::Issue : Severity::Warn, cLoud,
                        bi ("Loudness is too low (%.1f LUFS)", "ความดังต่ำเกินไป (%.1f LUFS)", lufs),
                        { fmt ("Target for %s is about %.0f LUFS. Your track is %.1f LU quieter, so it will sound weak next to commercial releases.", p.en, p.lufs, -dL),
                          fmt ("เป้าหมายของแนว %s อยู่ที่ราว %.0f LUFS แต่เพลงของคุณเบากว่า %.1f LU จึงฟังเบาเมื่อเทียบกับเพลงที่วางขายจริง", p.th, p.lufs, -dL) },
                        bi ("Raise the gain into your final limiter toward %.0f LUFS and keep the ceiling at -1 dBTP. If it distorts first, fix the low end and loud transients before pushing level.",
                            "เพิ่มเกนเข้าลิมิตเตอร์ตัวสุดท้ายให้ใกล้ %.0f LUFS ตั้ง Ceiling ที่ -1 dBTP ถ้าเริ่มแตกก่อน ให้แก้ย่านต่ำและทรานเซียนต์ที่พุ่งก่อนแล้วค่อยเพิ่มความดัง", p.lufs),
                        (100.f - sLoud) * 0.20f);
        else if (dL > p.lufsTol)
            addFinding (rep, dL > 2 * p.lufsTol ? Severity::Issue : Severity::Warn, cLoud,
                        bi ("Loudness is too high (%.1f LUFS)", "ความดังสูงเกินไป (%.1f LUFS)", lufs),
                        { fmt ("%.1f LU above the %s target. Streaming services normalise around -14 LUFS, so extra loudness is simply turned down again - you keep the distortion and lose punch.", dL, p.en),
                          fmt ("สูงกว่าเป้าหมายแนว %s อยู่ %.1f LU แพลตฟอร์มสตรีมมิงจะปรับความดังลงที่ราว -14 LUFS ความดังที่เกินจึงไม่ได้ช่วยอะไร เหลือแต่เสียงแตกและความพุ่งที่หายไป", p.th, dL) },
                        bi ("Pull the limiter input down by about %.1f dB and let transients breathe. Compare with a reference at the same loudness.",
                            "ลดเกนที่เข้าลิมิตเตอร์ประมาณ %.1f dB ให้ทรานเซียนต์ได้หายใจ และเทียบกับเพลงอ้างอิงที่ความดังเท่ากัน", dL - p.lufsTol),
                        (100.f - sLoud) * 0.20f);
        else
            addFinding (rep, Severity::Good, cLoud,
                        bi ("Loudness is on target (%.1f LUFS)", "ความดังอยู่ในเป้าหมาย (%.1f LUFS)", lufs),
                        { fmt ("Within +/-%.1f LU of the %s standard.", p.lufsTol, p.en), fmt ("อยู่ในช่วง +/-%.1f LU ของมาตรฐานแนว %s", p.lufsTol, p.th) }, {}, 0.f);

        // Peak & headroom
        float sPeak = tp <= -1.f ? 100.f : (tp <= 0.f ? 100.f + 30.f * tp : std::max (0.f, 70.f - 40.f * tp));
        if (tp > 0.f)
            addFinding (rep, Severity::Issue, cPeak, bi ("True peak is over full scale (%+.1f dBTP)", "True Peak เกิน 0 dBTP (%+.1f)", tp),
                        bi ("Inter-sample peaks rise above 0 dBFS. After MP3/AAC conversion or on some DACs this turns into audible clipping.",
                            "พีคระหว่างตัวอย่างเกิน 0 dBFS เมื่อแปลงเป็น MP3/AAC หรือเล่นผ่าน DAC บางรุ่นจะได้ยินเป็นเสียงแตก"),
                        bi ("Set the limiter ceiling to -1.0 dBTP with true-peak / oversampling ON, then bounce again.",
                            "ตั้ง Ceiling ของลิมิตเตอร์ที่ -1.0 dBTP เปิดโหมด True Peak/Oversampling แล้วบาวน์ใหม่"), (100.f - sPeak) * 0.15f + 5.f);
        else if (tp > -1.f)
            addFinding (rep, Severity::Warn, cPeak, bi ("Very little headroom (%+.1f dBTP)", "เฮดรูมน้อย (%+.1f dBTP)", tp),
                        bi ("Streaming platforms recommend peaks at -1 dBTP or lower so lossy encoding does not clip.",
                            "แพลตฟอร์มสตรีมมิงแนะนำให้พีคไม่เกิน -1 dBTP เพื่อไม่ให้การเข้ารหัสแบบ lossy ทำให้แตก"),
                        bi ("Lower the limiter ceiling to -1.0 dBTP with true-peak mode ON.", "ลด Ceiling ของลิมิตเตอร์เป็น -1.0 dBTP และเปิดโหมด True Peak"), (100.f - sPeak) * 0.15f);
        else
            addFinding (rep, Severity::Good, cPeak, bi ("Peak level is safe (%+.1f dBTP)", "ระดับพีคปลอดภัย (%+.1f dBTP)", tp),
                        bi ("Under the -1 dBTP ceiling.", "ต่ำกว่าเพดาน -1 dBTP"), {}, 0.f);

        // Dynamics
        const float plrDev = rangeDev (plr, p.plrLo, p.plrHi);
        float sDyn = devScore (plrDev, 3.f);
        if (plr < p.plrLo)
            addFinding (rep, plrDev > 3.f ? Severity::Issue : Severity::Warn, cDyn,
                        bi ("Over-compressed (PLR %.1f dB)", "ถูกบีบอัดมากเกินไป (PLR %.1f dB)", plr),
                        bi ("Peaks are only %.1f dB above the average loudness (typical here: %.0f-%.0f dB). The track can feel flat, lifeless and tiring.",
                            "พีคสูงกว่าความดังเฉลี่ยเพียง %.1f dB (ปกติราว %.0f-%.0f dB) เพลงจึงฟังแบน ไร้ชีวิต และทำให้เมื่อยหู", plr, p.plrLo, p.plrHi),
                        bi ("Reduce gain reduction on the master limiter/compressor (3-4 dB or less) and shape balance with bus compression earlier in the chain.",
                            "ลดการกดบนลิมิตเตอร์/คอมเพรสเซอร์ของ Master (ราว 3-4 dB หรือน้อยกว่า) และไปคุมบาลานซ์ที่บัสก่อนหน้าแทน"), (100.f - sDyn) * 0.15f);
        else if (plr > p.plrHi)
            addFinding (rep, plrDev > 3.f ? Severity::Issue : Severity::Warn, cDyn,
                        bi ("Peaks are too spiky (PLR %.1f dB)", "พีคพุ่งสูงเกินไป (PLR %.1f dB)", plr),
                        bi ("A few loud transients (usually kick or snare) tower over everything else, so the whole track has to stay quieter.",
                            "ทรานเซียนต์ไม่กี่จุด (มักเป็นกระเดื่องหรือสแนร์) พุ่งสูงกว่าส่วนอื่นมาก ทำให้เพลงโดยรวมต้องเบาลง"),
                        bi ("Lightly clip or limit the drum bus before the master limiter to shave the biggest peaks.",
                            "ใช้ Clipper หรือลิมิตเตอร์เบา ๆ ที่บัสกลองก่อนเข้า Master เพื่อตัดพีคที่สูงที่สุด"), (100.f - sDyn) * 0.15f);
        else
            addFinding (rep, Severity::Good, cDyn, bi ("Healthy punch (PLR %.1f dB)", "ความพุ่งสมดุล (PLR %.1f dB)", plr),
                        bi ("Peaks and average loudness have a natural relationship.", "พีคและความดังเฉลี่ยมีความสัมพันธ์ที่เป็นธรรมชาติ"), {}, 0.f);

        if (r.lraValid && longEnough)
        {
            const float lraDev = rangeDev (r.loudnessRange, p.lraLo, p.lraHi);
            sDyn = 0.5f * sDyn + 0.5f * devScore (lraDev, 3.f);
            if (r.loudnessRange < p.lraLo)
                addFinding (rep, Severity::Warn, cDyn, bi ("Very little level variation (LRA %.1f LU)", "ระดับเสียงนิ่งมาก (LRA %.1f LU)", r.loudnessRange),
                            bi ("Every section sounds equally loud, so verse and chorus lack contrast.", "ทุกท่อนดังเท่ากัน เพลงจึงขาดความแตกต่างระหว่างท่อนเวิร์สกับท่อนฮุก"),
                            bi ("Automate levels or arrangement so the chorus lifts and breakdowns drop.", "ทำ Automation หรือปรับอะเรนจ์ให้ท่อนฮุกยกขึ้นและท่อนเบรกลงต่ำลง"), (100.f - sDyn) * 0.08f);
            else if (r.loudnessRange > p.lraHi)
                addFinding (rep, Severity::Warn, cDyn, bi ("Large level swings (LRA %.1f LU)", "ระดับเสียงขึ้นลงมาก (LRA %.1f LU)", r.loudnessRange),
                            bi ("Quiet and loud passages differ a lot, so quiet parts can vanish in cars or on phones.", "ช่วงเบาและช่วงดังต่างกันมาก ช่วงเบาอาจหายเมื่อฟังในรถหรือบนมือถือ"),
                            bi ("Automate quiet sections up, or use gentle bus compression to even things out.", "ยกช่วงที่เบาด้วย Automation หรือใช้คอมเพรสเซอร์เบา ๆ ที่บัสเพื่อให้สม่ำเสมอขึ้น"), (100.f - sDyn) * 0.08f);
        }

        // Tonal balance
        const float sTone = detail::evalBands (r, rep, false, cTone, 0.30f);

        // Stereo & phase
        float sSt = 100.f;
        if (r.mono)
        {
            sSt = 60.f;
            addFinding (rep, Severity::Warn, cSt, bi ("The file is mono", "ไฟล์เป็นโมโน"),
                        bi ("Stereo image and phase cannot be judged on a mono file.", "ไฟล์โมโนจึงประเมินภาพสเตอริโอและเฟสไม่ได้"),
                        bi ("Export in stereo if the mix is meant to be stereo.", "ส่งออกเป็นสเตอริโอถ้ามิกซ์ตั้งใจให้เป็นสเตอริโอ"), 2.f);
        }
        else
        {
            const float sidePct = r.sideShare * 100.f;
            if (r.correlation < 0.5f) sSt -= (0.5f - r.correlation) * 60.f;
            if (r.monoLossDb < -1.5f) sSt -= (-r.monoLossDb - 1.5f) * 12.f;
            if (sidePct < 1.5f) sSt -= 15.f;
            if (sidePct > 40.f) sSt -= 15.f;
            sSt = std::clamp (sSt, 0.f, 100.f);

            bool bad = false;
            if (r.monoLossDb < -1.5f)
            {
                bad = true;
                addFinding (rep, r.monoLossDb < -3.f ? Severity::Issue : Severity::Warn, cSt,
                            bi ("Mono compatibility problem (%.1f dB)", "ฟังแบบโมโนแล้วเสียงหาย (%.1f dB)", r.monoLossDb),
                            bi ("Summed to mono the level drops by %.1f dB because of phase cancellation. Phones, club systems and smart speakers often play mono.",
                                "เมื่อรวมเป็นโมโนระดับเสียงลดลง %.1f dB เพราะเฟสหักล้างกัน มือถือ ลำโพงคลับ และสมาร์ทสปีกเกอร์มักเล่นแบบโมโน", -r.monoLossDb),
                            bi ("Check stereo wideners, chorus and doubled tracks, mix with the mono button on, and keep bass below ~120 Hz centred.",
                                "ตรวจตัวขยายสเตอริโอ คอรัส และแทร็กที่ดับเบิล กดปุ่ม Mono ตอนมิกซ์ และให้เบสต่ำกว่า ~120 Hz อยู่กึ่งกลาง"), (100.f - sSt) * 0.10f);
            }
            if (r.correlation < 0.f)
            {
                bad = true;
                addFinding (rep, Severity::Issue, cSt, bi ("Channels are out of phase (corr %.2f)", "สองข้างเฟสตรงข้ามกัน (corr %.2f)", r.correlation),
                            bi ("Left and right largely cancel each other, usually a polarity or timing problem.", "ซ้ายและขวาหักล้างกันเป็นส่วนใหญ่ มักเกิดจากโพลาริตี้หรือเวลาไม่ตรง"),
                            bi ("Flip polarity on suspect tracks and check mic/plugin latency compensation.", "ลองกลับเฟสแทร็กที่น่าสงสัย และตรวจการชดเชยความหน่วงของไมค์/ปลั๊กอิน"), (100.f - sSt) * 0.10f);
            }
            if (sidePct < 1.5f)
            {
                bad = true;
                addFinding (rep, Severity::Warn, cSt, bi ("Almost mono (side %.1f%%)", "ภาพเสียงแคบเกือบโมโน (Side %.1f%%)", sidePct),
                            bi ("Very little stereo information, so the mix may sound small and flat on headphones.", "มีข้อมูลสเตอริโอน้อยมาก มิกซ์อาจฟังเล็กและแบนบนหูฟัง"),
                            bi ("Pan instruments, add stereo reverb/delays and wider doubles on guitars, keys and backing vocals.", "แพนเครื่องดนตรี เพิ่มรีเวิร์บ/ดีเลย์สเตอริโอ และดับเบิลกีตาร์ คีย์ และคอรัสให้กว้างขึ้น"), 3.f);
            }
            if (sidePct > 40.f)
            {
                bad = true;
                addFinding (rep, Severity::Warn, cSt, bi ("Extremely wide (side %.0f%%)", "กว้างมากเกินไป (Side %.0f%%)", sidePct),
                            bi ("Side energy is very high compared with the centre, so vocal and kick may feel weak and mono playback suffers.", "พลังงาน Side สูงมากเมื่อเทียบกับกึ่งกลาง เสียงร้องและกระเดื่องอาจอ่อนลงและฟังแบบโมโนแล้วเสียเสียง"),
                            bi ("Bring wide elements down a little and keep lead vocal, kick and bass in the centre.", "ลดองค์ประกอบที่กว้างลงเล็กน้อย และให้เสียงร้องนำ กระเดื่อง และเบสอยู่กึ่งกลาง"), 3.f);
            }
            if (! bad)
                addFinding (rep, Severity::Good, cSt, bi ("Stereo image is balanced (side %.0f%%)", "ภาพสเตอริโอสมดุล (Side %.0f%%)", sidePct),
                            bi ("Good mono compatibility (%.1f dB) and healthy correlation (%.2f).", "เข้ากับโมโนได้ดี (%.1f dB) และค่า Correlation ดี (%.2f)", r.monoLossDb, r.correlation), {}, 0.f);
        }

        // Technical
        float sTech = 100.f;
        bool techBad = false;
        if (r.clipEvents > 0)
        {
            techBad = true;
            sTech -= std::min (70.f, 20.f + 15.f * std::log10 ((float) r.clipEvents));
            addFinding (rep, Severity::Issue, cTech, bi ("Digital clipping detected (%d events)", "พบเสียงแตกดิจิทัล (%d จุด)", r.clipEvents),
                        bi ("Consecutive samples are pinned at full scale, which sounds like crackle or harsh distortion.", "ตัวอย่างเสียงติดเพดานเต็มสเกลต่อเนื่องกัน ได้ยินเป็นเสียงแตกพร่าหรือบาดหู"),
                        bi ("Lower the level before the clipping stage (limiter input, a hot plugin or an already-clipped sample) and re-export at 24-bit or 32-bit float.",
                            "ลดระดับก่อนจุดที่แตก (อินพุตลิมิตเตอร์ ปลั๊กอินที่ดังเกิน หรือไฟล์ตัวอย่างที่แตกมาแล้ว) แล้วส่งออกใหม่แบบ 24-bit หรือ 32-bit float"), (100.f - sTech) * 0.10f + 5.f);
        }
        if (std::fabs (r.dcOffset) > 0.005f)
        {
            techBad = true;
            sTech -= std::fabs (r.dcOffset) > 0.02f ? 30.f : 15.f;
            addFinding (rep, std::fabs (r.dcOffset) > 0.02f ? Severity::Issue : Severity::Warn, cTech, bi ("DC offset (%.3f)", "พบ DC offset (%.3f)", r.dcOffset),
                        bi ("A constant offset wastes headroom and can cause clicks at edits.", "ค่าชดเชยคงที่กินเฮดรูมและอาจทำให้เกิดเสียงคลิกตรงจุดตัดต่อ"),
                        bi ("Apply a 20 Hz high-pass or a DC-removal plugin.", "ใส่ High-pass 20 Hz หรือปลั๊กอินลบ DC"), (100.f - sTech) * 0.10f);
        }
        if (! techBad)
            addFinding (rep, Severity::Good, cTech, bi ("File is technically clean", "ไฟล์สะอาดทางเทคนิค"), bi ("No clipping or DC offset found.", "ไม่พบเสียงแตกหรือ DC offset"), {}, 0.f);
        sTech = std::max (0.f, sTech);

        rep.cats = { { cLoud, sLoud }, { cPeak, sPeak }, { cDyn, sDyn }, { cTone, sTone }, { cSt, sSt }, { cTech, sTech } };

        auto sv = [] (float s) { return sevFromScore (s); };
        rep.metrics = {
            { { "Loudness", "ความดัง" }, fmt ("%.1f LUFS", lufs), sv (sLoud) },
            { { "True peak", "True Peak" }, fmt ("%+.1f dBTP", tp), tp > 0 ? Severity::Issue : tp > -1 ? Severity::Warn : Severity::Good },
            { { "Loudness range", "ช่วงความดัง (LRA)" }, r.lraValid ? fmt ("%.1f LU", r.loudnessRange) : "-", (! r.lraValid || (r.loudnessRange >= p.lraLo && r.loudnessRange <= p.lraHi)) ? Severity::Good : Severity::Warn },
            { { "Peak-to-loudness", "พีคต่อความดัง (PLR)" }, fmt ("%.1f dB", plr), plrDev == 0 ? Severity::Good : Severity::Warn },
            { { "Correlation", "Correlation" }, fmt ("%+.2f", r.correlation), r.correlation < 0 ? Severity::Issue : r.correlation < 0.3f ? Severity::Warn : Severity::Good },
            { { "Mono loss", "เสียงหายเมื่อรวมโมโน" }, fmt ("%.1f dB", r.monoLossDb), r.monoLossDb < -3 ? Severity::Issue : r.monoLossDb < -1.5f ? Severity::Warn : Severity::Good },
            { { "Stereo width", "ความกว้างสเตอริโอ" }, fmt ("%.0f %%", r.sideShare * 100.f), (r.sideShare < 0.015f || r.sideShare > 0.4f) ? Severity::Warn : Severity::Good },
            { { "Clipping", "เสียงแตก" }, fmt ("%d", r.clipEvents), r.clipEvents > 0 ? Severity::Issue : Severity::Good } };

        detail::finalise (rep, { 0.20f, 0.15f, 0.15f, 0.30f, 0.10f, 0.10f });
        return rep;
    }

    // ------------------------------------------------------------ VOCAL ONLY
    rep.profileName = { "Vocal standard", "มาตรฐานเสียงร้อง" };
    rep.target = kVocalBand; rep.tol = kVocalTol;

    const Bi cLev { "Level & Headroom", "ระดับเสียงและเฮดรูม" }, cCons { "Consistency", "ความสม่ำเสมอ" }, cTone { "Tone", "โทนเสียง" },
             cSib { "Sibilance", "เสียงเสียดแทรก (S)" }, cClean { "Cleanliness", "ความสะอาด" };

    // Level & headroom
    float sPk = tp <= -3.f ? 100.f : (tp <= -1.f ? 90.f : (tp <= 0.f ? 90.f + 20.f * tp : std::max (0.f, 70.f - 40.f * tp)));
    if (tp < -24.f) sPk = std::min (sPk, 70.f); else if (tp < -18.f) sPk = std::min (sPk, 85.f);
    const float sLv = devScore (rangeDev (lufs, kVocalLufsLo, kVocalLufsHi), 4.f);
    const float sLevel = 0.5f * sPk + 0.5f * sLv;
    bool levelBad = false;
    if (tp > 0.f)
    {
        levelBad = true;
        addFinding (rep, Severity::Issue, cLev, bi ("Vocal peaks go over 0 dBTP (%+.1f)", "เสียงร้องพีคเกิน 0 dBTP (%+.1f)", tp),
                    bi ("The vocal is clipping or will clip after conversion. Distortion on the lead vocal is very noticeable.", "เสียงร้องแตกหรือจะแตกหลังแปลงไฟล์ ความเพี้ยนบนเสียงร้องนำได้ยินชัดมาก"),
                    bi ("Lower the track/clip gain so peaks land around -6 to -3 dBFS. Re-record if the original is already clipped.", "ลดเกนของแทร็กให้พีคอยู่ราว -6 ถึง -3 dBFS ถ้าต้นฉบับแตกแล้วควรอัดใหม่"), 20.f);
    }
    else if (tp > -1.f)
    {
        levelBad = true;
        addFinding (rep, Severity::Warn, cLev, bi ("Vocal is too hot (%+.1f dBTP)", "เสียงร้องดังชนเพดาน (%+.1f dBTP)", tp),
                    bi ("There is no headroom left for EQ, compression and the final mix bus.", "ไม่เหลือเฮดรูมสำหรับ EQ คอมเพรสเซอร์ และมิกซ์บัส"),
                    bi ("Leave -6 to -3 dBFS of peak headroom on a vocal stem.", "เหลือเฮดรูมพีคไว้ -6 ถึง -3 dBFS สำหรับไฟล์เสียงร้อง"), 8.f);
    }
    if (lufs > kVocalLufsHi)
    {
        levelBad = true;
        addFinding (rep, Severity::Warn, cLev, bi ("Vocal is very loud (%.1f LUFS)", "เสียงร้องดังมาก (%.1f LUFS)", lufs),
                    bi ("A vocal stem this loud usually means heavy limiting, which leaves little room to mix it with the music.", "ไฟล์เสียงร้องดังขนาดนี้มักผ่านการลิมิตแรง ทำให้เหลือที่ให้มิกซ์กับดนตรีน้อย"),
                    bi ("Turn it down and balance against the backing track by ear or with a vocal-balance tool.", "ลดระดับลงแล้วบาลานซ์กับดนตรีด้วยหูหรือเครื่องมือบาลานซ์เสียงร้อง"), (100.f - sLv) * 0.1f);
    }
    else if (lufs < kVocalLufsLo)
    {
        levelBad = true;
        addFinding (rep, Severity::Warn, cLev, bi ("Vocal level is low (%.1f LUFS)", "ระดับเสียงร้องต่ำ (%.1f LUFS)", lufs),
                    bi ("A quiet recording forces you to add a lot of gain later, which also raises room noise and hiss.", "อัดมาเบาเกินไปทำให้ต้องเพิ่มเกนมากภายหลัง นอยส์และเสียงฟู่จะดังขึ้นตามไปด้วย"),
                    bi ("Normalise or add clip gain so the average sits around -22 to -18 LUFS.", "Normalize หรือเพิ่ม Clip gain ให้ค่าเฉลี่ยอยู่ราว -22 ถึง -18 LUFS"), (100.f - sLv) * 0.1f);
    }
    if (! levelBad)
        addFinding (rep, Severity::Good, cLev, bi ("Level and headroom are healthy", "ระดับเสียงและเฮดรูมกำลังดี"),
                    bi ("Peaks %+.1f dBTP, average %.1f LUFS.", "พีค %+.1f dBTP ค่าเฉลี่ย %.1f LUFS", tp, lufs), {}, 0.f);

    // Consistency
    float sCons = devScore (rangeDev (plr, kVocalPlrLo, kVocalPlrHi), 3.f);
    if (r.lraValid && r.durationSec >= 10.0)
        sCons = 0.5f * sCons + 0.5f * devScore (rangeDev (r.loudnessRange, kVocalLraLo, kVocalLraHi), 3.f);
    bool consBad = false;
    if (r.lraValid && r.loudnessRange > kVocalLraHi)
    {
        consBad = true;
        addFinding (rep, r.loudnessRange > kVocalLraHi + 4.f ? Severity::Issue : Severity::Warn, cCons,
                    bi ("Uneven level - phrases jump (LRA %.1f LU)", "ระดับไม่สม่ำเสมอ บางท่อนดังเบาต่างกัน (LRA %.1f LU)", r.loudnessRange),
                    bi ("Some phrases are much louder or quieter than others, so words get lost in the mix.", "บางวรรคดังหรือเบากว่าวรรคอื่นมาก คำร้องจึงหายไปในมิกซ์"),
                    bi ("Use clip gain on the quiet words, then a compressor (2:1-4:1, 3-6 dB gain reduction) and fader automation.", "ใช้ Clip gain ยกคำที่เบา แล้วใช้คอมเพรสเซอร์ (2:1-4:1 กด 3-6 dB) และ Automation เฟเดอร์"), (100.f - sCons) * 0.2f);
    }
    else if (r.lraValid && r.durationSec >= 10.0 && r.loudnessRange < kVocalLraLo)
    {
        consBad = true;
        addFinding (rep, Severity::Warn, cCons, bi ("Very flat dynamics (LRA %.1f LU)", "ไดนามิกแบนมาก (LRA %.1f LU)", r.loudnessRange),
                    bi ("The vocal may be over-compressed, which sounds lifeless and fatiguing.", "เสียงร้องอาจถูกคอมเพรสมากเกินไป ฟังแล้วไร้ชีวิตและเมื่อยหู"),
                    bi ("Lower the ratio or raise the threshold and keep some natural expression.", "ลด Ratio หรือยก Threshold ให้เหลือความเป็นธรรมชาติของการร้องบ้าง"), (100.f - sCons) * 0.2f);
    }
    if (plr < kVocalPlrLo)
    {
        consBad = true;
        addFinding (rep, Severity::Warn, cCons, bi ("Peaks barely above average (PLR %.1f dB)", "พีคสูงกว่าค่าเฉลี่ยนิดเดียว (PLR %.1f dB)", plr),
                    bi ("A sign of heavy compression or clipping on the vocal.", "เป็นสัญญาณของการคอมเพรสหรือลิมิตเสียงร้องแรงเกินไป"),
                    bi ("Back off the limiter/compressor and keep the natural peaks.", "ลดลิมิตเตอร์/คอมเพรสเซอร์ลง และคงพีคธรรมชาติของเสียงไว้"), (100.f - sCons) * 0.2f);
    }
    else if (plr > kVocalPlrHi)
    {
        consBad = true;
        addFinding (rep, Severity::Warn, cCons, bi ("Spiky peaks (PLR %.1f dB)", "พีคพุ่งแหลม (PLR %.1f dB)", plr),
                    bi ("Single loud consonants or pops stick out far above the average.", "พยัญชนะหรือเสียงลมบางจุดพุ่งสูงกว่าค่าเฉลี่ยมาก"),
                    bi ("Use clip gain or a fast limiter just on the peaks, and de-ess/de-pop.", "ใช้ Clip gain หรือลิมิตเตอร์เร็วเฉพาะที่พีค พร้อม De-ess/De-pop"), (100.f - sCons) * 0.2f);
    }
    if (! consBad)
        addFinding (rep, Severity::Good, cCons, bi ("Vocal level is consistent", "เสียงร้องสม่ำเสมอ"),
                    bi ("Dynamics (PLR %.1f dB) are natural and controlled.", "ไดนามิก (PLR %.1f dB) เป็นธรรมชาติและควบคุมได้ดี", plr), {}, 0.f);

    // Tone
    const float sTone = detail::evalBands (r, rep, true, cTone, 0.25f);

    // Sibilance
    float sSib = 100.f;
    if (r.sibilanceDb > kVocalSibIssue) sSib = std::max (0.f, 55.f - 6.f * (r.sibilanceDb - kVocalSibIssue));
    else if (r.sibilanceDb > kVocalSibWarn) sSib = 85.f - 30.f * (r.sibilanceDb - kVocalSibWarn) / (kVocalSibIssue - kVocalSibWarn);
    if (r.sibilanceDb > kVocalSibWarn)
        addFinding (rep, r.sibilanceDb > kVocalSibIssue ? Severity::Issue : Severity::Warn, cSib,
                    bi ("Harsh 'S' sounds (%+.1f dB)", "เสียง S แรงและบาดหู (%+.1f dB)", r.sibilanceDb),
                    bi ("The loudest 'S'/'T' frames carry as much energy at 5-9 kHz as the body of the voice. They will sting on earbuds, especially after compression and boosting air.",
                        "เสียง S/T ที่ดังที่สุดมีพลังงานย่าน 5-9 kHz พอ ๆ กับตัวเสียงร้อง จะแสบหูบนหูฟัง โดยเฉพาะหลังคอมเพรสและบูสต์ย่านสูง"),
                    bi ("Insert a de-esser (centre ~6-8 kHz, 4-6 dB reduction) BEFORE adding air/brightness, or use a dynamic EQ band. Check mic angle too.",
                        "ใส่ De-esser (กลางที่ ~6-8 kHz ลด 4-6 dB) ก่อนเติมความสว่าง/Air หรือใช้ Dynamic EQ และเช็กมุมไมค์ด้วย"), (100.f - sSib) * 0.15f);
    else
        addFinding (rep, Severity::Good, cSib, bi ("Sibilance is under control (%+.1f dB)", "เสียง S อยู่ในระดับที่ควบคุมได้ (%+.1f dB)", r.sibilanceDb),
                    bi ("No harsh 'S' build-up found.", "ไม่พบเสียง S ที่แรงเกินไป"), {}, 0.f);

    // Cleanliness
    float sClean = 100.f;
    bool cleanBad = false;
    if (r.noiseFloorDb > kVocalNoiseWarn)
    {
        cleanBad = true;
        const bool bad = r.noiseFloorDb > kVocalNoiseIssue;
        sClean -= bad ? 35.f : 18.f;
        addFinding (rep, bad ? Severity::Issue : Severity::Warn, cClean, bi ("Background noise is audible (%.0f dBFS)", "มีนอยส์พื้นหลังชัด (%.0f dBFS)", r.noiseFloorDb),
                    bi ("Even the quietest moments sit well above silence: room noise, hiss, bleed or reverb tails. It gets louder once the vocal is compressed and boosted.",
                        "แม้ช่วงที่เงียบที่สุดก็ยังมีเสียงอยู่สูง: นอยส์ห้อง เสียงฟู่ เสียงรั่วจากเครื่องอื่น หรือหางรีเวิร์บ ซึ่งจะดังขึ้นเมื่อคอมเพรสและบูสต์เสียงร้อง"),
                    bi ("Use a noise gate/expander or spectral noise reduction between phrases, and record away from fans and air-conditioning.",
                        "ใช้ Gate/Expander หรือ Noise reduction ในช่วงระหว่างวรรค และอัดให้ห่างพัดลม/แอร์"), (100.f - sClean) * 0.15f);
    }
    if (r.clipEvents > 0)
    {
        cleanBad = true;
        sClean -= std::min (60.f, 20.f + 15.f * std::log10 ((float) r.clipEvents));
        addFinding (rep, Severity::Issue, cClean, bi ("Digital clipping on the vocal (%d events)", "เสียงร้องแตกดิจิทัล (%d จุด)", r.clipEvents),
                    bi ("Samples hit full scale repeatedly - it sounds crunchy and can't be fixed by turning it down afterwards.", "ตัวอย่างเสียงชนเพดานซ้ำ ๆ ฟังแล้วแตกพร่า และแก้ด้วยการลดเสียงภายหลังไม่ได้"),
                    bi ("Re-record with lower input gain, or use a declipper plugin to repair.", "อัดใหม่โดยลดเกนอินพุต หรือใช้ปลั๊กอิน Declip ซ่อม"), (100.f - sClean) * 0.15f + 5.f);
    }
    if (std::fabs (r.dcOffset) > 0.005f)
    {
        cleanBad = true; sClean -= 15.f;
        addFinding (rep, Severity::Warn, cClean, bi ("DC offset (%.3f)", "พบ DC offset (%.3f)", r.dcOffset),
                    bi ("A constant offset wastes headroom and can click at edits.", "ค่าชดเชยคงที่กินเฮดรูมและอาจเกิดเสียงคลิกตรงจุดตัดต่อ"),
                    bi ("A 20 Hz high-pass removes it.", "High-pass 20 Hz ช่วยลบได้"), 3.f);
    }
    if (! r.mono && r.monoLossDb < -3.f)
    {
        cleanBad = true; sClean -= 15.f;
        addFinding (rep, Severity::Warn, cClean, bi ("Stereo vocal loses level in mono (%.1f dB)", "เสียงร้องสเตอริโอเบาลงเมื่อฟังแบบโมโน (%.1f dB)", r.monoLossDb),
                    bi ("A lead vocal should usually be mono-compatible - check chorus/doubling effects.", "เสียงร้องนำควรเข้ากับโมโนได้ ตรวจเอฟเฟกต์คอรัสหรือการดับเบิล"),
                    bi ("Print the vocal as mono or reduce stereo widening on it.", "บันทึกเสียงร้องเป็นโมโน หรือลดการขยายสเตอริโอ"), 3.f);
    }
    if (! cleanBad)
        addFinding (rep, Severity::Good, cClean, bi ("Clean recording", "เสียงสะอาด"), bi ("Low background noise, no clipping or DC offset.", "นอยส์พื้นหลังต่ำ ไม่มีเสียงแตกหรือ DC offset"), {}, 0.f);
    sClean = std::max (0.f, sClean);

    rep.cats = { { cLev, sLevel }, { cCons, sCons }, { cTone, sTone }, { cSib, sSib }, { cClean, sClean } };
    rep.metrics = {
        { { "Loudness", "ความดัง" }, fmt ("%.1f LUFS", lufs), sevFromScore (sLv) },
        { { "True peak", "True Peak" }, fmt ("%+.1f dBTP", tp), tp > 0 ? Severity::Issue : tp > -1 ? Severity::Warn : Severity::Good },
        { { "Loudness range", "ช่วงความดัง (LRA)" }, r.lraValid ? fmt ("%.1f LU", r.loudnessRange) : "-", (! r.lraValid || r.loudnessRange <= kVocalLraHi) ? Severity::Good : Severity::Warn },
        { { "Peak-to-loudness", "พีคต่อความดัง (PLR)" }, fmt ("%.1f dB", plr), (plr >= kVocalPlrLo && plr <= kVocalPlrHi) ? Severity::Good : Severity::Warn },
        { { "Noise floor", "นอยส์พื้นหลัง" }, fmt ("%.0f dBFS", r.noiseFloorDb), r.noiseFloorDb > kVocalNoiseIssue ? Severity::Issue : r.noiseFloorDb > kVocalNoiseWarn ? Severity::Warn : Severity::Good },
        { { "Sibilance", "เสียง S" }, fmt ("%+.1f dB", r.sibilanceDb), r.sibilanceDb > kVocalSibIssue ? Severity::Issue : r.sibilanceDb > kVocalSibWarn ? Severity::Warn : Severity::Good },
        { { "Clipping", "เสียงแตก" }, fmt ("%d", r.clipEvents), r.clipEvents > 0 ? Severity::Issue : Severity::Good },
        { { "DC offset", "DC offset" }, fmt ("%.3f", r.dcOffset), std::fabs (r.dcOffset) > 0.005f ? Severity::Warn : Severity::Good } };

    detail::finalise (rep, { 0.20f, 0.20f, 0.25f, 0.15f, 0.20f });
    return rep;
}
} // namespace mc
