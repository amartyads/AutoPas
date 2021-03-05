/**
 * @file ParticleIteratorInterfaceTest.cpp
 * @author seckler
 * @date 22.07.19
 */

#include "ParticleIteratorInterfaceTest.h"

#include "autopas/options/IteratorBehavior.h"
#include "autopas/utils/WrapOpenMP.h"
#include "autopasTools/generators/RandomGenerator.h"
#include "testingHelpers/EmptyFunctor.h"
#include "testingHelpers/TouchableParticle.h"
#include "testingHelpers/commonTypedefs.h"

using ::testing::_;

template <typename AutoPasT>
auto ParticleIteratorInterfaceTest::defaultInit(AutoPasT &autoPas, autopas::ContainerOption &containerOption,
                                                double cellSizeFactor) {
  autoPas.setBoxMin({0., 0., 0.});
  autoPas.setBoxMax({10., 10., 10.});
  autoPas.setCutoff(1);
  autoPas.setVerletSkin(0.2);
  autoPas.setVerletRebuildFrequency(2);
  autoPas.setNumSamples(2);
  autoPas.setAllowedContainers(std::set<autopas::ContainerOption>{containerOption});
  autoPas.setAllowedTraversals(autopas::compatibleTraversals::allCompatibleTraversals(containerOption));
  autoPas.setAllowedCellSizeFactors(autopas::NumberSetFinite<double>(std::set<double>({cellSizeFactor})));

#ifdef AUTOPAS_CUDA
  autoPas.setVerletClusterSize(32);
#endif

  autoPas.init();

  auto haloBoxMin =
      autopas::utils::ArrayMath::subScalar(autoPas.getBoxMin(), autoPas.getVerletSkin() + autoPas.getCutoff());
  auto haloBoxMax =
      autopas::utils::ArrayMath::addScalar(autoPas.getBoxMax(), autoPas.getVerletSkin() + autoPas.getCutoff());

  return std::make_tuple(haloBoxMin, haloBoxMax);
}

template <class AutoPasT>
auto ParticleIteratorInterfaceTest::fillContainerAroundBoundary(AutoPasT &autoPas) {
  constexpr size_t numParticles1dTotal = 10;

  auto cutoff = autoPas.getCutoff();
  auto skin = autoPas.getVerletSkin();

  // generator function for critical coordinates (along  one dimension)
  auto generateInteresting1DPositions = [&](double min, double max) -> auto {
    // ensure that all particles are at most skin away from halo!
    // interesting cases are:
    //   - outside of the halo by skin
    //   - edge of halo
    //   - in the halo
    //   - edge of actual domain
    //   - just inside the domain
    return std::array<double, numParticles1dTotal>{min - cutoff - skin + 1e-10,
                                                   min - cutoff,
                                                   min - skin / 4,
                                                   min,
                                                   min + skin / 4,
                                                   max - skin / 4,
                                                   max,
                                                   max + skin / 4,
                                                   max + cutoff,
                                                   max + cutoff + skin - 1e-10};
  };

  // fill container
  size_t id = 0;
  auto boxMin = autoPas.getBoxMin();
  auto boxMax = autoPas.getBoxMax();

  std::vector<size_t> particleIDsHalo;
  std::vector<size_t> particleIDsOwned;
  for (auto x : generateInteresting1DPositions(boxMin[0], boxMax[0])) {
    for (auto y : generateInteresting1DPositions(boxMin[1], boxMax[1])) {
      for (auto z : generateInteresting1DPositions(boxMin[2], boxMax[2])) {
        std::array<double, 3> pos{x, y, z};
        Molecule p(pos, {0., 0., 0.}, id++, 0);
        // add the particle as actual or halo particle
        if (autopas::utils::inBox(pos, boxMin, boxMax)) {
          autoPas.addParticle(p);
          particleIDsOwned.push_back(p.getID());
        } else {
          // AutoPas should set the ownership state of this particle to halo
          autoPas.addOrUpdateHaloParticle(p);
          particleIDsHalo.push_back(p.getID());
        }
      }
    }
  }

  // sanity check. Can not use assert because this introduces a different return.
  EXPECT_EQ(particleIDsOwned.size() + particleIDsHalo.size(),
            numParticles1dTotal * numParticles1dTotal * numParticles1dTotal);
  // getNumberOfParticles works via counters in the logic handler
  EXPECT_EQ(autoPas.getNumberOfParticles(autopas::IteratorBehavior::ownedOnly), particleIDsOwned.size());
  EXPECT_EQ(autoPas.getNumberOfParticles(autopas::IteratorBehavior::haloOnly), particleIDsHalo.size());
  return std::make_tuple(particleIDsOwned, particleIDsHalo);
}

