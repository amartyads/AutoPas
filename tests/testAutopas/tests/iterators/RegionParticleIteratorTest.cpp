/**
 * @file RegionParticleIteratorTest.cpp
 * @author F. Gratl
 * @date 08.03.21
 */
#include "RegionParticleIteratorTest.h"

#include "IteratorTestHelper.h"
#include "autopas/AutoPasDecl.h"
#include "testingHelpers/EmptyFunctor.h"

extern template class autopas::AutoPas<Molecule>;
extern template bool autopas::AutoPas<Molecule>::iteratePairwise(EmptyFunctor<Molecule> *);

template <typename AutoPasT>
auto RegionParticleIteratorTest::defaultInit(AutoPasT &autoPas, const autopas::ContainerOption &containerOption,
                                             double cellSizeFactor) {
  autoPas.setBoxMin({0., 0., 0.});
  autoPas.setBoxMax({10., 10., 10.});
  autoPas.setCutoff(1);
  autoPas.setVerletSkinPerTimestep(0.1);
  autoPas.setVerletRebuildFrequency(2);
  autoPas.setNumSamples(2);
  autoPas.setAllowedContainers(std::set<autopas::ContainerOption>{containerOption});
  autoPas.setAllowedTraversals(autopas::compatibleTraversals::allCompatibleTraversals(containerOption));
  autoPas.setAllowedCellSizeFactors(autopas::NumberSetFinite<double>(std::set<double>({cellSizeFactor})));

  autoPas.init();

  auto haloBoxMin =
      autopas::utils::ArrayMath::subScalar(autoPas.getBoxMin(), autoPas.getVerletSkin() + autoPas.getCutoff());
  auto haloBoxMax =
      autopas::utils::ArrayMath::addScalar(autoPas.getBoxMax(), autoPas.getVerletSkin() + autoPas.getCutoff());

  return std::make_tuple(haloBoxMin, haloBoxMax);
}

/**
 * 1. Create an AutoPas container with 1000 particles that are placed around its 8 corners.
 * 2. Create a region iterator well around the lower corner of the container
 * 3. Run the region iterator for its full range and track the IDs it encounters
 * 4. Compare the found IDs to the expectations from the initialization.
 */
TEST_P(RegionParticleIteratorTest, testRegionAroundCorner) {
  auto [containerOption, cellSizeFactor, useConstIterator, priorForceCalc, behavior] = GetParam();

  // init autopas and fill it with some particles
  autopas::AutoPas<Molecule> autoPas;
  defaultInit(autoPas, containerOption, cellSizeFactor);

  using ::autopas::utils::ArrayMath::add;
  using ::autopas::utils::ArrayMath::mulScalar;
  using ::autopas::utils::ArrayMath::sub;
  const auto domainLength = sub(autoPas.getBoxMax(), autoPas.getBoxMin());
  // draw a box around the lower corner of the domain
  const auto searchBoxLengthHalf = mulScalar(domainLength, 0.3);
  const auto searchBoxMin = sub(autoPas.getBoxMin(), searchBoxLengthHalf);
  const auto searchBoxMax = add(autoPas.getBoxMin(), searchBoxLengthHalf);

  // initialize particles and remember which IDs are in the search box
  const auto [particleIDsOwned, particleIDsHalo, particleIDsInBoxOwned, particleIDsInBoxHalo] =
      IteratorTestHelper::fillContainerAroundBoundary(autoPas, searchBoxMin, searchBoxMax);

  if (priorForceCalc) {
    // the prior force calculation is partially wanted as this sometimes changes the state of the internal containers.
    EmptyFunctor<Molecule> eFunctor;
    autoPas.iteratePairwise(&eFunctor);
  }

  // set up expectations
  // can't trivially convert this to const + lambda initialization bc behavior is a structured binding
  std::vector<size_t> expectedIDs;
  switch (behavior) {
    case autopas::IteratorBehavior::owned: {
      expectedIDs = particleIDsInBoxOwned;
      break;
    }
    case autopas::IteratorBehavior::halo: {
      expectedIDs = particleIDsInBoxHalo;
      break;
    }
    case autopas::IteratorBehavior::ownedOrHalo: {
      expectedIDs = particleIDsInBoxOwned;
      expectedIDs.insert(expectedIDs.end(), particleIDsInBoxHalo.begin(), particleIDsInBoxHalo.end());
      break;
    }
    default: {
      GTEST_FAIL() << "IteratorBehavior::" << behavior
                   << "  should not be tested through this test!\n"
                      "Container behavior with dummy particles is not uniform.\n"
                      "Using forceSequential is not supported.";
      break;
    }
  }

  // sanity check: there should be particles in the expected region
  ASSERT_THAT(expectedIDs, ::testing::Not(::testing::IsEmpty()));

  // actual test
  IteratorTestHelper::provideRegionIterator(
      useConstIterator, autoPas, behavior, searchBoxMin, searchBoxMax,
      [&](const auto &autopas, auto &iter) { IteratorTestHelper::findParticles(autoPas, iter, expectedIDs); });
}

