#include "drift_correct.hpp"
#include <ArduinoLog.h>

#include <cmath>
#include <limits>

namespace mlm
{
    DriftCorrector::DriftCorrector(float timeDelta, float cornerFrequency)
    {
        if (!std::isfinite(cornerFrequency) || cornerFrequency <= 0.0f)
        {
            Log.fatalln("invalid corner frequency given to drift corrector");
            // TODO: add fatal error logic
        }
        if (!std::isfinite(timeDelta) || timeDelta <= 0.0f)
        {
            Log.fatalln("invalid time delta given to drift corrector");
            // TODO: add fatal error logic
        }

        m_lastSampleCorrected = std::numeric_limits<float>::signaling_NaN();
        m_lastSampleUncorrected = std::numeric_limits<float>::signaling_NaN();

        float RC = 1.0f / (2.0f * PI * cornerFrequency);
        m_alpha = RC / (RC + timeDelta);
        if (!std::isfinite(m_alpha) || !std::isnormal(m_alpha))
        {
            Log.fatalln("corner frequency given to drift corrector is too great or too small");
            // TODO: add fatal error logic
        }
    }

    float DriftCorrector::next(float sample)
    {
        // Algorithm from https://en.wikipedia.org/wiki/High-pass_filter (2023-02-28)

        if (!std::isfinite(sample))
        {
            Log.warningln("invalid sample passed into drift correction, ignoring");
            return m_lastSampleCorrected;
        }
        if (std::isnan(m_lastSampleCorrected) || std::isnan(m_lastSampleUncorrected))
        {
            // First sample taken in, just pass it straight through
            m_lastSampleCorrected = sample;
            m_lastSampleUncorrected = sample;
            return sample;
        }

        float corrected = m_alpha * (m_lastSampleCorrected + sample - m_lastSampleUncorrected);
        m_lastSampleCorrected = corrected;
        m_lastSampleUncorrected = sample;
        return corrected;
    }
}