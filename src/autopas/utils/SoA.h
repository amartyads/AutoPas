/**
 * @file SoA.h
 * @authors tchipevn, seckler
 * @date 18.01.2018
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <map>
#include <tuple>
#include <vector>
#include "autopas/utils/AlignedAllocator.h"
#include "autopas/utils/ExceptionHandler.h"
#include "autopas/utils/SoAStorage.h"
#include "autopas/utils/SoAType.h"

namespace autopas {

/**
 * Structur of the array class.
 * @tparam SoAArraysType The SoAArrayType to be used for storage.
 */
template <class SoAArraysType>
class SoA {
 public:
  /**
   * @brief Default constructor.
   */
  SoA() = default;

  /**
   * @brief Copy constructor.
   * @param soa SoA to copy.
   */
  SoA(const SoA &soa) = default;

  /**
   * @brief Resizes all Vectors to the given length.
   * @param length new length.
   */
  void resizeArrays(size_t length) {
    soaStorage.apply([=](auto &list) { list.resize(length + viewStart); });
  }

  /**
   * @brief Pushes a given value to the desired attribute array.
   * @tparam attribute Index of array to push to.
   * @param value Value to push.
   */
  template <std::size_t attribute>
  void push(const double value) {
    soaStorage.template get<attribute>().push_back(value);
  }

  /**
   * @brief Writes / updates the value of an attribute for a specific particle.
   * @tparam attribute Attribute to update.
   * @tparam ValueType type of the attribute
   * @param particleId Particle to update.
   * @param value New value.
   */
  template <int attribute, class ValueType>
  void write(size_t particleId, const ValueType &value) {
    soaStorage.template get<attribute>().at(particleId) = value;
  }

  /**
   * @brief Writes / updates values of attributes for a specific particle.
   * Appends the other SoA buffer.
   * @param other other buffer.
   */
  void append(const SoA<SoAArraysType> &other) {
    if (other.getNumParticles() > 0) {
      append_impl(other, std::make_index_sequence<std::tuple_size<SoAArraysType>::value>{});
    }
  }

  /**
   * Writes or updates values of attributes for a specific particle.
   * @tparam attributes Array of attributes to update.
   * @tparam ValueArrayType type of the array
   * @param particleId Particle to update.
   * @param values New value.
   */
  template <int... attributes, class ValueArrayType>
  void writeMultiple(size_t particleId, const ValueArrayType &values) {
    write_impl<attributes...>(particleId + viewStart, values);
  }

  /**
   * Specialized version to pass arrays without specifying it directly.
   * @tparam attributes
   * @tparam N
   * @param particleId
   * @param values
   */
  template <int... attributes, size_t N = sizeof...(attributes)>
  inline void writeMultiple(size_t particleId, std::array<double, N> values) {
    write_impl<attributes...>(particleId + viewStart, values);
  }

  /**
   * @brief Reads from all given attribute arrays at position `particleId`.
   * @tparam ArrayLength length of the returned array. Should be equal
   * attributes.size().
   * @tparam attributes Attributes to read from.
   * @param particleId Position to read from.
   * @return Array of attributes ordered by given attribute order.
   */
  template <int... attributes>
  std::array<double, sizeof...(attributes)> readMultiple(size_t particleId) {
    std::array<double, sizeof...(attributes)> retArray;
    if (particleId >= getNumParticles()) {
      autopas::utils::ExceptionHandler::exception(
          "SoA::read: requested particle id ({}) is bigger than number of particles ({})", particleId,
          getNumParticles());
      return retArray;
    }
    read_impl<attributes...>(particleId + viewStart, retArray);
    return retArray;
  }

  /**
   * @brief Reads the value of a given attribute of a given particle.
   * @tparam attribute Attribute to read from.
   * @param particleId Position to read from.
   * @return Attribute value.
   */
  template <std::size_t attribute>
  auto read(size_t particleId) {
    return soaStorage.template get<attribute>().at(particleId + viewStart);
  }

  /**
   * Returns a pointer to the given attribute vector.
   * @tparam attribute ID of the desired attribute.
   * @return Pointer to the beginning of the attribute vector
   */
  template <std::size_t attribute>
  auto begin() {
    return soaStorage.template get<attribute>().data() + viewStart;
  }

  /**
   * @brief Returns the number of particles.
   *
   * This function only checks the size of the first array since it is assumed
   * that the user manages the arrays responsibly.
   *
   * @return Number of particles.
   */
  inline constexpr size_t getNumParticles() const {
    return (viewLength == -1l) ? soaStorage.template get<0>().size() - viewStart : viewLength;
  }

  /**
   * delete all particles in the soa
   */
  void clear() {
    soaStorage.apply([](auto &list) { list.clear(); });
    viewStart = 0;
    viewLength = -1;
  }

  /**
   * swap the position of two particles in the soa
   * @param a position of the first particle
   * @param b position of the second particle
   */
  void swap(std::size_t a, std::size_t b) {
    soaStorage.apply([=](auto &list) { std::swap(list[a], list[b]); });
  }

  /**
   * Delete the last particle in the SoA.
   */
  void pop_back() {
    soaStorage.apply([](auto &list) { list.pop_back(); });
  }

  /**
   * Set index of the particle which is externally shown as the first particle in the buffer.
   * @param start index of the first element
   */
  void setViewStart(size_t start) { viewStart = start; }

  /**
   * Set length of view (-1 == view continues until the end).
   * @param length length of view
   */
  void setViewLength(long length) { viewLength = length; }

 private:
  // actual implementation of read
  template <int attribute, int... attributes, class ValueArrayType>
  void read_impl(size_t particleId, ValueArrayType &values, int _current = 0) {
    values[_current] = soaStorage.template get<attribute>().at(particleId);
    read_impl<attributes...>(particleId, values, _current + 1);
  }

  // stop of recursive read call
  template <class ValueArrayType>
  void read_impl(size_t particleId, ValueArrayType &values, int _current = 0) {}

  // actual implementation of the write function.
  // uses a recursive call.
  template <int attribute, int... attributes, class ValueArrayType>
  void write_impl(size_t particleId, const ValueArrayType &values, int _current = 0) {
    soaStorage.template get<attribute>().at(particleId) = values[_current];
    write_impl<attributes...>(particleId, values, _current + 1);
  }

  // Stop of the recursive write_impl call
  template <class ValueArrayType>
  void write_impl(size_t particleId, const ValueArrayType &values, int _current = 0) {}

  // helper function to append a single array
  template <std::size_t attribute>
  void appendSingleArray(const utils::SoAStorage<SoAArraysType> &valArrays, const size_t otherViewStart) {
    auto &currentVector = soaStorage.template get<attribute>();
    const auto &otherVector = valArrays.template get<attribute>();
    if (otherVector.size() < otherViewStart) {
      return;
    }
    currentVector.insert(currentVector.end(), otherVector.begin() + otherViewStart, otherVector.end());
  }

  // actual implementation of append
  template <std::size_t... Is>
  void append_impl(const SoA<SoAArraysType> &other, std::index_sequence<Is...>) {
    // fold expression
    (appendSingleArray<Is>(other.soaStorage, other.viewStart), ...);
  }

  // ------------- members ---------------

  // storage container for the SoA's
  utils::SoAStorage<SoAArraysType> soaStorage;

  size_t viewStart = 0;
  long viewLength = -1;
};
}  // namespace autopas
