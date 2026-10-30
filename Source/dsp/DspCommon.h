#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <complex>

namespace forge
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db * 0.05); }
inline double gainToDb (double g)  noexcept { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

/** Pade (3,2) tanh approximation. It reaches +-1 with zero slope at |x| = 3, so it is
    smooth everywhere, and it is several times cheaper than std::tanh. */
template <typename T>
inline T fastTanh (T x) noexcept
{
    if (x >  T (3)) return T (1);
    if (x < T (-3)) return T (-1);
    const T x2 = x * x;
    return x * (T (27) + x2) / (T (27) + T (9) * x2);
}

//==============================================================================
/** RBJ-cookbook biquad, transposed direct form II, in double precision so low-frequency
    filters stay accurate at oversampled rates. */
struct Biquad
{
    enum class Type { lowPass, highPass, peak, lowShelf, highShelf };

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void set (Type type, double fs, double freq, double q, double gainDb = 0.0) noexcept
    {
        freq = std::clamp (freq, 1.0, fs * 0.49);
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = kTwoPi * freq / fs;
        const double cs    = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        double B0, B1, B2, A0, A1, A2;

        switch (type)
        {
            case Type::lowPass:
                B0 = (1.0 - cs) * 0.5;  B1 = 1.0 - cs;     B2 = B0;
                A0 = 1.0 + alpha;       A1 = -2.0 * cs;    A2 = 1.0 - alpha;
                break;

            case Type::highPass:
                B0 = (1.0 + cs) * 0.5;  B1 = -(1.0 + cs);  B2 = B0;
                A0 = 1.0 + alpha;       A1 = -2.0 * cs;    A2 = 1.0 - alpha;
                break;

            case Type::peak:
                B0 = 1.0 + alpha * A;   B1 = -2.0 * cs;    B2 = 1.0 - alpha * A;
                A0 = 1.0 + alpha / A;   A1 = -2.0 * cs;    A2 = 1.0 - alpha / A;
                break;

            case Type::lowShelf:
            {
                const double sA = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1.0) - (A - 1.0) * cs + sA);
                B1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
                B2 = A * ((A + 1.0) - (A - 1.0) * cs - sA);
                A0 = (A + 1.0) + (A - 1.0) * cs + sA;
                A1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
                A2 = (A + 1.0) + (A - 1.0) * cs - sA;
                break;
            }

            case Type::highShelf:
            default:
            {
                const double sA = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1.0) + (A - 1.0) * cs + sA);
                B1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
                B2 = A * ((A + 1.0) + (A - 1.0) * cs - sA);
                A0 = (A + 1.0) - (A - 1.0) * cs + sA;
                A1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
                A2 = (A + 1.0) - (A - 1.0) * cs - sA;
                break;
            }
        }

        const double inv = 1.0 / A0;
        b0 = B0 * inv;  b1 = B1 * inv;  b2 = B2 * inv;
        a1 = A1 * inv;  a2 = A2 * inv;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    /** |H(e^jw)| at frequency f - used to draw response curves. */
    double magnitude (double fs, double f) const noexcept
    {
        const auto e1 = std::polar (1.0, -kTwoPi * f / fs);
        const auto e2 = e1 * e1;
        return std::abs ((b0 + b1 * e1 + b2 * e2) / (1.0 + a1 * e1 + a2 * e2));
    }
};

//==============================================================================
struct OnePoleLP
{
    double a = 1.0, z = 0.0;

    void setCutoff (double fs, double f) noexcept
    {
        a = 1.0 - std::exp (-kTwoPi * std::min (f, fs * 0.49) / fs);
    }

    inline double process (double x) noexcept { z += a * (x - z); return z; }
    void reset (double v = 0.0) noexcept      { z = v; }

    double magnitude (double fs, double f) const noexcept
    {
        const auto e1 = std::polar (1.0, -kTwoPi * f / fs);
        return std::abs (a / (1.0 - (1.0 - a) * e1));
    }
};

struct OnePoleHP
{
    OnePoleLP lp;

    void setCutoff (double fs, double f) noexcept { lp.setCutoff (fs, f); }
    inline double process (double x) noexcept    { return x - lp.process (x); }
    void reset() noexcept                         { lp.reset(); }

    double magnitude (double fs, double f) const noexcept
    {
        const auto e1 = std::polar (1.0, -kTwoPi * f / fs);
        return std::abs (1.0 - lp.a / (1.0 - (1.0 - lp.a) * e1));
    }
};

//==============================================================================
/** Click-free switching of something discrete (amp voice, cab model): fade out, let the
    caller swap at silence, fade back in. A new request while fading in reverses the fade. */
class SwitchFader
{
public:
    void prepare (double fs, double fadeMs) noexcept
    {
        step = 1.0f / (float) std::max (1.0, fs * fadeMs * 0.001);
        reset();
    }

    void reset() noexcept               { gain = 1.0f; state = State::idle; }
    void request() noexcept             { state = State::fadingOut; }
    bool isActive() const noexcept      { return state != State::idle; }

    /** Advances one sample. Sets swapNow exactly once, when the output has reached silence. */
    inline float next (bool& swapNow) noexcept
    {
        swapNow = false;

        if (state == State::fadingOut)
        {
            gain -= step;
            if (gain <= 0.0f) { gain = 0.0f; state = State::fadingIn; swapNow = true; }
        }
        else if (state == State::fadingIn)
        {
            gain += step;
            if (gain >= 1.0f) { gain = 1.0f; state = State::idle; }
        }

        return gain;
    }

private:
    enum class State { idle, fadingOut, fadingIn };
    State state = State::idle;
    float gain = 1.0f, step = 0.01f;
};

//==============================================================================
/** Lock-free peak meter: the audio thread pushes, the UI takes and resets. */
class LevelMeter
{
public:
    void push (const float* data, int n) noexcept
    {
        float m = 0.0f;
        for (int i = 0; i < n; ++i)
            m = std::max (m, std::abs (data[i]));

        float prev = peak.load (std::memory_order_relaxed);
        while (m > prev && ! peak.compare_exchange_weak (prev, m, std::memory_order_relaxed)) {}
    }

    float take() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> peak { 0.0f };
};

//==============================================================================
/** Power-of-two circular delay line with fractional reads. read() must be called before
    push() for a delay of d samples; d is clamped to [1, capacity - 4]. */
class DelayLine
{
public:
    void allocate (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }

    inline void push (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }

    inline float readLinear (double d) const noexcept
    {
        d = std::clamp (d, 1.0, (double) (mask - 4));
        const double rp = (double) writeIndex - d;
        const double fl = std::floor (rp);
        const int i0 = (int) fl;
        const float frac = (float) (rp - fl);
        const float s0 = buffer[(size_t) (i0 & mask)];
        const float s1 = buffer[(size_t) ((i0 + 1) & mask)];
        return s0 + frac * (s1 - s0);
    }

    /** 4-point Hermite read; needs d >= 2 so that every tap is already written. */
    inline float readHermite (double d) const noexcept
    {
        d = std::clamp (d, 2.0, (double) (mask - 4));
        const double rp = (double) writeIndex - d;
        const double fl = std::floor (rp);
        const int i = (int) fl;
        const float t = (float) (rp - fl);
        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) (i & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writeIndex = 0;
};
} // namespace forge
