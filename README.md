# Mix Check AI (VST3 + Standalone)

Checks a whole song, or a vocal only, against mix/master standards. Gives a 0-100 score,
per-category scores, a tonal-balance chart and plain-language explanations (Thai / English)
saying exactly what is missing and how to fix it.

## Use
- Drag & drop (or Browse) a WAV/AIFF/FLAC/MP3/OGG file, or use **Live analysis**:
  insert the plugin on the master track, press Start capture, play the song, press Stop & Analyze.
- Switch **Full Song / Vocal Only** at the top. Pick a standard or leave **AI Auto**.
- Window is resizable (drag the bottom-right corner).

## What is measured
Integrated loudness (ITU-R BS.1770 LUFS), loudness range, true peak (4x oversampled), clipping,
DC offset, stereo correlation + mono compatibility, 7-band tonal balance, sibilance, noise floor.

## About the "AI"
The advisor is an on-device expert model: AI Auto picks the closest style profile from your spectrum,
a weighted scoring model rates each category and ranks problems by impact. It is not a neural network.
All targets are in `Source/Core/Profiles.h` - tune them against your own reference tracks.

## Build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target MixCheckAI_VST3 MixCheckAI_Standalone
Engine test (no JUCE): `cd Tests && g++ -std=c++17 -I../Source/Core core_test.cpp -o core_test && ./core_test`