template <class AutoPasT>
auto ParticleIteratorInterfaceTest::fillContainerWithGrid(AutoPasT &autoPas, double sparsity) {
  auto cutoff = autoPas.getCutoff();
  auto skin = autoPas.getVerletSkin();
  auto cellSizeFactor = *(autoPas.getAllowedCellSizeFactors().getAll().begin());

  auto boxLength = autopas::utils::ArrayMath::sub(autoPas.getBoxMax(), autoPas.getBoxMin());

  auto gridWidth1D = (cutoff + skin) * cellSizeFactor;
  auto gridEdgesPerDim = autopas::utils::ArrayMath::mulScalar(boxLength, 1 / gridWidth1D);
  auto gridWidth3D = autopas::utils::ArrayMath::div(boxLength, gridEdgesPerDim);

  size_t id = 0;
  std::vector<size_t> particleIDs;
  for (double x = gridWidth3D[0] / 2; x < boxLength[0]; x += sparsity * gridWidth3D[0]) {
    for (double y = gridWidth3D[1] / 2; y < boxLength[1]; y += sparsity * gridWidth3D[1]) {
      for (double z = gridWidth3D[2] / 2; z < boxLength[2]; z += sparsity * gridWidth3D[2]) {
        std::array<double, 3> pos{x, y, z};
        Molecule p(pos, {0., 0., 0.}, id++, 0);
        autoPas.addParticle(p);
        particleIDs.push_back(p.getID());
      }
    }
  }

  return particleIDs;
}

template <class AutoPasT, class F>
void ParticleIteratorInterfaceTest::provideIterator(bool useRegionIterator, bool useConstIterator,
                                                    autopas::IteratorBehavior behavior, AutoPasT &autoPas, F fun) {
  if (useConstIterator) {
    provideIterator<true>(useRegionIterator, behavior, autoPas, fun);
  } else {
    provideIterator<false>(useRegionIterator, behavior, autoPas, fun);
  }
}

template <bool useConstIterator, class AutoPasT, class F>
void ParticleIteratorInterfaceTest::provideIterator(bool useRegionIterator, autopas::IteratorBehavior behavior,
                                                    AutoPasT &autoPas, F fun) {
  if (useRegionIterator) {
    const auto interactionLength = autoPas.getCutoff() + autoPas.getVerletSkin();
    // halo has width of interactionLength
    const auto haloBoxMin = autopas::utils::ArrayMath::subScalar(autoPas.getBoxMin(), interactionLength);
    const auto haloBoxMax = autopas::utils::ArrayMath::addScalar(autoPas.getBoxMax(), interactionLength);
    if constexpr (useConstIterator) {
      const auto &autoPasRef = autoPas;
      auto getIter = [&]() -> typename AutoPasT::const_iterator_t {
        return autoPasRef.getRegionIterator(haloBoxMin, haloBoxMax, behavior);
      };
      fun(autoPasRef, getIter);
    } else {
      auto getIter = [&]() -> typename AutoPasT::iterator_t {
        return autoPas.getRegionIterator(haloBoxMin, haloBoxMax, behavior);
      };
      fun(autoPas, getIter);
    }
  } else {
    if constexpr (useConstIterator) {
      auto getIter = [&]() -> typename AutoPasT::const_iterator_t { return autoPas.cbegin(behavior); };
      fun(autoPas, getIter);
    } else {
      auto getIter = [&]() -> typename AutoPasT::iterator_t { return autoPas.begin(behavior); };
      fun(autoPas, getIter);
    }
  }
}

