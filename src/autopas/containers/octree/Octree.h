/**
 * @file Octree.h
 *
 * @author Johannes Spies
 * @date 09.04.2021
 */

#pragma once

#include <cstdio>
#include <list>
#include <stack>

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/CellBasedParticleContainer.h"
#include "autopas/containers/CellBorderAndFlagManager.h"
#include "autopas/containers/LeavingParticleCollector.h"
#include "autopas/containers/octree/OctreeLeafNode.h"
#include "autopas/containers/octree/OctreeNodeInterface.h"
#include "autopas/containers/octree/OctreeNodeWrapper.h"
#include "autopas/containers/octree/traversals/OTTraversalInterface.h"
#include "autopas/iterators/ContainerIterator.h"
#include "autopas/utils/ParticleCellHelpers.h"
#include "autopas/utils/logging/OctreeLogger.h"

namespace autopas {

/**
 * The octree is a CellBasedParticleContainer that consists internally of two octrees. One for owned and one for halo
 * particles. It abuses the CellBasedParticleContainer::_cell vector to hold the root nodes for each.
 *
 * The tree consists of OctreeNodeWrapper objects, which
 *
 * @tparam Particle
 */
template <class Particle>
class Octree : public CellBasedParticleContainer<OctreeNodeWrapper<Particle>>,
               public internal::CellBorderAndFlagManager {
 public:
  /**
   * The particle cell used in this CellBasedParticleContainer
   */
  using ParticleCell = OctreeNodeWrapper<Particle>;

  /**
   * The particle type used in this container.
   */
  using ParticleType = typename ParticleCell::ParticleType;

  /**
   * This particle container contains two cells. Both cells are octrees.
   * - this->_cells[CellTypes::OWNED] yields the octree that contains all owned particles
   * - this->_cells[CellTypes::HALO] yields the octree that contains all halo particles
   */
  enum CellTypes { OWNED = 0, HALO = 1 };

  /**
   * Construct a new octree with two sub-octrees: One for the owned particles and one for the halo particles.
   * @param boxMin The minimum coordinate of the enclosing box
   * @param boxMax The maximum coordinate of the enclosing box
   * @param cutoff The cutoff radius
   * @param skinPerTimestep The skin radius per timestep
   * @param rebuildFrequency The Rebuild Frequency
   * @param cellSizeFactor The cell size factor
   */
  Octree(std::array<double, 3> boxMin, std::array<double, 3> boxMax, const double cutoff, const double skinPerTimestep,
         const unsigned int rebuildFrequency, const double cellSizeFactor)
      : CellBasedParticleContainer<ParticleCell>(boxMin, boxMax, cutoff, skinPerTimestep * rebuildFrequency) {
    // @todo Obtain this from a configuration, reported in https://github.com/AutoPas/AutoPas/issues/624
    int unsigned treeSplitThreshold = 16;

    double interactionLength = this->getInteractionLength();

    // Create the octree for the owned particles
    this->_cells.push_back(
        OctreeNodeWrapper<Particle>(boxMin, boxMax, treeSplitThreshold, interactionLength, cellSizeFactor));

    // Extend the halo region with cutoff + skin in all dimensions
    auto haloBoxMin = utils::ArrayMath::subScalar(boxMin, interactionLength);
    auto haloBoxMax = utils::ArrayMath::addScalar(boxMax, interactionLength);
    // Create the octree for the halo particles
    this->_cells.push_back(
        OctreeNodeWrapper<Particle>(haloBoxMin, haloBoxMax, treeSplitThreshold, interactionLength, cellSizeFactor));
  }

  [[nodiscard]] std::vector<ParticleType> updateContainer(bool keepNeighborListValid) override {
    // invalidParticles: all outside boxMin/Max
    std::vector<Particle> invalidParticles{};

    if (keepNeighborListValid) {
      invalidParticles = LeavingParticleCollector::collectParticlesAndMarkNonOwnedAsDummy(*this);
    } else {
      // This is a very primitive and inefficient way to rebuild the container:

      // @todo Make this less indirect. (Find a better way to iterate all particles inside the octree to change
      //   this function back to a function that actually copies all particles out of the octree.)
      //   The problem is captured by https://github.com/AutoPas/AutoPas/issues/622

      // 1. Copy all particles out of the container
      std::vector<Particle *> particleRefs;
      this->_cells[CellTypes::OWNED].collectAllParticles(particleRefs);
      std::vector<Particle> particles{};
      particles.reserve(particleRefs.size());

      for (auto *p : particleRefs) {
        if (p->isDummy()) {
          // don't do anything with dummies. They will just be dropped when the container is rebuilt.
          continue;
        } else if (utils::inBox(p->getR(), this->getBoxMin(), this->getBoxMax())) {
          particles.push_back(*p);
        } else {
          invalidParticles.push_back(*p);
        }
      }

      // 2. Clear the container
      this->deleteAllParticles();

      // 3. Insert the particles back into the container
      for (auto &particle : particles) {
        addParticleImpl(particle);
      }
    }

    return invalidParticles;
  }

  void iteratePairwise(TraversalInterface *traversal) override {
    if (auto *traversalInterface = dynamic_cast<OTTraversalInterface<ParticleCell> *>(traversal)) {
      traversalInterface->setCells(&this->_cells);
    }

    traversal->initTraversal();
    traversal->traverseParticlePairs();
    traversal->endTraversal();
  }

  /**
   * @copydoc ParticleContainerInterface::getContainerType()
   */
  [[nodiscard]] ContainerOption getContainerType() const override { return ContainerOption::octree; }

  /**
   * @copydoc ParticleContainerInterface::getParticleCellTypeEnum()
   * @note The octree stores the particles in its leaves. Even though the leaves inherit from `FullParticleCell`, they
   * are an extension of `FullParticleCell`. However, the interface stays the same and the leaves can be treated just
   * like regular `FullParticleCell`s.
   */
  [[nodiscard]] CellType getParticleCellTypeEnum() override { return CellType::FullParticleCell; }

  /**
   * @copydoc ParticleContainerInterface::addParticleImpl()
   */
  void addParticleImpl(const ParticleType &p) override { this->_cells[CellTypes::OWNED].addParticle(p); }

  /**
   * @copydoc ParticleContainerInterface::addHaloParticleImpl()
   */
  void addHaloParticleImpl(const ParticleType &haloParticle) override {
    ParticleType p_copy = haloParticle;
    p_copy.setOwnershipState(OwnershipState::halo);
    this->_cells[CellTypes::HALO].addParticle(p_copy);
  }

  /**
   * @copydoc ParticleContainerInterface::updateHaloParticle()
   */
  bool updateHaloParticle(const ParticleType &haloParticle) override {
    ParticleType pCopy = haloParticle;
    pCopy.setOwnershipState(OwnershipState::halo);
    return internal::checkParticleInCellAndUpdateByIDAndPosition(this->_cells[CellTypes::HALO], pCopy,
                                                                 this->getVerletSkin());
  }

  void rebuildNeighborLists(TraversalInterface *traversal) override {}

  std::tuple<const ParticleType *, size_t, size_t> getParticle(size_t cellIndex, size_t particleIndex,
                                                               IteratorBehavior iteratorBehavior,
                                                               const std::array<double, 3> &boxMin,
                                                               const std::array<double, 3> &boxMax) const override {
    // in this context cell == leaf cell
    // The index encodes the location in the octree. Each digit signifies which child of the node to enter. The right
    // most digit selects the tree, the next the child of the root, and so on.

    // shortcut if the given index doesn't exist
    if (cellIndex % 10 > 7) {
      return {nullptr, 0, 0};
    }

    // FIXME think about parallelism. This if currently disables it.
    if (autopas_get_thread_num() > 0 and not(iteratorBehavior & IteratorBehavior::forceSequential)) {
      return {nullptr, 0, 0};
    }

    // get referenced particle
    std::vector<size_t> currentCellIndex;
    // constant heuristic
    currentCellIndex.reserve(10);
    currentCellIndex.push_back(cellIndex % 10);
    cellIndex /= 10;
    OctreeNodeInterface<Particle> *currentCell = this->_cells[currentCellIndex.back()].getRaw();
    while (currentCell->hasChildren()) {
      currentCellIndex.push_back(cellIndex % 10);
      cellIndex /= 10;
      currentCell = currentCell->getChild(currentCellIndex.back());
    }
    const Particle *retPtr = &(dynamic_cast<OctreeLeafNode<Particle> *>(currentCell)->operator[](particleIndex));

    // Finding the indices for the next particle
    const size_t stride = (iteratorBehavior & IteratorBehavior::forceSequential) ? 1 : autopas_get_num_threads();

    // FIXME: Region iter
    do {
      // If cell has wrong type, or there are no more particles in this cell jump to the next
      if (++particleIndex >= currentCell->getNumberOfParticles()) {
        // Move up until there are siblings left
        while (currentCellIndex.back() == 7) {
          currentCell = currentCell->getParent();
          currentCellIndex.pop_back();
        }
        // Go to the next child of the same parent
        ++currentCellIndex.back();
        // If we notice that there is nothing else to look at set invalid values, so we get a nullptr next time and
        // break.
        if (currentCellIndex[0] > (iteratorBehavior & IteratorBehavior::owned) ? CellTypes::OWNED : CellTypes::HALO) {
          cellIndex = 8;
          break;
        }
        currentCell = currentCell->getParent()->getChild(currentCellIndex.back());
        particleIndex = 0;
      }

      // Repeat this as long as the current particle is not interesting.
      //  - coordinates are in region of interest
      //  - ownership fits to the iterator behavior
    } while (
        not utils::inBox((dynamic_cast<OctreeLeafNode<Particle> *>(currentCell)->operator[](particleIndex)).getR(),
                         boxMin, boxMax) or
        not(static_cast<unsigned int>((dynamic_cast<OctreeLeafNode<Particle> *>(currentCell)->operator[](particleIndex))
                                          .getOwnershipState()) &
            static_cast<unsigned int>(iteratorBehavior)));

    return {retPtr, cellIndex, particleIndex};
  }

  bool deleteParticle(ParticleType &particle) override {
    if (particle.isOwned()) {
      return this->_cells[CellTypes::OWNED].deleteParticle(particle);
    } else if (particle.isHalo()) {
      return this->_cells[CellTypes::HALO].deleteParticle(particle);
    } else {
      utils::ExceptionHandler::exception("Particle to be deleted is neither owned nor halo!\n" + particle.toString());
      return false;
    }
  }

  [[nodiscard]] ContainerIterator<ParticleType, true, false> begin(
      IteratorBehavior behavior,
      typename ContainerIterator<ParticleType, true, false>::ParticleVecType *additionalVectors = nullptr) override {
    return ContainerIterator<ParticleType, true, false>(*this, behavior, additionalVectors);
  }

  [[nodiscard]] ContainerIterator<ParticleType, false, false> begin(
      IteratorBehavior behavior,
      typename ContainerIterator<ParticleType, false, false>::ParticleVecType *additionalVectors =
          nullptr) const override {
    return ContainerIterator<ParticleType, false, false>(*this, behavior, additionalVectors);
  }

  [[nodiscard]] ContainerIterator<ParticleType, true, true> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner, IteratorBehavior behavior,
      typename ContainerIterator<Particle, true, true>::ParticleVecType *additionalVectors) override {
    return ContainerIterator<ParticleType, true, true>(*this, behavior, additionalVectors, lowerCorner, higherCorner);
  }

