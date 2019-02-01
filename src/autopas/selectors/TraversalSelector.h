/**
 * @file TraversalSelector.h
 * @author F. Gratl
 * @date 11.06.18
 */

#pragma once

#include <array>
#include <numeric>
#include <unordered_map>
#include <vector>
#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/containers/cellPairTraversals/DummyTraversal.h"
#include "autopas/containers/cellPairTraversals/TraversalInterface.h"
#include "autopas/containers/directSum/DirectSumTraversal.h"
#include "autopas/containers/linkedCells/traversals/C01Traversal.h"
#include "autopas/containers/linkedCells/traversals/C08Traversal.h"
#include "autopas/containers/linkedCells/traversals/C18Traversal.h"
#include "autopas/containers/linkedCells/traversals/SlicedTraversal.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/traversals/C01TraversalVerlet.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/traversals/C18TraversalVerlet.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/traversals/SlicedTraversalVerlet.h"
#include "autopas/options/SelectorStrategie.h"
#include "autopas/pairwiseFunctors/CellFunctor.h"
#include "autopas/utils/ExceptionHandler.h"
#include "autopas/utils/Logger.h"
#include "autopas/utils/StringUtils.h"
#include "autopas/utils/TrivialHash.h"

namespace autopas {

/**
 * Selector for a container traversal.
 * @tparam ParticleCell
 */
template <class ParticleCell>
class TraversalSelector {
 public:
  /**
   * Dummy constructor such that this class can be used in maps
   */
  TraversalSelector() : _dims({0, 0, 0}), _allowedTraversalOptions({}) {}
  /**
   * Constructor of the TraversalSelector class.
   * @param dims Array with the dimension lengths of the domain.
   * @param allowedTraversalOptions Vector of traversals the selector can choose from.
   */
  TraversalSelector(const std::array<unsigned long, 3> &dims,
                    const std::vector<TraversalOption> &allowedTraversalOptions)
      : _dims(dims), _allowedTraversalOptions(allowedTraversalOptions) {}

  /**
   * Gets the optimal traversal for a given cell functor. If no traversal is selected yet a optimum search is started.
   * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
   * @tparam useSoA
   * @tparam useNewton3
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   * @return Smartpointer to the optimal traversal.
   */
  template <class PairwiseFunctor, bool useSoA, bool useNewton3>
  std::unique_ptr<CellPairTraversal<ParticleCell>> getOptimalTraversal(PairwiseFunctor &pairwiseFunctor);

  /**
   * Save the runtime of a given traversal if the functor is relevant for tuning.
   * @param pairwiseFunctor
   * @param traversal
   * @param time
   */
  template <class PairwiseFunctor>
  void addTimeMeasurement(PairwiseFunctor &pairwiseFunctor, TraversalOption traversal, long time) {
    if (pairwiseFunctor.isRelevantForTuning()) {
      struct TimeMeasurement measurement = {traversal, time};
      _traversalTimes.push_back(measurement);
    }
  }

  /**
   * Selects the next allowed and applicable traversal.
   * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
   * @tparam useSoA
   * @tparam useNewton3
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   * @return Smartpointer to the selected traversal.
   */
  template <class PairwiseFunctor, bool useSoA, bool useNewton3>
  std::unique_ptr<CellPairTraversal<ParticleCell>> selectNextTraversal(PairwiseFunctor &pairwiseFunctor);

  /**
   * Selects the optimal traversal based on saved measurements.
   * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
   * @tparam useSoA
   * @tparam useNewton3
   * @param strategy Strategy the selector should employ to choose the best traversal.
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   * @return Smartpointer to the selected traversal.
   */
  template <class PairwiseFunctor, bool useSoA, bool useNewton3>
  std::unique_ptr<CellPairTraversal<ParticleCell>> selectOptimalTraversal(SelectorStrategy strategy,
                                                                          PairwiseFunctor &pairwiseFunctor);

  /**
   * Sets the traversal to the given Option.
   * @param traversalOption
   */
  void selectTraversal(TraversalOption traversalOption);

 private:
  void findFastestAbsTraversal();

  void findFastestMeanTraversal();

  void findFastestMedianTraversal();

  template <class PairwiseFunctor, bool useSoA, bool useNewton3>
  std::vector<std::unique_ptr<TraversalInterface>> generateAllAllowedTraversals(PairwiseFunctor &pairwiseFunctor);

  template <class PairwiseFunctor, bool useSoA, bool useNewton3>
  std::unique_ptr<CellPairTraversal<ParticleCell>> generateTraversal(TraversalOption traversalType,
                                                                     PairwiseFunctor &pairwiseFunctor);

  // The optimal traversal for all functors that are marked relevant.
  TraversalOption _currentTraversal;
  // indicating whether or not the optimalTraversalOption is already initialized
  bool _isInitialized = false;
  // indicating whether we are currently testing through all options
  bool _isTuning = false;
  const std::array<unsigned long, 3> _dims;
  const std::vector<TraversalOption> _allowedTraversalOptions;

