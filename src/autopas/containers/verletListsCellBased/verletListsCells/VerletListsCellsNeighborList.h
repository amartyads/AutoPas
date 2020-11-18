/**
 * @file VerletListsCellsNeighborList.h
 * @author tirgendetwas
 * @date 25.10.20
 */

#pragma once

#include "autopas/containers/verletListsCellBased/verletListsCells/VerletListsCellsHelpers.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/VerletListsCellsNeighborListInterface.h"
#include "autopas/options/TraversalOption.h"
#include "autopas/selectors/TraversalSelector.h"
#include "autopas/utils/ArrayMath.h"
#include "autopas/utils/StaticBoolSelector.h"

namespace autopas {
/**
 * Neighbor list to be used with VerletListsCells container. Classic implementation of verlet lists based on linked
 * cells.
 * @tparam Particle Type of particle to be used for this neighbor list.
 * */
template <class Particle>
class VerletListsCellsNeighborList : public VerletListsCellsNeighborListInterface<Particle> {
 public:
  /**
   * Constructor for VerletListsCellsNeighborList. Initializes private attributes.
   * */
  VerletListsCellsNeighborList() : _aosNeighborList{}, _particleToCellMap{} {}

  /**
   * @copydoc VerletListsCellsNeighborListInterface::buildAoSNeighborList()
   * */
  void buildAoSNeighborList(LinkedCells<Particle> &linkedCells, bool useNewton3, double cutoff, double skin,
                            double interactionLength, const TraversalOption buildTraversalOption) override {
    // Initialize a neighbor list for each cell.
    _aosNeighborList.clear();
    auto &cells = linkedCells.getCells();
    size_t cellsSize = cells.size();
    _aosNeighborList.resize(cellsSize);
    for (size_t cellIndex = 0; cellIndex < cellsSize; ++cellIndex) {
      _aosNeighborList[cellIndex].reserve(cells[cellIndex].numParticles());
      size_t particleIndexWithinCell = 0;
      for (auto iter = cells[cellIndex].begin(); iter.isValid(); ++iter, ++particleIndexWithinCell) {
        Particle *particle = &*iter;
        _aosNeighborList[cellIndex].emplace_back(particle, std::vector<Particle *>());
        // In a cell with N particles, reserve space for 5N neighbors.
        // 5 is an empirically determined magic number that provides good speed.
        _aosNeighborList[cellIndex].back().second.reserve(cells[cellIndex].numParticles() * 5);
        _particleToCellMap[particle] = std::make_pair(cellIndex, particleIndexWithinCell);
      }
    }

    applyBuildFunctor(linkedCells, useNewton3, cutoff, skin, interactionLength, buildTraversalOption);
  }

  /**
   * @copydoc VerletListsCellsNeighborListInterface::getVerletList()
   * */
  const std::vector<Particle *> &getVerletList(const Particle *particle) const override {
    const auto [cellIndex, particleIndexInCell] = _particleToCellMap.at(const_cast<Particle *>(particle));
    return _aosNeighborList.at(cellIndex).at(particleIndexInCell).second;
  }

  /**
   * @copydoc VerletListsCellsNeighborListInterface::getContainerType()
   * */
  [[nodiscard]] ContainerOption getContainerType() const override { return ContainerOption::verletListsCells; }

  /**
   * Returns the neighbor list in AoS layout.
   * @return Neighbor list in AoS layout.
   * */
  typename VerletListsCellsHelpers<Particle>::NeighborListsType &getAoSNeighborList() { return _aosNeighborList; }

 private:
  /**
   * Creates and applies generator functor for the building of the neighbor list.
   * @param linkedCells Linked Cells object used to build the neighbor list.
   * @param useNewton3 Whether Newton 3 should be used for the neighbor list.
   * @param cutoff Cutoff radius.
   * @param skin Skin of the verlet list.
   * @param interactionLength Interaction length of the underlying linked cells object.
   * @param buildTraversalOption Traversal option necessary for generator functor.
   * */
  void applyBuildFunctor(LinkedCells<Particle> &linkedCells, bool useNewton3, double cutoff, double skin,
                         double interactionLength, const TraversalOption buildTraversalOption) {
    typename VerletListsCellsHelpers<Particle>::VerletListGeneratorFunctor f(_aosNeighborList, _particleToCellMap,
                                                                             cutoff + skin);

    // Generate the build traversal with the traversal selector and apply the build functor with it.
    TraversalSelector<FullParticleCell<Particle>> traversalSelector;
    // Argument "cluster size" does not matter here.
    TraversalSelectorInfo traversalSelectorInfo(linkedCells.getCellBlock().getCellsPerDimensionWithHalo(),
                                                interactionLength, linkedCells.getCellBlock().getCellLength(), 0);
    autopas::utils::withStaticBool(useNewton3, [&](auto n3) {
      auto buildTraversal = traversalSelector.template generateTraversal<decltype(f), DataLayoutOption::aos, n3>(
          buildTraversalOption, f, traversalSelectorInfo);
      linkedCells.iteratePairwise(buildTraversal.get());
    });
  }

  /**
   * Internal neighbor list structure in AoS format - Verlet lists for each particle for each cell.
   * */
  typename VerletListsCellsHelpers<Particle>::NeighborListsType _aosNeighborList;

  /**
   * Mapping of each particle to its corresponding cell and id within this cell.
   */
  std::unordered_map<Particle *, std::pair<size_t, size_t>> _particleToCellMap;
};
}  // namespace autopas