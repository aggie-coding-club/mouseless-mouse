#include "drift_correct.hpp"
#include <ArduinoLog.h>

#include <cmath>
#include <limits>

DriftCorrector::DriftCorrector(float cornerFrequency = 0.1f)
{
    if (std::isnan(cornerFrequency) || std::isinf(cornerFrequency) || cornerFrequency <= 0.0f)
    {
        Log.errorln(
            "invalid corner frequency given to drift corrector, substituting with 0.1 Hz");
        cornerFrequency = 0.1f;
    }

    m_lastSampleCorrected = std::numeric_limits<float>::signaling_NaN();
    m_lastSampleUncorrected = std::numeric_limits<float>::signaling_NaN();
    m_timeSinceGoodSample = 0.0f;
    m_RC = 1.0f / (2.0f * PI * cornerFrequency);
}

float DriftCorrector::next(float sample, float timeDelta)
{
    // Algorithm from https://en.wikipedia.org/wiki/High-pass_filter (2023-02-28)

    if (std::isnan(sample) || std::isinf(sample))
    {
        Log.warningln("invalid sample passed into drift correction, ignoring");
        if (!std::isnan(timeDelta) && !std::isinf(timeDelta) && timeDelta > 0.0f)
        {
            m_timeSinceGoodSample += timeDelta;
        }
        return sample;
    }
    else if (std::isnan(timeDelta) || std::isinf(timeDelta) || timeDelta <= 0.0f)
    {
        Log.warningln("invalid time delta passed into drift correction, ignoring");
        return sample;
    }
    timeDelta += m_timeSinceGoodSample;
    m_timeSinceGoodSample = 0.0f;

    if (std::isnan(m_lastSampleCorrected) || std::isnan(m_lastSampleUncorrected))
    {
        // First sample taken in, just pass it straight through
        m_lastSampleCorrected = sample;
        m_lastSampleUncorrected = sample;
        return sample;
    }
    float alpha = m_RC / (m_RC + timeDelta);
    float corrected = alpha * (m_lastSampleCorrected + sample - m_lastSampleUncorrected);
    m_lastSampleCorrected = corrected;
    m_lastSampleUncorrected = sample;
    return corrected;
}