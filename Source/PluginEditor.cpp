#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int kW = 1200, kH = 760;   // logical design size; the whole UI scales from this

const Colour cBg1 (0xff08080c), cBg2 (0xff16131d), cBorder (0xff2b2835), cGold (0xffd9b36a), cGold2 (0xfff3dfa6),
             cText (0xffeeeae0), cDim (0xff8f8a9a), cGood (0xff5fd3a0), cWarn (0xfff2b84b), cIssue (0xfff0645a);

Font F (float h, bool bold = false, float kern = 0.f)
{
    auto o = FontOptions (h).withStyle (bold ? "Bold" : "Regular");
    if (kern > 0.f) o = o.withKerningFactor (kern);
    return Font (o);
}

float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.f, 0.f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

Colour sevColour (mc::Severity s) { return s == mc::Severity::Good ? cGood : (s == mc::Severity::Warn ? cWarn : cIssue); }

bool isAudioFile (const String& path)
{
    static const StringArray exts { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a", ".wma", ".caf" };
    return exts.contains (File (path).getFileExtension().toLowerCase());
}

class DarkLaf : public LookAndFeel_V4
{
public:
    DarkLaf()
    {
        setColour (PopupMenu::backgroundColourId, Colour (0xff1b1923));
        setColour (PopupMenu::textColourId, cText);
        setColour (PopupMenu::highlightedBackgroundColourId, cGold.withAlpha (0.25f));
        setColour (PopupMenu::highlightedTextColourId, cGold2);
        setColour (ScrollBar::thumbColourId, cGold.withAlpha (0.45f));
        setColour (ScrollBar::trackColourId, Colours::transparentBlack);
    }
};

//==============================================================================
// Scrollable list of findings (issue / warning / passed cards)
//==============================================================================
class FindingsList : public Component
{
public:
    void setData (const std::vector<mc::Finding>& findings, bool thai, int width)
    {
        items.clear();
        const float pad = 16.f, textW = (float) width - pad - 12.f;
        int y = 0;
        for (auto& f : findings)
        {
            Item it;
            it.sev = f.sev;
            auto u = [thai] (const mc::Bi& b) { return String::fromUTF8 ((thai ? b.th : b.en).c_str()); };
            const String sev = f.sev == mc::Severity::Issue ? (thai ? String::fromUTF8 ("ต้องแก้") : String ("FIX NOW"))
                             : f.sev == mc::Severity::Warn  ? (thai ? String::fromUTF8 ("ควรปรับ") : String ("IMPROVE"))
                                                            : (thai ? String::fromUTF8 ("ผ่าน") : String ("PASSED"));
            it.tag = sev + "   |   " + u (f.cat);
            it.title = makeLayout (u (f.title), F (14.f, true), cText, textW);
            it.detail = makeLayout (u (f.detail), F (12.5f), cDim.brighter (0.25f), textW);
            const bool hasFix = f.fix.en.size() > 0;
            if (hasFix)
            {
                AttributedString as;
                as.setJustification (Justification::topLeft);
                as.append (thai ? String::fromUTF8 ("วิธีแก้:  ") : String ("HOW TO FIX:  "), F (12.5f, true), cGold);
                as.append (u (f.fix), F (12.5f), cGold2.withAlpha (0.92f));
                TextLayout tl; tl.createLayout (as, textW);
                it.fix = std::move (tl);
            }
            it.hasFix = hasFix;
            it.h = (int) (12.f + 14.f + 6.f + it.title.getHeight() + 5.f + it.detail.getHeight()
                          + (hasFix ? 8.f + it.fix.getHeight() : 0.f) + 14.f);
            it.y = y;
            y += it.h + 8;
            items.push_back (std::move (it));
        }
        setSize (width, jmax (y, 1));
        repaint();
    }

    void paint (Graphics& g) override
    {
        const auto clip = g.getClipBounds();
        const float pad = 16.f, textW = (float) getWidth() - pad - 12.f;
        for (auto& it : items)
        {
            if (it.y + it.h < clip.getY() || it.y > clip.getBottom()) continue;
            const auto r = Rectangle<float> (0.f, (float) it.y, (float) getWidth() - 10.f, (float) it.h);
            g.setColour (Colour (0xff1e1c27)); g.fillRoundedRectangle (r, 10.f);
            g.setColour (Colour (0xff2c2935)); g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.f);
            g.setColour (sevColour (it.sev)); g.fillRoundedRectangle (r.getX() + 1.f, r.getY() + 12.f, 3.f, r.getHeight() - 24.f, 1.5f);

            float y = r.getY() + 12.f;
            g.setColour (sevColour (it.sev)); g.setFont (F (10.5f, true, 0.1f));
            g.drawText (it.tag, Rectangle<float> (pad, y, textW, 14.f), Justification::centredLeft, true);
            y += 20.f;
            it.title.draw (g, Rectangle<float> (pad, y, textW, it.title.getHeight() + 2.f));
            y += it.title.getHeight() + 5.f;
            it.detail.draw (g, Rectangle<float> (pad, y, textW, it.detail.getHeight() + 2.f));
            y += it.detail.getHeight() + 8.f;
            if (it.hasFix) it.fix.draw (g, Rectangle<float> (pad, y, textW, it.fix.getHeight() + 2.f));
        }
    }

private:
    struct Item
    {
        mc::Severity sev = mc::Severity::Good;
        String tag;
        TextLayout title, detail, fix;
        bool hasFix = false;
        int y = 0, h = 0;
    };
    std::vector<Item> items;

    static TextLayout makeLayout (const String& text, const Font& font, Colour col, float width)
    {
        AttributedString as;
        as.setJustification (Justification::topLeft);
        as.append (text, font, col);
        TextLayout tl;
        tl.createLayout (as, width);
        return tl;
    }
};
} // namespace

