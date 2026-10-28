#pragma once
// ============================================================
// uitour_policy.h -- when the first-run Edit UI tour comes up, and how "Show me" pulses the outlines (1.6.1).
//
// No Windows, no client: tests/uitour_policy_test.cpp checks it. uitour.cpp applies it.
//
// The tour shows once per PC: HKCU\Software\SlumsMS\Overlay\tourSeen holds the tour version the player dismissed.
// Raising kVersion shows a redesigned tour once more. config.ini [clienthax] uitour=false never shows it (QA).
// ============================================================

namespace UiTour {

constexpr unsigned kVersion = 1;
constexpr unsigned kDelayMs = 1500;   // after the first field frame, so the map has drawn before the card
constexpr unsigned kPulseMs = 6000;   // how long "Show me" pulses the outlines

// Whether the card should come up on its own now.
inline bool ShouldOffer(bool enabled, unsigned seen, bool inField, unsigned msInField) {
    return enabled && seen < kVersion && inField && msInField >= kDelayMs;
}

// "Show me" outline strength, 0..1: a 1 Hz pulse that fades over kPulseMs, then 0 (the normal outlines again).
inline float Pulse(unsigned ms) {
    if (ms >= kPulseMs) return 0.0f;
    const float phase = static_cast<float>(ms % 1000) / 1000.0f;
    const float wave = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
    const float fade = 1.0f - static_cast<float>(ms) / static_cast<float>(kPulseMs);
    return (0.35f + 0.65f * wave) * (0.4f + 0.6f * fade);
}

}  // namespace UiTour
