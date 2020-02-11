/**
 * @file TraversalComparison.cpp
 * @author humig
 * @date 12.07.19
 */

#include "TraversalComparison.h"

#include <string>

#include "autopas/selectors/ContainerSelector.h"
#include "autopas/selectors/TraversalSelector.h"
#include "autopas/utils/StringUtils.h"
#include "autopasTools/generators/RandomGenerator.h"

using ::testing::_;  // anything is ok
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

std::vector<std::array<double, 3>> TraversalComparison::calculateForces(autopas::ContainerOption containerOption,
                                                                        autopas::TraversalOption traversalOption,
                                                                        autopas::DataLayoutOption dataLayoutOption,
                                                                        autopas::Newton3Option newton3Option,
                                                                        unsigned long numMolecules,
                                                                        std::array<double, 3> boxMax) {
  // Construct container
  autopas::ContainerSelector<Molecule, FMCell> selector{_boxMin, boxMax, _cutoff};
  selector.selectContainer(containerOption, autopas::ContainerSelectorInfo{1.0, _cutoff * 0.1, 32});
  auto container = selector.getCurrentContainer();
  autopas::LJFunctor<Molecule, FMCell> functor{_cutoff};
  functor.setParticleProperties(_eps * 24, _sig * _sig);

  auto traversal = autopas::TraversalSelector<FMCell>::generateTraversal(
      traversalOption, functor, container->getTraversalSelectorInfo(), dataLayoutOption, newton3Option);
  if (not traversal->isApplicable()) {
    return {};
  }

  autopasTools::generators::RandomGenerator::fillWithParticles(
      *container, autopas::MoleculeLJ({0., 0., 0.}, {0., 0., 0.}, 0), container->getBoxMin(), container->getBoxMax(),
      numMolecules);

  container->rebuildNeighborLists(traversal.get());
  container->iteratePairwise(traversal.get());

  std::vector<std::array<double, 3>> forces(numMolecules);
  for (auto it = container->begin(); it.isValid(); ++it) {
    forces.at(it->getID()) = it->getF();
  }

  return forces;
}
void TraversalComparison::SetUpTestSuite() {
  autopas::Logger::create();

  // Calculate reference forces
  for (auto numParticles : _numParticlesVector) {
    for (auto boxMax : _boxMaxVector) {
      auto calculatedForces =
          calculateForces(autopas::ContainerOption::linkedCells, autopas::TraversalOption::c08,
                          autopas::DataLayoutOption::aos, autopas::Newton3Option::enabled, numParticles, boxMax);
      _forcesReference[{numParticles, boxMax}] = calculatedForces;
    }
  }

  autopas::Logger::unregister();
}

TEST_P(TraversalComparison, traversalTest) {
  auto [containerOption, traversalOption, dataLayoutOption, newton3Option, numParticles, boxMax] = GetParam();

  // empirically determined and set near the minimal possible value for 2000 particles
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.0e-10;

  auto calculatedForces =
      calculateForces(containerOption, traversalOption, dataLayoutOption, newton3Option, numParticles, boxMax);
  if (calculatedForces.empty()) {
    GTEST_SKIP_("Not applicable!");
  }

  for (int i = 0; i < numParticles; ++i) {
    for (int d = 0; d < 3; ++d) {
      double calculatedForce = calculatedForces[i][d];
      double referenceForce = _forcesReference[{numParticles, boxMax}][i][d];
      EXPECT_NEAR(calculatedForce, referenceForce, std::fabs(calculatedForce * rel_err_tolerance));
    }
  }
}

static auto toString = [](const auto& info) {
  auto [containerOption, traversalOption, dataLayoutOption, newton3Option, numParticles, boxMax] = info.param;
  std::stringstream resStream;
  resStream << containerOption.to_string() << traversalOption.to_string()
            << (dataLayoutOption == autopas::DataLayoutOption::aos ? "AoS" : "SoA")
            << (newton3Option == autopas::Newton3Option::enabled ? "Newton3" : "NoNewton3") << "_" << numParticles
            << "_" << boxMax[0] << "_" << boxMax[1] << "_" << boxMax[2] << "";
  std::string res = resStream.str();
  std::replace(res.begin(), res.end(), '-', '_');
  return res;
};

static auto getTestParams() {
  std::vector<TestingTuple> params{};
  auto containerOptions = autopas::ContainerOption::getAllOptions();
  // @todo: Readd verlet cluster lists as soon as the iterator works without dummy particles.
  containerOptions.erase(autopas::ContainerOption::verletClusterLists);
  for (auto containerOption : containerOptions) {
    for (auto traversalOption : autopas::compatibleTraversals::allCompatibleTraversals(containerOption)) {
      for (auto dataLayoutOption : {autopas::DataLayoutOption::aos, autopas::DataLayoutOption::soa}) {
        for (auto newton3Option : {autopas::Newton3Option::enabled, autopas::Newton3Option::disabled}) {
          for (auto numParticles : {100, 1000, 2000}) {
            for (auto boxMax : std::array<std::array<double, 3>, 2>{{{3, 3, 3}, {10, 10, 10}}}) {
              params.emplace_back(containerOption, traversalOption, dataLayoutOption, newton3Option, numParticles,
                                  boxMax);
            }
          }
        }
      }
    }
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(Generated, TraversalComparison, ::testing::ValuesIn(getTestParams()), toString);