//==============================================================================
// The whole UI lives in one fixed-size component that the editor scales.
//==============================================================================
class MixCheckEditor::Content : public Component, public FileDragAndDropTarget, private Timer
{
public:
    explicit Content (MixCheckProcessor& p) : processor (p)
    {
        setLookAndFeel (&laf);
        setOpaque (true);
        setSize (kW, kH);

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (7);
        viewport.setBounds (findR.getX() + 12, findR.getY() + 44, findR.getWidth() - 20, findR.getHeight() - 56);
        addAndMakeVisible (viewport);

        hits = { { "th", { 1068, 26, 50, 28 } }, { "en", { 1118, 26, 54, 28 } },
                 { "mode0", { 28, 88, 150, 40 } }, { "mode1", { 178, 88, 150, 40 } },
                 { "profile", { 340, 88, 310, 40 } },
                 { "browse", { dropR.getX() + 16, dropR.getY() + 130, dropR.getWidth() - 32, 34 } },
                 { "live", { liveR.getX() + 16, liveR.getY() + 38, liveR.getWidth() - 32, 34 } } };

        animBand.fill (-40.f);
        rebuild();
        startTimerHz (30);
    }

    ~Content() override { setLookAndFeel (nullptr); }

    // ------------------------------------------------------------------ paint
    void paint (Graphics& g) override
    {
        paintBackground (g);
        paintHeader (g);
        paintControls (g);
        paintDrop (g);
        paintLive (g);
        paintMetrics (g);
        paintScore (g);
        paintTonal (g);
        paintAdvisor (g);
        paintFindings (g);
    }

    // ------------------------------------------------------------------ input
    void mouseMove (const MouseEvent& e) override { setHover (hitAt (e.getPosition())); }
    void mouseExit (const MouseEvent&) override { setHover ({}); }

    void mouseDown (const MouseEvent& e) override
    {
        const auto id = hitAt (e.getPosition());
        if (id == "th") processor.setChoice ("lang", 0);
        else if (id == "en") processor.setChoice ("lang", 1);
        else if (id == "mode0") processor.setChoice ("mode", 0);
        else if (id == "mode1") processor.setChoice ("mode", 1);
        else if (id == "profile" && ! vocalMode()) showProfileMenu();
        else if (id == "browse")
        {
            if (processor.isAnalyzing()) processor.cancelAnalysis();
            else openFileChooser();
        }
        else if (id == "live" && ! processor.isAnalyzing())
        {
            if (processor.isCapturing()) processor.stopCaptureAndAnalyze();
            else processor.startCapture();
        }
    }

    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& f : files) if (isAudioFile (f)) return true;
        return false;
    }
    void fileDragEnter (const StringArray&, int, int) override { dragOver = true; }
    void fileDragExit (const StringArray&) override { dragOver = false; }
    void filesDropped (const StringArray& files, int, int) override
    {
        dragOver = false;
        for (auto& f : files)
            if (isAudioFile (f)) { processor.analyzeFile (File (f)); break; }
    }