template <class AutoPasT, class FgetIter>
void ParticleIteratorInterfaceTest::findParticles(AutoPasT &autopas, FgetIter getIter,
                                                  const std::vector<size_t> &particleIDsExpected) {
  std::vector<size_t> particleIDsFound;

#ifdef AUTOPAS_OPENMP
  // aparently the version from WrapOpenMP.h can not be found
#pragma omp declare reduction(vecMerge : std::vector<size_t> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end()))
#pragma omp parallel reduction(vecMerge : particleIDsFound)
#endif
  {
    for (auto iterator = getIter(); iterator.isValid(); ++iterator) {
      auto id = iterator->getID();
      particleIDsFound.push_back(id);
    }
  }

  // check that everything was found
  EXPECT_THAT(particleIDsFound, ::testing::UnorderedElementsAreArray(particleIDsExpected));
}
template <bool constIter, class AutoPasT, class F>
auto ParticleIteratorInterfaceTest::deleteParticles(AutoPasT &autopas, F predicate, bool useRegionIterator,
                                                    const autopas::IteratorBehavior &behavior) {
  if constexpr (not constIter) {
    provideIterator<false>(useRegionIterator, behavior, autopas, [&](auto &autopas, auto getIter) {
#ifdef AUTOPAS_OPENMP
#pragma omp parallel
#endif
      {
        for (auto iter = getIter(); iter.isValid(); ++iter) {
          if (predicate(iter->getID())) {
            autopas.deleteParticle(iter);
          }
        }
      }
    });
  } else {
    GTEST_FAIL() << "Calling deleteParticles with a const iterator! This indicates that the test is ill defined!";
  }
}
template <class AutoPasT>
auto ParticleIteratorInterfaceTest::addParticles(AutoPasT &autopas, size_t idOffset, bool useRegionIterator,
                                                 const autopas::IteratorBehavior &behavior) {
  provideIterator<false>(useRegionIterator, behavior, autopas, [&](auto &autopas, auto getIter) {
    std::atomic<bool> encounteredBadParticle = false;
#ifdef AUTOPAS_OPENMP
#pragma omp parallel
#endif
    {
      for (auto iter = getIter(); iter.isValid(); ++iter) {
        // only insert new particles for original particles. Otherwise this becomes an infinite loop.
        if (iter->getID() < idOffset) {
          // copy the particle, offset its ID and add it
          auto newParticle = *iter;
          newParticle.setID(newParticle.getID() + idOffset);
          if (newParticle.isOwned()) {
            autopas.addParticle(newParticle);
          } else if (newParticle.isHalo()) {
            autopas.addOrUpdateHaloParticle(newParticle);
          } else {
            // we can not fail the test here since failing from inside an OpenMP region is not possible
            encounteredBadParticle.store(true, std::memory_order_relaxed);
          }
        }
      }
    }
    if (encounteredBadParticle) {
      GTEST_FAIL() << "Particle to add is neither owned not halo!";
    }
  });
}

/**
 * This Test applies an iterator on the whole domain and expects to find that it is empty.
 * Exact Behavior might vary depending on test parameters but the general flow is:
 * - Create an AutoPas object with a specified container.
 * - Apply an iterator and confirm that it finds no particles.
 */
TEST_P(ParticleIteratorInterfaceTest, emptyContainer) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  // actual test
  provideIterator(useRegionIterator, useConstIterator, behavior, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, {}); });
}

/**
 * This Test applies an iterator on the whole domain and expects to find all particles this way.
 * Exact Behavior might vary depending on test parameters but the general flow is:
 * - Create an AutoPas object with a specified container.
 * - Place particles in a grid inside the domain.
 * - Find the particles with iterators and compare their IDs with expectations.
 */
TEST_P(ParticleIteratorInterfaceTest, findAllParticlesInsideDomain) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);
  auto expectedIDs = fillContainerWithGrid(autoPas, 3);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  // set up expectations
  switch (behavior) {
    case autopas::IteratorBehavior::haloAndOwned:
      [[fallthrough]];
    case autopas::IteratorBehavior::ownedOnly: {
      // expectations already correct
      break;
    }
    case autopas::IteratorBehavior::haloOnly: {
      // no particles in the halo -> expect nothing
      expectedIDs = {};
      break;
    }
    case autopas::IteratorBehavior::haloOwnedAndDummy: {
      GTEST_FAIL() << "IteratorBehavior::haloOwnedAndDummy should not be tested through this test"
                      " as container behavior with dummy particles is not uniform.";
      break;
    }
  }

  // actual test
  provideIterator(useRegionIterator, useConstIterator, behavior, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, expectedIDs); });
}

/**
 * This Test applies an iterator on the whole domain and expects to find all particles this way.
 * Exact Behavior might vary depending on test parameters but the general flow is:
 * - Create an AutoPas object with a specified container.
 * - Strategically place particles around the boundaries.
 * - Find the particles with iterators and compare their IDs with expectations.
 */
TEST_P(ParticleIteratorInterfaceTest, findAllParticlesAroundBoundaries) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);
  auto [particleIDsOwned, particleIDsHalo] = fillContainerAroundBoundary(autoPas);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  // set up expectations
  std::vector<size_t> expectedIDs;
  switch (behavior) {
    case autopas::IteratorBehavior::ownedOnly: {
      expectedIDs = particleIDsOwned;
      break;
    }
    case autopas::IteratorBehavior::haloOnly: {
      expectedIDs = particleIDsHalo;
      break;
    }
    case autopas::IteratorBehavior::haloAndOwned: {
      expectedIDs = particleIDsOwned;
      expectedIDs.insert(expectedIDs.end(), particleIDsHalo.begin(), particleIDsHalo.end());
      break;
    }
    case autopas::IteratorBehavior::haloOwnedAndDummy: {
      GTEST_FAIL() << "IteratorBehavior::haloOwnedAndDummy should not be tested through this test"
                      " as container behavior with dummy particles is not uniform.";
      break;
    }
  }

  // actual test
  provideIterator(useRegionIterator, useConstIterator, behavior, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, expectedIDs); });
}

