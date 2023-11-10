#ifndef MVMT_GAUSS_FILTER_HPP
#define MVMT_GAUSS_FILTER_HPP

// NOTE: Implementation of functions is provided in this file due to the nature of C++ templating. If implementations
// are moved out of this file, linker errors ensue.

#include <ArduinoEigen/Eigen/Dense>
#include <cmath>
#include <deque>
#include <vector>

namespace mvmt {

/// @brief Implements a Gaussian filter, which works by convolving a bell curve with an input signal.
/// @tparam T The type of the signal, typically either a floating-point number or an Eigen::Matrix
template <typename T> class GaussianFilter {
public:
  std::deque<T> m_buffer;
  const std::size_t m_buffer_length;
  std::vector<double> m_gauss_table;
  const T m_zero_element;

public:
  /// @brief Constructs a GaussianFilter object.
  /// @param stddev The standard deviation of the bell curve to be used
  /// @param z_cutoff The number of standard deviations to lag behind the input signal
  /// @param time_delta The difference in time between two samples of the input signal
  /// @param zero_element The zero point of the input type, typically either 0.0 or a zero Eigen::Matrix
  GaussianFilter(double stddev, double z_cutoff, double time_delta, T zero_element) noexcept
      : m_buffer_length(2 * (z_cutoff * stddev) / time_delta), m_zero_element(zero_element) {
    double mean = stddev * z_cutoff;
    for (std::size_t i = 0; i < m_buffer_length; i++) {
      auto x = i * time_delta;
      auto intermediate = exp(-0.5 * pow((x - mean) / stddev, 2));
      m_gauss_table.push_back(intermediate / (stddev * sqrt(2 * M_PI)));
    }
    double accumulator = 0.0;
    for (auto val : m_gauss_table) {
      accumulator += val;
    }
    auto scale_factor = 1.0 / accumulator;
    for (auto &val : m_gauss_table) {
      val *= scale_factor;
    }
  }

  /// @brief Add a new sample to the filter's buffer.
  /// @param measurement The new sample to add
  void add_measurement(T measurement) noexcept {
    if (m_buffer.size() >= m_buffer_length) {
      m_buffer.pop_front();
    }
    m_buffer.push_back(measurement);
  }
  /// @brief Get the current output of the filter.
  /// @return The current output value of the filter; if the filter buffer is not yet full, returns the latest
  /// unfiltered sample
  [[nodiscard]] T get_current() const noexcept {
    // if queue is underfilled, return current back of queue
    if (m_buffer.size() < m_buffer_length) {
      return m_buffer.back();
    }
    // convolve!
    T accumulator = m_zero_element;
    for (std::size_t i = 0; i < m_buffer_length; i++) {
      accumulator += m_gauss_table[i] * m_buffer[i];
    }
    return accumulator;
  }
};

} // namespace mvmt

#endif