  struct TimeMeasurement {
    TraversalOption traversal;
    long time;
  };
  // vector of (traversal type, execution time)
  std::vector<TraversalSelector::TimeMeasurement> _traversalTimes;
};

template <class ParticleCell>
template <class PairwiseFunctor, bool useSoA, bool useNewton3>
std::vector<std::unique_ptr<TraversalInterface>> TraversalSelector<ParticleCell>::generateAllAllowedTraversals(
    PairwiseFunctor &pairwiseFunctor) {
  std::vector<std::unique_ptr<TraversalInterface>> traversals;

  for (auto &option : _allowedTraversalOptions) {
    traversals.push_back(generateTraversal<PairwiseFunctor, useSoA, useNewton3>(option, pairwiseFunctor));
  }

  if (traversals.empty()) utils::ExceptionHandler::exception("TraversalSelector: No traversals were generated.");

  return traversals;
}

template <class ParticleCell>
template <class PairwiseFunctor, bool useSoA, bool useNewton3>
std::unique_ptr<CellPairTraversal<ParticleCell>> TraversalSelector<ParticleCell>::generateTraversal(
    TraversalOption traversalType, PairwiseFunctor &pairwiseFunctor) {
  std::unique_ptr<CellPairTraversal<ParticleCell>> traversal;
  switch (traversalType) {
    case TraversalOption::directSumTraversal: {
      traversal =
          std::make_unique<DirectSumTraversal<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(&pairwiseFunctor);
      break;
    }
    case TraversalOption::c08: {
      traversal =
          std::make_unique<C08Traversal<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(_dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::sliced: {
      traversal =
          std::make_unique<SlicedTraversal<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(_dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::c18: {
      traversal =
          std::make_unique<C18Traversal<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(_dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::c01: {
      traversal =
          std::make_unique<C01Traversal<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(_dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::slicedVerlet: {
      traversal = std::make_unique<SlicedTraversalVerlet<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(
          _dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::c18Verlet: {
      traversal = std::make_unique<C18TraversalVerlet<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(
          _dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::c01Verlet: {
      traversal = std::make_unique<C01TraversalVerlet<ParticleCell, PairwiseFunctor, useSoA, useNewton3>>(
          _dims, &pairwiseFunctor);
      break;
    }
    case TraversalOption::dummyTraversal: {
      traversal = std::make_unique<DummyTraversal<ParticleCell>>(_dims);
      break;
    }
    default: {
      AutoPasLog(warn, "Traversal type {} is not a known type!", utils::StringUtils::to_string(traversalType));
    }
  }
  return traversal;
}

template <class ParticleCell>
template <class PairwiseFunctor, bool useSoA, bool useNewton3>
std::unique_ptr<CellPairTraversal<ParticleCell>> TraversalSelector<ParticleCell>::selectOptimalTraversal(
    SelectorStrategy strategy, PairwiseFunctor &pairwiseFunctor) {
  // Time measure strategy
  if (_traversalTimes.empty()) {
    utils::ExceptionHandler::exception("TraversalSelector: Trying to determine fastest traversal before measuring!");
  }

  switch (strategy) {
    case SelectorStrategy::fastestAbs: {
      findFastestAbsTraversal();
      break;
    }
    case SelectorStrategy::fastestMean: {
      findFastestMeanTraversal();
      break;
    }
    case SelectorStrategy::fastestMedian: {
      findFastestMedianTraversal();
      break;
    }
    default:
      utils::ExceptionHandler::exception("TraversalSelector: Unknown selector strategy {}", strategy);
  }

  // measurements are not needed anymore
  _traversalTimes.clear();

  // Assumption: the fastest traversal is applicable :O
  auto traversal = generateTraversal<PairwiseFunctor, useSoA, useNewton3>(_currentTraversal, pairwiseFunctor);

  AutoPasLog(debug, "Selected traversal {}", utils::StringUtils::to_string(_currentTraversal));
  return traversal;
}

template <class ParticleCell>
template <class PairwiseFunctor, bool useSoA, bool useNewton3>
std::unique_ptr<CellPairTraversal<ParticleCell>> TraversalSelector<ParticleCell>::selectNextTraversal(
    PairwiseFunctor &pairwiseFunctor) {
  std::unique_ptr<CellPairTraversal<ParticleCell>> traversal;
  bool traversalIsApplicable = false;

  // choose new traversals
  while (not traversalIsApplicable) {
    // if no measurements are in yet _currentTraversal is not initialized
    if (not _isTuning) {
      // no traversals are allowed
      if (_allowedTraversalOptions.size() == 0) {
        return std::unique_ptr<CellPairTraversal<ParticleCell>>(nullptr);
      }

      _currentTraversal = _allowedTraversalOptions.begin().operator*();
      _isTuning = true;
    } else {
      auto selectedTraversalIter =
          std::find(_allowedTraversalOptions.begin(), _allowedTraversalOptions.end(), _currentTraversal);
      ++selectedTraversalIter;

      // if there is no next return null
      if (selectedTraversalIter >= _allowedTraversalOptions.end()) {
        _isTuning = false;
        return std::unique_ptr<CellPairTraversal<ParticleCell>>(nullptr);
      }
      _currentTraversal = *selectedTraversalIter;
    }

    traversal = generateTraversal<PairwiseFunctor, useSoA, useNewton3>(_currentTraversal, pairwiseFunctor);
    traversalIsApplicable = traversal->isApplicable();
  }
  AutoPasLog(debug, "Testing traversal {}", utils::StringUtils::to_string(_currentTraversal));

  _isInitialized = true;
  return traversal;
}

template <class ParticleCell>
template <class PairwiseFunctor, bool useSoA, bool useNewton3>
std::unique_ptr<CellPairTraversal<ParticleCell>> TraversalSelector<ParticleCell>::getOptimalTraversal(
    PairwiseFunctor &pairwiseFunctor) {
  std::unique_ptr<CellPairTraversal<ParticleCell>> traversal;

  if (not _isInitialized)
    utils::ExceptionHandler::exception("TraversalSelector::getOptimalTraversal(): No Traversal selected yet!");

  traversal = generateTraversal<PairwiseFunctor, useSoA, useNewton3>(_currentTraversal, pairwiseFunctor);
  return traversal;
}

template <class ParticleCell>
void TraversalSelector<ParticleCell>::findFastestAbsTraversal() {
  // choose the fastest traversal and reset timings
  // Initialize with something. This will be overridden.
  long optimalTraversalTime = std::numeric_limits<long>::max();
  AutoPasLog(debug, "TraversalSelector: Collected traversal times:");
  for (auto &&t : _traversalTimes) {
    AutoPasLog(debug, "Traversal {} took {} nanoseconds.", utils::StringUtils::to_string(t.traversal), t.time);
    if (t.time < optimalTraversalTime) {
      _currentTraversal = t.traversal;
      optimalTraversalTime = t.time;
    }
  }

  // sanity check
  if (optimalTraversalTime == std::numeric_limits<long>::max()) {
    utils::ExceptionHandler::exception("TraversalSelector: Nothing was faster than max long! o_O");
  }
}

template <class ParticleCell>
void TraversalSelector<ParticleCell>::findFastestMeanTraversal() {
  // choose the fastest traversal and reset timings
  // reorder measurements
  std::unordered_map<TraversalOption, std::vector<long>, TrivialHash> measurementsMap;
  AutoPasLog(debug, "TraversalSelector: Collected traversal times:");
  for (auto &&t : _traversalTimes) {
    AutoPasLog(debug, "Traversal {} took {} nanoseconds.", utils::StringUtils::to_string(t.traversal), t.time);
    measurementsMap[t.traversal].push_back(t.time);
  }

  long optimalTraversalTime = std::numeric_limits<long>::max();
  // @todo: when verlet list traversals are here apply weights to measurement w/ or w/o vl rebuild
  for (auto &&m : measurementsMap) {
    long meanTime = std::accumulate(m.second.begin(), m.second.end(), 0l) / m.second.size();
    AutoPasLog(debug, "Traversal {} mean: {} nanoseconds", utils::StringUtils::to_string(m.first), meanTime);
    if (meanTime < optimalTraversalTime) {
      optimalTraversalTime = meanTime;
      _currentTraversal = m.first;
    }
  }

  // sanity check
  if (optimalTraversalTime == std::numeric_limits<long>::max()) {
    utils::ExceptionHandler::exception("TraversalSelector: Nothing was faster than max long! o_O");
  }
}

template <class ParticleCell>
void TraversalSelector<ParticleCell>::findFastestMedianTraversal() {
  // choose the fastest traversal and reset timings
  // reorder measurements
  std::unordered_map<TraversalOption, std::vector<long>, TrivialHash> measurementsMap;
  AutoPasLog(debug, "TraversalSelector: Collected traversal times:");
  for (auto &&t : _traversalTimes) {
    AutoPasLog(debug, "Traversal {} took {} nanoseconds.", utils::StringUtils::to_string(t.traversal), t.time);
    measurementsMap[t.traversal].push_back(t.time);
  }

  long optimalTraversalTime = std::numeric_limits<long>::max();
  for (auto &&m : measurementsMap) {
    std::sort(m.second.begin(), m.second.end());
    long medianTime = m.second[m.second.size() / 2];
    AutoPasLog(debug, "Traversal {} median: {} nanoseconds", utils::StringUtils::to_string(m.first), medianTime);
    if (medianTime < optimalTraversalTime) {
      optimalTraversalTime = medianTime;
      _currentTraversal = m.first;
    }
  }

  // sanity check
  if (optimalTraversalTime == std::numeric_limits<long>::max()) {
    utils::ExceptionHandler::exception("TraversalSelector: Nothing was faster than max long! o_O");
  }
}
template <class ParticleCell>
void TraversalSelector<ParticleCell>::selectTraversal(TraversalOption traversalOption) {
  _currentTraversal = traversalOption;
}

}  // namespace autopas
