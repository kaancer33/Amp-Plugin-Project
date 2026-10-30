#pragma once
#include "DspCommon.h"

namespace forge
{
/** Component values of the classic passive treble/middle/bass network ("FMV" stack).
    R1 = treble pot, R2 = bass pot, R3 = middle pot, R4 = slope resistor. */
struct ToneStackComponents
{
    double R1, R2, R3, R4, C1, C2, C3;
};

/** Measured component values, as collected by the Guitarix project (tonestacks.lib). */
namespace toneStacks
{
    constexpr double k = 1.0e3, M = 1.0e6, nF = 1.0e-9, pF = 1.0e-12;

    constexpr ToneStackComponents twin    { 250 * k, 250 * k, 10 * k, 100 * k, 120 * pF, 100 * nF, 47 * nF }; // '69 Fender Twin Reverb
    constexpr ToneStackComponents bassman { 250 * k, 1 * M,   25 * k,  56 * k, 250 * pF,  20 * nF, 20 * nF }; // '59 Fender Bassman 5F6-A
    constexpr ToneStackComponents jcm800  { 220 * k, 1 * M,   22 * k,  33 * k, 470 * pF,  22 * nF, 22 * nF }; // Marshall JCM800 2203
    constexpr ToneStackComponents soldano { 250 * k, 1 * M,   25 * k,  47 * k, 470 * pF,  20 * nF, 20 * nF }; // Soldano SLO-100
}

/** The 3rd-order analog transfer function of the tone stack, derived symbolically by
    D.T. Yeh and J.O. Smith ("Discretization of the '59 Fender Bassman Tone Stack", DAFx-06),
    discretised with the bilinear transform:

        H(s) = (b1 s + b2 s^2 + b3 s^3) / (1 + a1 s + a2 s^2 + a3 s^3)

    The knobs interact exactly as they do on the real circuit. Runs in double precision
    because a direct-form 3rd-order filter with poles near DC is not stable enough in float
    at oversampled rates. */
class ToneStack
{
public:
    void setComponents (const ToneStackComponents& comp) noexcept
    {
        const double R1 = comp.R1, R2 = comp.R2, R3 = comp.R3, R4 = comp.R4;
        const double C1 = comp.C1, C2 = comp.C2, C3 = comp.C3;

        b1t  = C1*R1;
        b1m  = C3*R3;
        b1l  = C1*R2 + C2*R2;
        b1d  = C1*R3 + C2*R3;
        b2t  = C1*C2*R1*R4 + C1*C3*R1*R4;
        b2m2 = -(C1*C3*R3*R3 + C2*C3*R3*R3);
        b2m  = C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3;
        b2l  = C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4;
        b2lm = C1*C3*R2*R3 + C2*C3*R2*R3;
        b2d  = C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4;
        b3lm = C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4;
        b3m2 = -(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4);
        b3m  = C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4;
        b3t  = C1*C2*C3*R1*R3*R4;
        b3tm = -b3t;
        b3tl = C1*C2*C3*R1*R2*R4;
        a1d  = C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4;
        a1m  = C3*R3;
        a1l  = C1*R2 + C2*R2;
        a2m  = C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3;
        a2lm = C1*C3*R2*R3 + C2*C3*R2*R3;
        a2m2 = -(C1*C3*R3*R3 + C2*C3*R3*R3);
        a2l  = C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4;
        a2d  = C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4
             + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4;
        a3lm = C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4;
        a3m2 = -(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4);
        a3m  = C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4;
        a3l  = C1*C2*C3*R1*R2*R4;
        a3d  = C1*C2*C3*R1*R3*R4;

        // The passive stack loses 10-25 dB. Normalise so that all knobs at noon average
        // 0 dB across the guitar band - the next gain stage makes this up on a real amp.
        double sumDb = 0.0;
        int count = 0;
        for (double f = 80.0; f < 5000.0; f *= 1.12, ++count)
            sumDb += gainToDb (analogMagnitude (0.5, 0.5, 0.5, f));
        makeup = dbToGain (-sumDb / count);
    }

    void prepare (double sampleRate) noexcept { c = 2.0 * sampleRate; reset(); }
    void reset() noexcept                     { s1 = s2 = s3 = 0.0; }