using ::testing::Combine;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::ValuesIn;

static inline auto getTestableContainerOptions() { return autopas::ContainerOption::getAllOptions(); }

INSTANTIATE_TEST_SUITE_P(Generated, RegionParticleIteratorTest,
                         Combine(ValuesIn(getTestableContainerOptions()), /*cell size factor*/ Values(0.5, 1., 1.5),
                                 /*use const*/ Values(true, false), /*prior force calc*/ Values(true, false),
                                 ValuesIn(autopas::IteratorBehavior::getMostOptions())),
                         RegionParticleIteratorTest::PrintToStringParamName());

/**
 * Tests that AutoPas rejects regions where regionMin > regionMax.
 */
TEST_F(RegionParticleIteratorTest, testInvalidBox) {
  // setup
  autopas::AutoPas<Molecule> autoPas{};
  const auto [haloBoxMin, haloBoxMax] = defaultInit(autoPas, autopas::ContainerOption::directSum, 1.);

  // helpers
  using autopas::utils::ArrayMath::mulScalar;
  using autopas::utils::ArrayMath::sub;

  // calculate box size
  const std::array<double, 3> haloBoxLength = sub(haloBoxMax, haloBoxMin);
  const std::array<double, 3> haloBoxLength3rd = mulScalar(haloBoxMax, 1. / 3.);

  // calculate points within the domain
  const std::array<double, 3> regionUpperLimit = mulScalar(haloBoxLength3rd, 2.);
  const std::array<double, 3> regionLowerLimit = mulScalar(haloBoxLength3rd, 1.);

  // actual test
  EXPECT_NO_THROW(autoPas.getRegionIterator(regionLowerLimit, regionUpperLimit));
  EXPECT_THROW(autoPas.getRegionIterator(regionUpperLimit, regionLowerLimit),
               autopas::utils::ExceptionHandler::AutoPasException);
}

///**
// TODO: Is this worthwhile to reimplement? If yes how?
// * Generates an iterator in a parallel region but iterates with only one and expects to find everything.
// * @note This behavior is needed by VerletClusterLists::updateHaloParticle().
// */
// TEST_F(RegionParticleIteratorTest, testForceSequential) {
//  constexpr size_t particlesPerCell = 1;
//  auto cells = IteratorTestHelper::generateCellsWithPattern(10, {1ul, 2ul, 4ul, 7ul, 8ul, 9ul}, particlesPerCell);
//
//  // min (inclusive) and max (exclusive) along the line of particles
//  size_t interestMin = 2;
//  size_t interestMax = 8;
//  const auto interestMinD = static_cast<double>(interestMin);
//  const auto interestMaxD = static_cast<double>(interestMax);
//  std::array<double, 3> searchBoxMin{interestMinD, interestMinD, interestMinD};
//  std::array<double, 3> searchBoxMax{interestMaxD, interestMaxD, interestMaxD};
//  std::vector<size_t> searchBoxCellIndices(interestMax - interestMin);
//  std::iota(searchBoxCellIndices.begin(), searchBoxCellIndices.end(), interestMin);
//
//  // IDs of particles in cells 2, 4, 7
//  std::vector<size_t> expectedIndices = {1, 2, 3};
//
//  constexpr size_t numAdditionalVectors = 3;
//  std::vector<std::vector<Molecule>> additionalVectors(numAdditionalVectors);
//
//  size_t particleId = cells.size() + 100;
//  for (auto &vector : additionalVectors) {
//    vector.emplace_back(Molecule({interestMinD, interestMinD, interestMinD}, {0., 0., 0.}, particleId));
//    expectedIndices.push_back(particleId);
//    ++particleId;
//  }
//
//#pragma omp parallel
//  {
//    std::vector<size_t> foundParticles;
//    constexpr bool modifyable = true;
//    autopas::internal::RegionParticleIterator<Molecule, FMCell, modifyable> iter(
//        &cells, searchBoxMin, searchBoxMax, searchBoxCellIndices, nullptr,
//        autopas::IteratorBehavior::ownedOrHalo | autopas::IteratorBehavior::forceSequential, &additionalVectors);
//    for (; iter.isValid(); ++iter) {
//      foundParticles.push_back(iter->getID());
//    }
//    EXPECT_THAT(foundParticles, ::testing::UnorderedElementsAreArray(expectedIndices));
//  }
//}