/**
 * This test uses an iterator to delete every particle with an odd ID.
 * Since deletion does not work through const iterators this test is skipped when instantiated with
 * useConstIterator==true.
 */
TEST_P(ParticleIteratorInterfaceTest, deleteParticles) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();

  if (useConstIterator) {
    GTEST_SKIP_("Not applicable since deleting with a const iterator is not possible");
  }

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);
  auto expectedIDs = fillContainerWithGrid(autoPas, 3);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  auto isOdd = [](auto id) -> bool { return id % 2 != 0; };

  // set up expectations
  switch (behavior) {
    case autopas::IteratorBehavior::haloAndOwned:
      [[fallthrough]];
    case autopas::IteratorBehavior::ownedOnly: {
      // remove all odd numbers from expectations
      expectedIDs.erase(std::remove_if(expectedIDs.begin(), expectedIDs.end(), [&](auto id) { return isOdd(id); }),
                        expectedIDs.end());
      break;
    }
    case autopas::IteratorBehavior::haloOnly: {
      // nothing should be deleted so expect everything.
      break;
    }
    case autopas::IteratorBehavior::haloOwnedAndDummy: {
      GTEST_FAIL() << "IteratorBehavior::haloOwnedAndDummy should not be tested through this test"
                      " as container behavior with dummy particles is not uniform.";
      break;
    }
  }

  // delete all particles with odd ids
  if (useConstIterator) {
    GTEST_FAIL() << "Calling deleteParticles with a const iterator! This indicates that the test is ill defined!";
  } else {
    deleteParticles<false>(autoPas, isOdd, useRegionIterator, behavior);
  }

  // now use again an iterator to confirm only the expected ones are still there
  provideIterator(useRegionIterator, useConstIterator, autopas::IteratorBehavior::haloAndOwned, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, expectedIDs); });
}

/**
 * This test iterates through all particles and adds a copy for every particle it encounters while iterating.
 * No copies of copies are created.
 * @note Threads insert particles only in cells they are currently iterating. Insertion in cells of other threads is
 *   untested!
 */
TEST_P(ParticleIteratorInterfaceTest, addParticles) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);
  auto expectedIDs = fillContainerWithGrid(autoPas, 4);

  decltype(expectedIDs) idsToAdd;
  // offset to be added to all inserted IDs.
  // Should be large enough so that it is two orders of magnitude larger than the largest ID.
  // E.g. max ID = 42 -> offset =1000
  size_t idOffset =
      std::pow(10ul, std::to_string(*std::max_element(expectedIDs.cbegin(), expectedIDs.cend())).length() + 1);
  std::transform(expectedIDs.begin(), expectedIDs.end(), std::back_insert_iterator(idsToAdd),
                 [&](auto id) { return id + idOffset; });

  // set up expectations
  switch (behavior) {
    case autopas::IteratorBehavior::haloAndOwned:
      [[fallthrough]];
    case autopas::IteratorBehavior::ownedOnly: {
      // we insert everything a second time
      expectedIDs.insert(expectedIDs.begin(), idsToAdd.begin(), idsToAdd.end());
      break;
    }
    case autopas::IteratorBehavior::haloOnly: {
      // nothing should be added so expect everything.
      break;
    }
    case autopas::IteratorBehavior::haloOwnedAndDummy: {
      GTEST_FAIL() << "IteratorBehavior::haloOwnedAndDummy should not be tested through this test"
                      " as container behavior with dummy particles is not uniform.";
      break;
    }
  }

  addParticles(autoPas, idOffset, useRegionIterator, behavior);

  // now use again an iterator to confirm only the expected ones are still there
  provideIterator(useRegionIterator, useConstIterator, autopas::IteratorBehavior::haloAndOwned, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, expectedIDs); });
}

/**
 * This test iterates through all particles and adds a copy for every particle it encounters while iterating.
 * No copies of copies are created.
 * This scenario consists of owned and halo particles.
 * @note Threads insert particles only in cells they are currently iterating. Insertion in cells of other threads is
 *   untested!
 */
