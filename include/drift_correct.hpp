#include <cstdint>

template <std::size_t history_length>
class DriftCorrector
{
public:
    [[nodiscard]] float next(float value);
    [[nodiscard]] float get_drift() const noexcept;
};