    /** Pot positions in 0..1, as on the front panel. */
    void setControls (double bass, double mid, double treble) noexcept
    {
        const auto an = analog (bass, mid, treble);
        const double c1 = c, c2 = c * c, c3 = c2 * c;

        const double B0 =  an.b1 * c1 + an.b2 * c2 +       an.b3 * c3;
        const double B1 =  an.b1 * c1 - an.b2 * c2 - 3.0 * an.b3 * c3;
        const double B2 = -an.b1 * c1 - an.b2 * c2 + 3.0 * an.b3 * c3;
        const double B3 = -an.b1 * c1 + an.b2 * c2 -       an.b3 * c3;
        const double A0 = 1.0 + an.a1 * c1 + an.a2 * c2 +       an.a3 * c3;
        const double A1 = 3.0 + an.a1 * c1 - an.a2 * c2 - 3.0 * an.a3 * c3;
        const double A2 = 3.0 - an.a1 * c1 - an.a2 * c2 + 3.0 * an.a3 * c3;
        const double A3 = 1.0 - an.a1 * c1 + an.a2 * c2 -       an.a3 * c3;

        const double inv = makeup / A0;
        nb0 = B0 * inv;  nb1 = B1 * inv;  nb2 = B2 * inv;  nb3 = B3 * inv;
        na1 = A1 / A0;   na2 = A2 / A0;   na3 = A3 / A0;
    }

    inline double process (double x) noexcept
    {
        const double y = nb0 * x + s1;
        s1 = nb1 * x - na1 * y + s2;
        s2 = nb2 * x - na2 * y + s3;
        s3 = nb3 * x - na3 * y;
        return y;
    }

    /** Exact analog magnitude |H(j 2 pi f)| including the make-up gain, for the UI. */
    double displayMagnitude (double bass, double mid, double treble, double f) const noexcept
    {
        return makeup * analogMagnitude (bass, mid, treble, f);
    }

private:
    struct Analog { double b1, b2, b3, a1, a2, a3; };

    Analog analog (double bass, double mid, double treble) const noexcept
    {
        const double t = std::clamp (treble, 0.0, 1.0);
        const double m = std::clamp (mid,    0.0, 1.0);
        // The bass pot is audio-taper on the real amps; this mapping follows Guitarix.
        const double l = std::exp ((std::clamp (bass, 0.0, 1.0) - 1.0) * 3.4);

        Analog an;
        an.b1 = t*b1t + m*b1m + l*b1l + b1d;
        an.b2 = t*b2t + m*m*b2m2 + m*b2m + l*b2l + l*m*b2lm + b2d;
        an.b3 = l*m*b3lm + m*m*b3m2 + m*b3m + t*b3t + t*m*b3tm + t*l*b3tl;
        an.a1 = a1d + m*a1m + l*a1l;
        an.a2 = m*a2m + l*m*a2lm + m*m*a2m2 + l*a2l + a2d;
        an.a3 = l*m*a3lm + m*m*a3m2 + m*a3m + l*a3l + a3d;
        return an;
    }

    double analogMagnitude (double bass, double mid, double treble, double f) const noexcept
    {
        const auto an = analog (bass, mid, treble);
        const std::complex<double> s (0.0, kTwoPi * f);
        const auto num = s * (an.b1 + s * (an.b2 + s * an.b3));
        const auto den = 1.0 + s * (an.a1 + s * (an.a2 + s * an.a3));
        return std::abs (num / den);
    }

    double b1t = 0, b1m = 0, b1l = 0, b1d = 0, b2t = 0, b2m2 = 0, b2m = 0, b2l = 0, b2lm = 0, b2d = 0;
    double b3lm = 0, b3m2 = 0, b3m = 0, b3t = 0, b3tm = 0, b3tl = 0;
    double a1d = 0, a1m = 0, a1l = 0, a2m = 0, a2lm = 0, a2m2 = 0, a2l = 0, a2d = 0;
    double a3lm = 0, a3m2 = 0, a3m = 0, a3l = 0, a3d = 0;

    double makeup = 1.0, c = 96000.0;
    double nb0 = 0, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0;
    double s1 = 0, s2 = 0, s3 = 0;
};
} // namespace forge
