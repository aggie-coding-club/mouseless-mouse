#include <cstdint>

/**
 * @class DriftCorrector
 * @brief Object offering drift-correction capabilities for real-valued signals
 * @details Uses a high-pass filter to correct for unintended long-term drift in a signal, assuming that changes below
 *  a minimum frequency are to be considered drift
 */
class DriftCorrector
{
private:
    float m_lastSampleUncorrected;
    float m_lastSampleCorrected;
    float m_RC; // time constant (RC)

public:
    /**
     * @param cornerFrequency the maximum frequency (in Hz) of changes that should be considered drift
     */
    DriftCorrector(float cornerFrequency = 0.1f);
    /**
     * @brief Sends a value through for correction.
     *
     * @param value the uncorrected value, which will also be stored in the history
     * @param timeDelta the time (in seconds) since the last value was stored
     * @returns the value corrected for any drift that has been detected thus far.
     */
    [[nodiscard]] float next(float value, float timeDelta);
};