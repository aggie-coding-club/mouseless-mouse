#include "drift_correct.hpp"
#include "errors.hpp"

#include <ArduinoLog.h>
#include <cmath>
#include <limits>

namespace mlm
{
    DriftCorrector::DriftCorrector(float timeDelta, float cornerFrequency) noexcept
    {
        if (!std::isfinite(cornerFrequency) || cornerFrequency <= 0.0f)
        {
            errors::doFatalError(
                "invalid corner frequency passed into drift corrector", errors::SOFTWARE_INITIALIZATION_FAILED);
        }
        if (!std::isfinite(timeDelta) || timeDelta <= 0.0f)
        {
            errors::doFatalError(
                "invalid time delta passed into drift corrector", errors::SOFTWARE_INITIALIZATION_FAILED);
        }

        m_lastSampleCorrected = std::numeric_limits<float>::signaling_NaN();
        m_lastSampleUncorrected = std::numeric_limits<float>::signaling_NaN();

        float RC = 1.0f / (2.0f * PI * cornerFrequency);
        m_alpha = RC / (RC + timeDelta);
        if (!std::isfinite(m_alpha) || !std::isnormal(m_alpha))
        {
            errors::doFatalError(
                "corner frequency given to drift corrector is too great or too small",
                errors::SOFTWARE_INITIALIZATION_FAILED);
        }
    }

    float DriftCorrector::next(float sample) noexcept
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