  [[nodiscard]] ContainerIterator<ParticleType, false, true> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner, IteratorBehavior behavior,
      typename ContainerIterator<Particle, false, true>::ParticleVecType *additionalVectors) const override {
    return ContainerIterator<ParticleType, false, true>(*this, behavior, additionalVectors, lowerCorner, higherCorner);
  }

  /**
   * @copydoc ParticleContainerInterface::getTraversalSelectorInfo()
   */
  [[nodiscard]] TraversalSelectorInfo getTraversalSelectorInfo() const override {
    // this is a dummy since it is not actually used
    std::array<unsigned long, 3> dims = {1, 1, 1};
    std::array<double, 3> cellLength = utils::ArrayMath::sub(this->getBoxMax(), this->getBoxMin());
    return TraversalSelectorInfo(dims, this->getInteractionLength(), cellLength, 0);
  }

  /**
   * Get the number of particles that belong to this octree. (Owned and and halo.)
   *
   * @return The integer # of particles in the container
   */
  [[nodiscard]] unsigned long getNumberOfParticles() const override {
    return this->_cells[CellTypes::OWNED].numParticles() + this->_cells[CellTypes::HALO].numParticles();
  }

  void deleteHaloParticles() override { this->_cells[CellTypes::HALO].clear(); }

  [[nodiscard]] bool cellCanContainHaloParticles(std::size_t i) const override {
    if (i > 1) {
      throw std::runtime_error("[Octree.h]: This cell container (octree) contains only two cells");
    }
    return i == CellTypes::HALO;
  }

  [[nodiscard]] bool cellCanContainOwnedParticles(std::size_t i) const override {
    if (i > 1) {
      throw std::runtime_error("[Octree.h]: This cell container (octree) contains only two cells");
    }
    return i == CellTypes::OWNED;
  }

  /**
   * Execute code on all particles in this container as defined by a lambda function.
   * @tparam Lambda (Particle &p) -> void
   * @param forEachLambda code to be executed on all particles
   * @param behavior @see IteratorBehavior
   */
  template <typename Lambda>
  void forEach(Lambda forEachLambda, IteratorBehavior behavior = IteratorBehavior::ownedOrHalo) {
    if (behavior & IteratorBehavior::owned) this->_cells[OWNED].forEach(forEachLambda);
    if (behavior & IteratorBehavior::halo) this->_cells[HALO].forEach(forEachLambda);
    if (not(behavior & IteratorBehavior::ownedOrHalo))
      utils::ExceptionHandler::exception("Encountered invalid iterator behavior!");
  }

  /**
   * Reduce properties of particles as defined by a lambda function.
   * @tparam Lambda (Particle p, A initialValue) -> void
   * @tparam A type of particle attribute to be reduced
   * @param reduceLambda code to reduce properties of particles
   * @param result reference to result of type A
   * @param behavior @see IteratorBehavior default: @see IteratorBehavior::ownedOrHalo
   */
  template <typename Lambda, typename A>
  void reduce(Lambda reduceLambda, A &result, IteratorBehavior behavior = IteratorBehavior::ownedOrHalo) {
    if (behavior & IteratorBehavior::owned) this->_cells[OWNED].reduce(reduceLambda, result);
    if (behavior & IteratorBehavior::halo) this->_cells[HALO].reduce(reduceLambda, result);
    if (not(behavior & IteratorBehavior::ownedOrHalo))
      utils::ExceptionHandler::exception("Encountered invalid iterator behavior!");
  }

  /**
   * @copydoc LinkedCells::forEachInRegion()
   */
  template <typename Lambda>
  void forEachInRegion(Lambda forEachLambda, const std::array<double, 3> &lowerCorner,
                       const std::array<double, 3> &higherCorner, IteratorBehavior behavior) {
    if (behavior & IteratorBehavior::owned)
      this->_cells[OWNED].forEachInRegion(forEachLambda, lowerCorner, higherCorner);
    if (behavior & IteratorBehavior::halo) this->_cells[HALO].forEachInRegion(forEachLambda, lowerCorner, higherCorner);
    if (not(behavior & IteratorBehavior::ownedOrHalo))
      utils::ExceptionHandler::exception("Encountered invalid iterator behavior!");
  }

  /**
   * @copydoc LinkedCells::reduceInRegion()
   */
  template <typename Lambda, typename A>
  void reduceInRegion(Lambda reduceLambda, A &result, const std::array<double, 3> &lowerCorner,
                      const std::array<double, 3> &higherCorner, IteratorBehavior behavior) {
    if (behavior & IteratorBehavior::owned)
      this->_cells[OWNED].reduceInRegion(reduceLambda, result, lowerCorner, higherCorner);
    if (behavior & IteratorBehavior::halo)
      this->_cells[HALO].reduceInRegion(reduceLambda, result, lowerCorner, higherCorner);
    if (not(behavior & IteratorBehavior::ownedOrHalo))
      utils::ExceptionHandler::exception("Encountered invalid iterator behavior!");
  }

 private:
  /**
   * A logger that can be called to log the octree data structure.
   */
  OctreeLogger<Particle> logger;

  double skin;
};

}  // namespace autopas