private:
    MixCheckProcessor& processor;
    DarkLaf laf;
    FindingsList list;
    Viewport viewport;
    std::unique_ptr<FileChooser> chooser;

    // layout (logical coordinates)
    const Rectangle<int> dropR { 28, 144, 300, 176 }, liveR { 28, 332, 300, 118 }, metricsR { 28, 462, 300, 270 },
                         scoreR { 340, 144, 410, 262 }, tonalR { 340, 418, 410, 314 },
                         advR { 762, 144, 410, 150 }, findR { 762, 306, 410, 426 };
    struct Hit { String id; Rectangle<int> r; };
    std::vector<Hit> hits;
    String hover;
    bool dragOver = false;

    // data
    bool hasResult = false, thai = true;
    mc::Result result;
    mc::Report report;
    String sourceName;
    int seenVersion = -1, seenMode = -1, seenProfile = -1, seenLang = -1;

    // animation state
    float animScore = 0.f, meter = -100.f, spin = 0.f;
    std::array<float, 6> animCat {};
    std::array<float, mc::kBands> animBand {};

    // ------------------------------------------------------------------ helpers
    String T (const char* en, const char* th) const { return String::fromUTF8 (thai ? th : en); }
    String U (const mc::Bi& b) const { return String::fromUTF8 ((thai ? b.th : b.en).c_str()); }
    bool vocalMode() const { return processor.getChoice ("mode") == 1; }

    String hitAt (Point<int> p) const
    {
        for (auto& h : hits) if (h.r.contains (p)) return h.id;
        return {};
    }
    void setHover (const String& id)
    {
        if (id == hover) return;
        hover = id;
        setMouseCursor (hover.isNotEmpty() ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    }

    void openFileChooser()
    {
        chooser = std::make_unique<FileChooser> (T ("Choose an audio file", "เลือกไฟล์เสียง"), File(),
                                                 "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.wma;*.caf");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) processor.analyzeFile (f);
                              });
    }

    void showProfileMenu()
    {
        PopupMenu m;
        const int cur = processor.getChoice ("profile");
        m.addItem (1, T ("AI Auto (best match)", "AI Auto (เลือกให้อัตโนมัติ)"), true, cur == 0);
        m.addSeparator();
        int id = 2;
        for (auto& p : mc::profiles())
        {
            m.addItem (id, String::fromUTF8 (thai ? p.th : p.en), true, cur == id - 1);
            ++id;
        }
        m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (hits[4].r)),
                         [this] (int r) { if (r > 0) processor.setChoice ("profile", r - 1); });
    }

    void rebuild()
    {
        thai = processor.getChoice ("lang") == 0;
        const auto snap = processor.getSnapshot();
        hasResult = snap.has;
        if (hasResult)
        {
            result = snap.result;
            sourceName = snap.name;
            report = mc::evaluate (result, vocalMode(), processor.getChoice ("profile"));
        }
        else report = {};
        list.setData (report.findings, thai, viewport.getWidth() - 10);
        viewport.setViewPosition (0, 0);
        viewport.setVisible (hasResult && ! report.findings.empty());
    }

    void timerCallback() override
    {
        const int v = processor.getResultVersion(), m = processor.getChoice ("mode"),
                  pr = processor.getChoice ("profile"), lg = processor.getChoice ("lang");
        if (v != seenVersion || m != seenMode || pr != seenProfile || lg != seenLang)
        {
            seenVersion = v; seenMode = m; seenProfile = pr; seenLang = lg;
            rebuild();
        }

        auto ease = [] (float& x, float t) { x += (t - x) * 0.14f; };
        ease (animScore, hasResult ? report.overall : 0.f);
        for (size_t i = 0; i < animCat.size(); ++i)
            ease (animCat[i], (hasResult && i < report.cats.size()) ? report.cats[i].score : 0.f);
        for (size_t i = 0; i < animBand.size(); ++i)
            ease (animBand[i], hasResult ? result.bandDb[i] : -40.f);
        const float lv = processor.getLevelDb();
        meter = lv > meter ? lv : meter - 1.6f;
        spin += 0.06f;
        repaint();
    }

    // ------------------------------------------------------------------ drawing blocks
    void paintBackground (Graphics& g)
    {
        g.setGradientFill (ColourGradient::vertical (cBg1, 0.f, cBg2, (float) kH));
        g.fillAll();
        g.setGradientFill (ColourGradient (cGold.withAlpha (0.10f), 120.f, -40.f, Colours::transparentBlack, 120.f, 420.f, true));
        g.fillRect (0, 0, kW, kH);
        g.setGradientFill (ColourGradient (Colour (0xff6a4fd8).withAlpha (0.07f), 1100.f, 760.f, Colours::transparentBlack, 1100.f, 360.f, true));
        g.fillRect (0, 0, kW, kH);
    }

    void card (Graphics& g, Rectangle<int> r, const String& title, const String& note = {})
    {
        const auto rf = r.toFloat();
        g.setColour (Colour (0x55000000)); g.fillRoundedRectangle (rf.translated (0.f, 5.f).expanded (1.f), 16.f);
        g.setGradientFill (ColourGradient::vertical (Colour (0xff1c1a24), rf.getY(), Colour (0xff141219), rf.getBottom()));
        g.fillRoundedRectangle (rf, 14.f);
        g.setColour (cBorder); g.drawRoundedRectangle (rf.reduced (0.5f), 14.f, 1.f);
        g.setColour (cGold.withAlpha (0.28f)); g.fillRect (rf.getX() + 20.f, rf.getY(), rf.getWidth() - 40.f, 1.f);
        g.setColour (cGold); g.setFont (F (11.5f, true, 0.16f));
        g.drawText (title.toUpperCase(), r.getX() + 18, r.getY() + 14, r.getWidth() - 36, 16, Justification::centredLeft, true);
        if (note.isNotEmpty())
        {
            g.setColour (cDim); g.setFont (F (11.5f));
            g.drawText (note, r.getX() + 18, r.getY() + 14, r.getWidth() - 36, 16, Justification::centredRight, true);
        }
    }

    void button (Graphics& g, Rectangle<int> r, const String& text, bool primary, bool hot, bool enabled = true)
    {
        const auto rf = r.toFloat();
        const float a = enabled ? 1.f : 0.4f;
        if (primary)
        {
            g.setGradientFill (ColourGradient::vertical (cGold2.withAlpha (a), rf.getY(), cGold.darker (0.15f).withAlpha (a), rf.getBottom()));
            g.fillRoundedRectangle (rf, 9.f);
            if (hot && enabled) { g.setColour (Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (rf, 9.f); }
            g.setColour (Colour (0xff1a1409).withAlpha (a));
        }
        else
        {
            if (hot && enabled) { g.setColour (cGold.withAlpha (0.12f)); g.fillRoundedRectangle (rf, 9.f); }
            g.setColour (cGold.withAlpha (0.6f * a)); g.drawRoundedRectangle (rf.reduced (0.5f), 9.f, 1.2f);
            g.setColour (cGold.withAlpha (a));
        }
        g.setFont (F (12.5f, true, 0.1f));
        g.drawText (text, r, Justification::centred, true);
    }

    void paintHeader (Graphics& g)
    {
        // logo mark
        const auto lr = Rectangle<float> (28.f, 16.f, 46.f, 46.f);
        g.setGradientFill (ColourGradient::vertical (cGold2, lr.getY(), cGold.darker (0.2f), lr.getBottom()));
        g.fillRoundedRectangle (lr, 12.f);
        g.setColour (Colour (0xff15110a)); g.fillRoundedRectangle (lr.reduced (2.f), 10.f);
        const float hs[5] = { 0.35f, 0.62f, 0.95f, 0.55f, 0.30f };
        for (int i = 0; i < 5; ++i)
        {
            const float bh = 26.f * hs[i], bx = lr.getX() + 9.f + (float) i * 6.4f;
            g.setGradientFill (ColourGradient::vertical (cGold2, lr.getCentreY() - bh / 2, cGold, lr.getCentreY() + bh / 2));
            g.fillRoundedRectangle (bx, lr.getCentreY() - bh / 2, 3.6f, bh, 1.8f);
        }

        const auto f = F (27.f, true, 0.14f);
        g.setColour (cText); g.setFont (f);
        g.drawText ("MIX CHECK", 90, 12, 260, 34, Justification::centredLeft, false);
        const float w = textWidth (f, "MIX CHECK");
        const auto pill = Rectangle<float> (90.f + w + 12.f, 19.f, 38.f, 22.f);
        g.setGradientFill (ColourGradient::vertical (cGold2, pill.getY(), cGold, pill.getBottom()));
        g.fillRoundedRectangle (pill, 7.f);
        g.setColour (Colour (0xff1a1409)); g.setFont (F (13.f, true, 0.1f));
        g.drawText ("AI", pill.toNearestInt(), Justification::centred, false);
        g.setColour (cDim); g.setFont (F (12.5f));
        g.drawText (T ("Mix & Master standard analyzer", "ตรวจมาตรฐานมิกซ์และมาสเตอร์ พร้อมคำอธิบายว่าขาดอะไร"), 90, 46, 700, 18, Justification::centredLeft, true);

        // language switch
        segment (g, hits[0].r, "ไทย", thai, hover == "th");
        segment (g, hits[1].r, "EN", ! thai, hover == "en");
    }

    void segment (Graphics& g, Rectangle<int> r, const String& text, bool active, bool hot)
    {
        const auto rf = r.toFloat();
        if (active)
        {
            g.setGradientFill (ColourGradient::vertical (cGold2, rf.getY(), cGold, rf.getBottom()));
            g.fillRoundedRectangle (rf, 8.f);
            g.setColour (Colour (0xff1a1409));
        }
        else
        {
            g.setColour (hot ? cGold.withAlpha (0.14f) : Colours::transparentBlack); g.fillRoundedRectangle (rf, 8.f);
            g.setColour (cBorder); g.drawRoundedRectangle (rf.reduced (0.5f), 8.f, 1.f);
            g.setColour (cDim.brighter (0.3f));
        }
        g.setFont (F (12.5f, true, 0.08f));
        g.drawText (text, r, Justification::centred, false);
    }

    void paintControls (Graphics& g)
    {
        // mode switch (one pill, two halves)
        g.setColour (Colour (0xff14121a)); g.fillRoundedRectangle (Rectangle<float> (28.f, 88.f, 300.f, 40.f), 11.f);
        g.setColour (cBorder); g.drawRoundedRectangle (Rectangle<float> (28.5f, 88.5f, 299.f, 39.f), 11.f, 1.f);
        segment (g, hits[2].r.reduced (4), T ("FULL SONG", "ทั้งเพลง"), ! vocalMode(), hover == "mode0");
        segment (g, hits[3].r.reduced (4), T ("VOCAL ONLY", "เฉพาะเสียงร้อง"), vocalMode(), hover == "mode1");

        // standard picker
        const auto pr = hits[4].r;
        g.setColour (Colour (0xff14121a)); g.fillRoundedRectangle (pr.toFloat(), 11.f);
        g.setColour (hover == "profile" && ! vocalMode() ? cGold.withAlpha (0.7f) : cBorder); g.drawRoundedRectangle (pr.toFloat().reduced (0.5f), 11.f, 1.f);
        g.setColour (cDim); g.setFont (F (10.5f, true, 0.14f));
        g.drawText (T ("STANDARD", "มาตรฐานที่ใช้เทียบ"), pr.getX() + 16, pr.getY() + 5, 200, 14, Justification::centredLeft, true);
        String val;
        if (vocalMode()) val = T ("Vocal standard", "มาตรฐานเสียงร้อง");
        else if (hasResult && report.autoDetected) val = T ("AI Auto  >  ", "AI Auto  >  ") + U (report.profileName);
        else if (hasResult) val = U (report.profileName);
        else val = processor.getChoice ("profile") == 0 ? T ("AI Auto (best match)", "AI Auto (เลือกให้อัตโนมัติ)") : String::fromUTF8 (thai ? mc::profiles()[(size_t) processor.getChoice ("profile") - 1].th : mc::profiles()[(size_t) processor.getChoice ("profile") - 1].en);
        g.setColour (vocalMode() ? cDim : cText); g.setFont (F (14.f, true));
        g.drawText (val, pr.getX() + 16, pr.getY() + 18, pr.getWidth() - 50, 20, Justification::centredLeft, true);
        if (! vocalMode())
        {
            Path chev; const float cx = (float) pr.getRight() - 24.f, cy = (float) pr.getCentreY();
            chev.startNewSubPath (cx - 5, cy - 2); chev.lineTo (cx, cy + 3); chev.lineTo (cx + 5, cy - 2);
            g.setColour (cGold); g.strokePath (chev, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // source info / error
        const auto err = processor.getLastError();
        g.setFont (F (12.5f));
        if (err.isNotEmpty() && ! processor.isAnalyzing())
        {
            g.setColour (cIssue);
            g.drawText (err.fromFirstOccurrenceOf (" / ", false, false).isNotEmpty() ? (thai ? err.fromFirstOccurrenceOf (" / ", false, false) : err.upToFirstOccurrenceOf (" / ", false, false)) : err,
                        664, 88, 508, 40, Justification::centredRight, true);
        }
        else if (hasResult)
        {
            const int secs = (int) result.durationSec;
            g.setColour (cDim.brighter (0.4f));
            g.drawText (sourceName + "   |   " + String::formatted ("%d:%02d", secs / 60, secs % 60) + "   |   "
                            + String (result.sampleRate / 1000.0, 1) + " kHz   |   " + (result.mono ? T ("mono", "โมโน") : T ("stereo", "สเตอริโอ")),
                        664, 88, 508, 40, Justification::centredRight, true);
        }
    }

    void paintDrop (Graphics& g)
    {
        card (g, dropR, vocalMode() ? T ("Vocal file", "ไฟล์เสียงร้อง") : T ("Song file", "ไฟล์เพลง"));
        const auto zone = Rectangle<float> ((float) dropR.getX() + 16.f, (float) dropR.getY() + 38.f, (float) dropR.getWidth() - 32.f, 84.f);
        if (dragOver) { g.setColour (cGold.withAlpha (0.10f)); g.fillRoundedRectangle (zone, 12.f); }
        Path outline; outline.addRoundedRectangle (zone, 12.f);
        Path dashed; const float dashes[] = { 6.f, 5.f };
        PathStrokeType (1.4f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (dragOver ? cGold2 : cGold.withAlpha (0.5f)); g.fillPath (dashed);

        const bool busy = processor.isAnalyzing();
        if (busy)
        {
            const float p = processor.getProgress();
            g.setColour (cGold); g.setFont (F (13.f, true, 0.12f));
            g.drawText (T ("ANALYZING", "กำลังวิเคราะห์") + "   " + String (juce::roundToInt (p * 100.f)) + "%",
                        zone.withTrimmedTop (14.f).withHeight (22.f).toNearestInt(), Justification::centred, false);
            const auto bar = Rectangle<float> (zone.getX() + 24.f, zone.getY() + 50.f, zone.getWidth() - 48.f, 6.f);
            g.setColour (Colour (0xff24212d)); g.fillRoundedRectangle (bar, 3.f);
            g.setGradientFill (ColourGradient::horizontal (cGold, bar.getX(), cGold2, bar.getRight()));
            g.fillRoundedRectangle (bar.withWidth (jmax (6.f, bar.getWidth() * p)), 3.f);
        }
        else
        {
            const float cx = zone.getCentreX(), y = zone.getY() + 10.f;
            Path p;
            p.startNewSubPath (cx, y); p.lineTo (cx, y + 16.f);
            p.startNewSubPath (cx - 7.f, y + 9.f); p.lineTo (cx, y + 16.f); p.lineTo (cx + 7.f, y + 9.f);
            p.startNewSubPath (cx - 12.f, y + 14.f); p.lineTo (cx - 12.f, y + 22.f); p.lineTo (cx + 12.f, y + 22.f); p.lineTo (cx + 12.f, y + 14.f);
            g.setColour (cGold); g.strokePath (p, PathStrokeType (2.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (cText); g.setFont (F (13.5f, true));
            g.drawText (vocalMode() ? T ("Drop your vocal file here", "วางไฟล์เสียงร้องที่นี่") : T ("Drop your song here", "วางไฟล์เพลงที่นี่"),
                        zone.withTrimmedTop (38.f).withHeight (20.f).toNearestInt(), Justification::centred, true);
            g.setColour (cDim); g.setFont (F (11.f));
            g.drawText ("WAV  |  AIFF  |  FLAC  |  MP3  |  OGG", zone.withTrimmedTop (58.f).withHeight (16.f).toNearestInt(), Justification::centred, true);
        }
        button (g, hits[5].r, busy ? T ("CANCEL", "ยกเลิก") : T ("BROWSE FILE", "เลือกไฟล์"), false, hover == "browse");
    }

    void paintLive (Graphics& g)
    {
        card (g, liveR, T ("Live analysis", "วิเคราะห์สด"));
        const bool cap = processor.isCapturing(), busy = processor.isAnalyzing();
        button (g, hits[6].r, cap ? T ("STOP & ANALYZE", "หยุดและวิเคราะห์") : T ("START CAPTURE", "เริ่มบันทึกสัญญาณ"), cap, hover == "live", ! busy);

        g.setFont (F (11.5f));
        if (cap)
        {
            const int s = (int) processor.getCaptureSeconds();
            g.setColour (cIssue.withAlpha (0.65f + 0.35f * std::sin (spin * 2.f)));
            g.fillEllipse ((float) liveR.getX() + 18.f, (float) liveR.getY() + 84.f, 8.f, 8.f);
            g.setColour (cText);
            g.drawText ("REC  " + String::formatted ("%d:%02d", s / 60, s % 60) + "   " + T ("play your song now", "กดเล่นเพลงได้เลย"),
                        liveR.getX() + 32, liveR.getY() + 80, liveR.getWidth() - 50, 16, Justification::centredLeft, true);
        }
        else
        {
            g.setColour (cDim);
            g.drawText (T ("Insert on the master track, then press play", "ใส่ที่แทร็ก Master แล้วกดเล่นเพลง"),
                        liveR.getX() + 18, liveR.getY() + 80, liveR.getWidth() - 36, 16, Justification::centredLeft, true);
        }
        const auto bar = Rectangle<float> ((float) liveR.getX() + 18.f, (float) liveR.getY() + 102.f, (float) liveR.getWidth() - 36.f, 5.f);
        g.setColour (Colour (0xff24212d)); g.fillRoundedRectangle (bar, 2.5f);
        const float lvl = jlimit (0.f, 1.f, (jmax (meter, -60.f) + 60.f) / 60.f);
        if (lvl > 0.f)
        {
            g.setGradientFill (ColourGradient::horizontal (cGood, bar.getX(), meter > -3.f ? cIssue : cGold, bar.getRight()));
            g.fillRoundedRectangle (bar.withWidth (jmax (4.f, bar.getWidth() * lvl)), 2.5f);
        }
    }

    void paintMetrics (Graphics& g)
    {
        card (g, metricsR, T ("Key metrics", "ค่าสำคัญ"));
        float y = (float) metricsR.getY() + 42.f;
        if (! hasResult)
        {
            g.setColour (cDim); g.setFont (F (12.5f));
            g.drawFittedText (T ("Loudness, true peak, dynamics, stereo and noise measurements appear here.",
                                 "ค่าความดัง True Peak ไดนามิก สเตอริโอ และนอยส์ จะแสดงที่นี่"),
                              metricsR.reduced (22, 50), Justification::centredTop, 4);
            return;
        }
        for (auto& m : report.metrics)
        {
            g.setColour (sevColour (m.sev)); g.fillEllipse ((float) metricsR.getX() + 20.f, y + 9.f, 8.f, 8.f);
            g.setColour (cDim.brighter (0.35f)); g.setFont (F (12.5f));
            g.drawText (U (m.name), metricsR.getX() + 38, (int) y, 170, 26, Justification::centredLeft, true);
            g.setColour (cText); g.setFont (F (13.5f, true));
            g.drawText (String::fromUTF8 (m.value.c_str()), metricsR.getX() + 150, (int) y, metricsR.getWidth() - 170, 26, Justification::centredRight, true);
            y += 27.f;
        }
    }

    void paintScore (Graphics& g)
    {
        card (g, scoreR, T ("Overall score", "คะแนนรวม"));
        const float cx = (float) scoreR.getX() + 108.f, cy = (float) scoreR.getY() + 128.f, rad = 70.f;
        const float a0 = -0.75f * MathConstants<float>::pi, sweep = 1.5f * MathConstants<float>::pi;

        Path track; track.addCentredArc (cx, cy, rad, rad, 0.f, a0, a0 + sweep, true);
        g.setColour (Colour (0xff25222e)); g.strokePath (track, PathStrokeType (11.f, PathStrokeType::curved, PathStrokeType::rounded));

        const float frac = jlimit (0.f, 1.f, animScore / 100.f);
        const Colour main = ! hasResult ? cDim : (animScore >= 75.f ? cGold : (animScore >= 55.f ? cWarn : cIssue));
        if (frac > 0.005f)
        {
            Path val; val.addCentredArc (cx, cy, rad, rad, 0.f, a0, a0 + sweep * frac, true);
            g.setGradientFill (ColourGradient (main.brighter (0.5f), cx - rad, cy - rad, main.darker (0.2f), cx + rad, cy + rad, false));
            g.strokePath (val, PathStrokeType (11.f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        if (processor.isAnalyzing())
        {
            g.setColour (cGold); g.setFont (F (12.f, true, 0.14f));
            g.drawText (T ("ANALYZING", "กำลังวิเคราะห์"), Rectangle<float> (cx - 60.f, cy - 12.f, 120.f, 24.f).toNearestInt(), Justification::centred, false);
        }
        else if (! hasResult)
        {
            g.setColour (cDim); g.setFont (F (42.f, true));
            g.drawText ("--", Rectangle<float> (cx - 60.f, cy - 36.f, 120.f, 56.f).toNearestInt(), Justification::centred, false);
        }
        else
        {
            g.setColour (cText); g.setFont (F (46.f, true));
            g.drawText (String (juce::roundToInt (animScore)), Rectangle<float> (cx - 60.f, cy - 38.f, 120.f, 52.f).toNearestInt(), Justification::centred, false);
            g.setColour (cDim); g.setFont (F (12.f));
            g.drawText ("/ 100", Rectangle<float> (cx - 40.f, cy + 12.f, 80.f, 16.f).toNearestInt(), Justification::centred, false);
            const auto chip = Rectangle<float> (cx - 21.f, cy + 32.f, 42.f, 22.f);
            g.setColour (main.withAlpha (0.18f)); g.fillRoundedRectangle (chip, 7.f);
            g.setColour (main); g.setFont (F (14.f, true));
            g.drawText (String (report.grade), chip.toNearestInt(), Justification::centred, false);
        }

        // verdict under the ring
        g.setColour (hasResult ? main : cDim); g.setFont (F (13.f, true, 0.06f));
        g.drawFittedText (hasResult ? U (report.verdict) : T ("Waiting for a song", "รอไฟล์เพลง"),
                          Rectangle<int> (scoreR.getX() + 14, scoreR.getY() + 212, 200, 40), Justification::centred, 2);

        // category bars
        float y = (float) scoreR.getY() + 46.f;
        const float bx = (float) scoreR.getX() + 232.f, bw = (float) scoreR.getWidth() - 232.f - 20.f;
        const size_t n = hasResult ? report.cats.size() : 0;
        for (size_t i = 0; i < n; ++i)
        {
            const float s = animCat[i];
            const Colour c = sevColour (mc::sevFromScore (report.cats[i].score));
            g.setColour (cDim.brighter (0.4f)); g.setFont (F (11.5f));
            g.drawText (U (report.cats[i].name), (int) bx, (int) y, (int) bw - 34, 15, Justification::centredLeft, true);
            g.setColour (cText); g.setFont (F (12.f, true));
            g.drawText (String (juce::roundToInt (s)), (int) (bx + bw - 34.f), (int) y, 34, 15, Justification::centredRight, false);
            const auto bar = Rectangle<float> (bx, y + 19.f, bw, 6.f);
            g.setColour (Colour (0xff25222e)); g.fillRoundedRectangle (bar, 3.f);
            g.setColour (c); g.fillRoundedRectangle (bar.withWidth (jmax (4.f, bw * jlimit (0.f, 1.f, s / 100.f))), 3.f);
            y += 34.f;
        }
        if (n == 0)
        {
            g.setColour (cDim); g.setFont (F (12.f));
            g.drawFittedText (T ("Category scores appear after analysis.", "คะแนนแยกหมวดจะแสดงหลังวิเคราะห์"),
                              Rectangle<int> ((int) bx, scoreR.getY() + 90, (int) bw, 50), Justification::centredLeft, 3);
        }
    }

    void paintTonal (Graphics& g)
    {
        card (g, tonalR, T ("Tonal balance", "สมดุลความถี่"), T ("gold zone = target", "โซนทอง = เป้าหมาย"));
        static const char* en[] = { "SUB", "BASS", "LOW-MID", "MID", "PRES.", "BRIGHT", "AIR" };
        static const char* th[] = { "ซับ", "เบส", "โลว์-มิด", "มิด", "ชัด", "สว่าง", "แอร์" };
        static const char* hz[] = { "20-60", "60-150", "150-400", "0.4-2k", "2-5k", "5-10k", "10-20k" };

        const float x0 = (float) tonalR.getX() + 24.f, w = (float) tonalR.getWidth() - 42.f;
        const float top = (float) tonalR.getY() + 62.f, h = 188.f, colW = w / (float) mc::kBands;
        const float dbTop = -3.f, dbBot = -40.f;
        auto yOf = [&] (float db) { return top + (dbTop - jlimit (dbBot, dbTop, db)) / (dbTop - dbBot) * h; };

        g.setFont (F (9.5f));
        for (float d : { -10.f, -20.f, -30.f })
        {
            g.setColour (Colour (0x22ffffff)); g.fillRect (x0, yOf (d), w, 1.f);
            g.setColour (cDim.withAlpha (0.7f));
            g.drawText (String ((int) d), (int) (tonalR.getX() + 2), (int) yOf (d) - 6, 20, 12, Justification::centredRight, false);
        }

        for (int b = 0; b < mc::kBands; ++b)
        {
            const float cx = x0 + ((float) b + 0.5f) * colW;
            if (hasResult)
            {
                const float t = report.target[(size_t) b], tol = report.tol[(size_t) b];
                const auto zone = Rectangle<float> (cx - 20.f, yOf (t + tol), 40.f, yOf (t - tol) - yOf (t + tol));
                g.setColour (cGold.withAlpha (0.13f)); g.fillRoundedRectangle (zone, 5.f);
                g.setColour (cGold.withAlpha (0.55f)); g.drawRoundedRectangle (zone, 5.f, 1.f);
                g.fillRect (cx - 20.f, yOf (t) - 0.5f, 40.f, 1.f);

                const float v = animBand[(size_t) b];
                float dev = std::fabs (result.bandDb[(size_t) b] - t);
                if (vocalMode() && b == 0 && result.bandDb[0] < t) dev = 0.f;
                const auto c = dev <= tol ? cGood : (dev <= 2.f * tol ? cWarn : cIssue);
                const auto bar = Rectangle<float> (cx - 11.f, yOf (v), 22.f, top + h - yOf (v));
                g.setGradientFill (ColourGradient::vertical (c.brighter (0.2f), bar.getY(), c.darker (0.5f), bar.getBottom()));
                g.fillRoundedRectangle (bar, 5.f);
                g.setColour (cText); g.setFont (F (11.f, true));
                g.drawText (String (juce::roundToInt (result.bandDb[(size_t) b])), (int) cx - 22, (int) bar.getY() - 17, 44, 14, Justification::centred, false);
            }
            g.setColour (cText.withAlpha (0.9f)); g.setFont (F (11.f, true));
            g.drawText (String::fromUTF8 (thai ? th[b] : en[b]), (int) (cx - colW / 2), (int) (top + h + 8), (int) colW, 14, Justification::centred, true);
            g.setColour (cDim); g.setFont (F (10.f));
            g.drawText (hz[b], (int) (cx - colW / 2), (int) (top + h + 24), (int) colW, 13, Justification::centred, false);
        }
        if (! hasResult)
        {
            g.setColour (cDim); g.setFont (F (12.5f));
            g.drawText (T ("Your frequency balance versus the target will be drawn here.", "สมดุลความถี่ของเพลงเทียบกับเป้าหมายจะแสดงที่นี่"),
                        tonalR.getX() + 20, (int) top + 70, tonalR.getWidth() - 40, 20, Justification::centred, true);
        }
    }

    void paintAdvisor (Graphics& g)
    {
        String note;
        if (hasResult) note = report.autoDetected ? T ("AI Auto: ", "AI Auto: ") + U (report.profileName) : U (report.profileName);
        card (g, advR, "     " + T ("AI advisor", "AI ผู้ช่วยวิเคราะห์"), note);
        Path star; star.addStar (Point<float> ((float) advR.getX() + 26.f, (float) advR.getY() + 22.f), 4, 2.2f, 7.f, 0.f);
        g.setColour (cGold2); g.fillPath (star);

        g.setColour (hasResult ? cText.withAlpha (0.92f) : cDim); g.setFont (F (13.f));
        const auto area = Rectangle<int> (advR.getX() + 18, advR.getY() + 42, advR.getWidth() - 36, advR.getHeight() - 52);
        if (processor.isAnalyzing())
            g.drawFittedText (T ("Listening to every second of your track...", "กำลังฟังเพลงของคุณทุกวินาที..."), area, Justification::topLeft, 3);
        else if (hasResult)
            g.drawFittedText (U (report.summary), area, Justification::topLeft, 6, 0.9f);
        else
            g.drawFittedText (T ("Drop a song (or switch to Vocal Only and drop a vocal). I will score it against the standard, show what is missing and tell you exactly where to fix it.",
                                 "วางไฟล์เพลง (หรือสลับเป็นเฉพาะเสียงร้องแล้ววางไฟล์เสียงร้อง) ระบบจะให้คะแนนเทียบมาตรฐาน บอกว่าขาดอะไร และชี้จุดที่ต้องแก้ให้ตรงจุด"),
                              area, Justification::topLeft, 6, 0.9f);
    }

    void paintFindings (Graphics& g)
    {
        int toFix = 0;
        for (auto& f : report.findings) if (f.sev != mc::Severity::Good) ++toFix;
        String note;
        if (hasResult) note = toFix > 0 ? String (toFix) + T (" to fix", " จุดที่ควรแก้") : T ("all clear", "ผ่านทั้งหมด");
        card (g, findR, T ("What to fix", "สิ่งที่ควรแก้"), note);
        if (! hasResult)
        {
            g.setColour (cDim); g.setFont (F (12.5f));
            g.drawFittedText (T ("Each problem is explained in plain language with a specific fix: which frequency, how many dB, which tool to use.",
                                 "ทุกปัญหาจะมีคำอธิบายภาษาง่าย ๆ พร้อมวิธีแก้ที่ชัดเจน: ย่านความถี่ไหน กี่ dB ใช้เครื่องมืออะไร"),
                              findR.reduced (30, 70), Justification::centredTop, 6);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};

//==============================================================================
MixCheckEditor::MixCheckEditor (MixCheckProcessor& p) : AudioProcessorEditor (&p)
{
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (612, 388, 2400, 1520);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1020, 646);
}

MixCheckEditor::~MixCheckEditor() = default;

void MixCheckEditor::paint (Graphics& g) { g.fillAll (cBg1); }

void MixCheckEditor::resized()
{
    const float s = jmin ((float) getWidth() / (float) kW, (float) getHeight() / (float) kH);
    content->setBounds (0, 0, kW, kH);
    content->setTransform (AffineTransform::scale (s));
}
