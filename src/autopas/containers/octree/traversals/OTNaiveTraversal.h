/**
 * @file OTNaiveTraversal.h
 *
 * @author Johannes Spies
 * @date 09.04.2021
 */

#pragma once

#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/containers/octree/OctreeInnerNode.h"
#include "autopas/containers/octree/OctreeLeafNode.h"
#include "autopas/containers/octree/OctreeNodeInterface.h"
#include "autopas/containers/octree/traversals/OTTraversalInterface.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/pairwiseFunctors/CellFunctor.h"
#include "autopas/utils/DataLayoutConverter.h"

namespace autopas {

/**
 * This traversal is capable of iterating over particles stored in the Octree data structure. This traversal does not
 * use any parallelization or speed-increasing strategies and is therefore called naive.
 *
 * @tparam Particle
 * @tparam PairwiseFunctor
 * @tparam dataLayout
 * @tparam useNewton3
 */
template <class Particle, class PairwiseFunctor, DataLayoutOption::Value dataLayout, bool useNewton3>
class OTNaiveTraversal : public CellPairTraversal<OctreeLeafNode<Particle>>,
                         public OTTraversalInterface<OctreeNodeWrapper<Particle>> {
 public:
  /**
   * A shortcut to specify the type of the actual iterated cell
   */
  using ParticleCell = OctreeLeafNode<Particle>;

  // TODO(johannes): The TraversalSelector passes the interactionLength as the cutoff value: Keep in mind when
  // implementing...
  /**
   * Constructor for the Octree traversal.
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   * @param cutoff cutoff (this is enough for the octree traversal, please don't use the interaction length here.)
   */
  explicit OTNaiveTraversal(PairwiseFunctor *pairwiseFunctor, double cutoff)
      : CellPairTraversal<ParticleCell>({2, 1, 1}),
        _cellFunctor(pairwiseFunctor, cutoff /*should use cutoff here, if not used to build verlet-lists*/),
        _dataLayoutConverter(pairwiseFunctor) {}

  [[nodiscard]] TraversalOption getTraversalType() const override { return TraversalOption::ot_naive; }

  [[nodiscard]] bool isApplicable() const override {
    int nDevices = 0;
#if defined(AUTOPAS_CUDA)
    cudaGetDeviceCount(&nDevices);
#endif
    if (dataLayout == DataLayoutOption::cuda)
      return nDevices > 0;
    else
      return true;
  }

  [[nodiscard]] bool getUseNewton3() const override { return useNewton3; };

  [[nodiscard]] DataLayoutOption getDataLayout() const override { return dataLayout; };

  void initTraversal() override { printf("Johannes' OTNaiveTraversal::initTraversal\n"); }

  void endTraversal() override { printf("Johannes' OTNaiveTraversal::endTraversal\n"); }

  /**
   * @copydoc TraversalInterface::traverseParticlePairs()
   * @note This function expects a vector of exactly two cells. First cell is the main region, second is halo.
   */
  void traverseParticlePairs() override {
    // TODO(johannes): If the dataLayout is SoA, there needs to be a special buffer inside the iterated ParticleCells
    //  that is not initialized by default. Find out why this is the case and initialize it.
    //  LinkedCells uses an if constexpr check to see whether the code is using SoA or AoS.

    // Gather all leaves
    std::vector<OctreeLeafNode<Particle> *> leaves;
    auto *wrapper = dynamic_cast<OctreeNodeWrapper<Particle> *>(&(*_cells)[0]);
    wrapper->appendAllLeaves(leaves);

    for (OctreeLeafNode<Particle> *leaf : leaves) {
      leaf->clearAlreadyProcessedList();
    }

    // Get neighboring cells for each leaf
    for (OctreeLeafNode<Particle> *leaf : leaves) {
      // Process cell itself
      _cellFunctor.processCell(*leaf);

      // Process connection to all neighbors
      auto uniqueNeighboringLeaves = leaf->getNeighborLeaves();
      for (OctreeLeafNode<Particle> *neighborLeaf : uniqueNeighboringLeaves) {
        // TODO(johannes): Is this check still required if we are using newton3.
        //if (!leaf->alreadyProcessed(neighborLeaf)) {
          // Execute the cell functor
          _cellFunctor.processCellPair(*leaf, *neighborLeaf);

          // Mark the neighbor as processed in the leaf
        //  leaf->markAlreadyProcessed(neighborLeaf);
        //}
      }
    }
  }

  /**
   * Set the cells to iterate.
   * @param cells A list of octree roots that should be used during iteration
   */
  void setCells(std::vector<OctreeNodeWrapper<Particle>> *cells) override { _cells = cells; }

 private:
  // TODO(johannes): Can we use the newton3 optimization at all if we process the cells without the "alreadyProcessed"
  //  nodes?
  /**
   * CellFunctor to be used for the traversal defining the interaction between two cells.
   */
  internal::CellFunctor<Particle, ParticleCell, PairwiseFunctor, dataLayout, useNewton3, false> _cellFunctor;

  /**
   * Data Layout Converter to be used with this traversal
   */
  utils::DataLayoutConverter<PairwiseFunctor, dataLayout> _dataLayoutConverter;

  std::vector<OctreeNodeWrapper<Particle>> *_cells;
};

}  // namespace autopas