TEST_P(ParticleIteratorInterfaceTest, addOwnedAndHaloParticles) {
  auto [containerOption, cellSizeFactor, useRegionIterator, useConstIterator, priorForceCalc, behavior] = GetParam();
#ifdef AUTOPAS_OPENMP
  if (containerOption == autopas::ContainerOption::linkedCellsReferences and
      (behavior == autopas::IteratorBehavior::haloOnly or behavior == autopas::IteratorBehavior::haloAndOwned)) {
    GTEST_FAIL() << "TEST FAILED MANUALLY TO PREVENT CRASH!";
  }
#endif

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);
  auto [particleIDsOwned, particleIDsHalo] = fillContainerAroundBoundary(autoPas);
  // sanity check: There should be owned and halo particles
  ASSERT_THAT(particleIDsOwned, ::testing::Not(::testing::IsEmpty()));
  ASSERT_THAT(particleIDsHalo, ::testing::Not(::testing::IsEmpty()));

  decltype(particleIDsOwned) allParticleIDs, idsToAdd;

  allParticleIDs.insert(allParticleIDs.cend(), particleIDsOwned.begin(), particleIDsOwned.end());
  allParticleIDs.insert(allParticleIDs.cend(), particleIDsHalo.begin(), particleIDsHalo.end());

  auto expectedIDs = allParticleIDs;

  // offset to be added to all inserted IDs.
  // Should be large enough so that it is two orders of magnitude larger than the largest ID.
  // E.g. max ID = 42 -> offset =1000
  size_t idOffset =
      std::pow(10ul, std::to_string(*std::max_element(allParticleIDs.cbegin(), allParticleIDs.cend())).length() + 1);

  // set up expectations: fill idsToAdd depending on the iterator behavior
  switch (behavior) {
    case autopas::IteratorBehavior::haloAndOwned: {
      std::transform(allParticleIDs.begin(), allParticleIDs.end(), std::back_insert_iterator(idsToAdd),
                     [&](auto id) { return id + idOffset; });
      break;
    }
    case autopas::IteratorBehavior::haloOnly: {
      std::transform(particleIDsHalo.begin(), particleIDsHalo.end(), std::back_insert_iterator(idsToAdd),
                     [&](auto id) { return id + idOffset; });
      break;
    }
    case autopas::IteratorBehavior::ownedOnly: {
      std::transform(particleIDsOwned.begin(), particleIDsOwned.end(), std::back_insert_iterator(idsToAdd),
                     [&](auto id) { return id + idOffset; });
      break;
    }
    case autopas::IteratorBehavior::haloOwnedAndDummy: {
      GTEST_FAIL() << "IteratorBehavior::haloOwnedAndDummy should not be tested through this test"
                      " as container behavior with dummy particles is not uniform.";
      break;
    }
  }
  // we expect to find all previously existing particles plus idsToAdd
  expectedIDs.insert(expectedIDs.begin(), idsToAdd.begin(), idsToAdd.end());

  addParticles(autoPas, idOffset, useRegionIterator, behavior);

  // now use again an iterator to confirm only the expected ones are still there
  provideIterator(useRegionIterator, useConstIterator, autopas::IteratorBehavior::haloAndOwned, autoPas,
                  [&](const auto &autopas, auto &iter) { findParticles(autoPas, iter, expectedIDs); });
}

using ::testing::Combine;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::ValuesIn;

static inline auto getTestableContainerOptions() {
#ifdef AUTOPAS_CUDA
  return autopas::ContainerOption::getAllOptions();
#else
  auto containerOptions = autopas::ContainerOption::getAllOptions();
  containerOptions.erase(containerOptions.find(autopas::ContainerOption::verletClusterCells));
  return containerOptions;
#endif
}

static inline auto getIteratorBehaviorOptions() {
  auto allOptions = autopas::IteratorBehavior::getAllOptions();
  std::set<autopas::IteratorBehavior> retSet;
  // we ignore dummy particles in the general tests because they can behave differently depending on the container
  std::set<autopas::IteratorBehavior> ignoredOptions = {autopas::IteratorBehavior::haloOwnedAndDummy};
  std::set_difference(allOptions.begin(), allOptions.end(), ignoredOptions.begin(), ignoredOptions.end(),
                      std::inserter(retSet, retSet.begin()));
  return retSet;
}

INSTANTIATE_TEST_SUITE_P(Generated, ParticleIteratorInterfaceTest,
                         Combine(ValuesIn(getTestableContainerOptions()), /*cell size factor*/ Values(0.5, 1., 1.5),
                                 /*use region iter*/ Values(true, false),
                                 /*use const*/ Values(true, false), /*prior force calc*/ Values(true, false),
                                 ValuesIn(getIteratorBehaviorOptions())),
                         ParticleIteratorInterfaceTest::PrintToStringParamName());
