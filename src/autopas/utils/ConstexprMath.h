/**
 * @file ConstexprMath.h
 *
 * @date 15.05.2023
 * @author D. Martin
 */

#pragma once

#include <limits>

namespace autopas::utils::ConstexprMath {

/**
 * Calculates the square root of floating point values x based on Newton-Raphson methon
 * @tparam T floating point type
 * @param x input value
 * @return sqrt(x)
 */
template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type sqrt(T x) {
  if (x >= 0 && x < std::numeric_limits<T>::infinity()) {
    T xn = x;
    T prev = 0;
    while (xn != prev) {
      prev = xn;
      T xn1 = 0.5 * (xn + x / xn);
      xn = xn1;
    }
    return xn;
  } else {
    return std::numeric_limits<T>::quiet_NaN();
  }
}

/**
 * Calculates the square root of integral values x based on Newton-Raphson methon
 * @tparam T integral type
 * @param x input value
 * @return sqrt(x)
 */
template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type sqrt(T x) {
  // see https://en.wikipedia.org/wiki/Integer_square_root#Example_implementation_in_C
  if (x >= 0) {
    if (x <= 1) return x;
    T x0 = x / 2;
    T x1 = (x0 + x / x0) / 2;
    while (x1 < x0) {
      x0 = x1;
      x1 = (x0 + x / x0) / 2;
    }
    return x0;
  } else {
    throw std::invalid_argument("Negative number passed.");
  }
}

}  // namespace autopas::utils::ConstexprMath