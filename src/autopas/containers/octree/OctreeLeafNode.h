/**
 * @file OctreeLeafNode.h
 *
 * @author Johannes Spies
 * @date 15.04.2021
 */

#pragma once

#include <optional>

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/octree/OctreeInnerNode.h"
#include "autopas/containers/octree/OctreeNodeInterface.h"
#include "autopas/utils/ArrayMath.h"

namespace autopas {
template <typename Particle>
class OctreeInnerNode;

/**
 * An octree leaf node. This class utilizes the FullParticleCell to store the actual particles.
 *
 * @tparam Particle
 */
template <typename Particle>
class OctreeLeafNode : public OctreeNodeInterface<Particle>, public FullParticleCell<Particle> {
 public:
  /**
   * Create an empty octree leaf node
   * @param boxMin The min coordinate of the octree box
   * @param boxMax The max coordinate of the octree box
   * @param parent A pointer to the parent node. Should be nullptr for root nodes.
   * @param treeSplitThreshold Maximum number of particles inside a leaf before it tries to split itself
   * @param interactionLength The minimum distance at which a force is considered nonzero, cutoff+skin.
   * @param cellSizeFactor The cell size factor
   */
  OctreeLeafNode(const std::array<double, 3> &boxMin, const std::array<double, 3> &boxMax,
                 OctreeNodeInterface<Particle> *parent, const int unsigned treeSplitThreshold,
                 const double interactionLength, const double cellSizeFactor)
      : OctreeNodeInterface<Particle>(boxMin, boxMax, parent, treeSplitThreshold, interactionLength, cellSizeFactor),
        FullParticleCell<Particle>(utils::ArrayMath::sub(boxMax, boxMin)) {}

  /**
   * Copy a leaf by copying all particles from the other leaf to this leaf.
   * @param other The leaf to copy from
   */
  OctreeLeafNode(OctreeLeafNode<Particle> const &other)
      : OctreeNodeInterface<Particle>(other._boxMin, other._boxMax, other._parent, other._treeSplitThreshold,
                                      other._interactionLength),
        FullParticleCell<Particle>(utils::ArrayMath::sub(other._boxMax, other._boxMin)),
        _id(other.getID()) {
    this->_particles.reserve(other._particles.size());
    for (auto &p : other._particles) {
      this->_particles.push_back(p);
    }
  }

  /**
   * @copydoc OctreeNodeInterface::insert()
   */
  std::unique_ptr<OctreeNodeInterface<Particle>> insert(const Particle &p) override {
    using namespace autopas::utils::ArrayMath::literals;

    // Check if the size of the new leaves would become smaller than cellSizeFactor*interactionLength
    std::array<double, 3> splitLeafDimensions = this->getBoxMax() - this->getBoxMin();
    splitLeafDimensions *= 0.5;
    bool anyNewDimSmallerThanMinSize = false;
    for (auto d = 0; d < 3; ++d) {
      // auto cellSizeFactor = 1.0;
      // @todo The condition below should actually be
      //   splitLeafDimensions[d] < (this->_cellSizeFactor * this->_interactionLength)
      //   But with this condition, the TraversalComparison test fails for cell size factor 0.5. Find out why the octree
      //   cannot handle this value. This problem is addressed in https://github.com/AutoPas/AutoPas/issues/658.
      if (splitLeafDimensions[d] < this->_interactionLength) {
        anyNewDimSmallerThanMinSize = true;
        break;
      }
    }

    if ((this->_particles.size() < this->_treeSplitThreshold) or anyNewDimSmallerThanMinSize) {
      this->_particles.push_back(p);
      return nullptr;
    } else {
      std::unique_ptr<OctreeNodeInterface<Particle>> newInner = std::make_unique<OctreeInnerNode<Particle>>(
          this->getBoxMin(), this->getBoxMax(), this->_parent, this->_treeSplitThreshold, this->_interactionLength,
          this->_cellSizeFactor);
      auto ret = newInner->insert(p);
      if (ret) newInner = std::move(ret);
      for (auto cachedParticle : this->_particles) {
        ret = newInner->insert(cachedParticle);
        if (ret) newInner = std::move(ret);
      }

      return newInner;
    }
  }

  bool deleteParticle(Particle &particle) override {
    const bool isRearParticle = &particle == &this->_particles.back();
    // WARNING no runtime check that this particle is actually within the node!
    particle = this->_particles.back();
    this->_particles.pop_back();
    return not isRearParticle;
  }

  /**
   * @copydoc OctreeNodeInterface::collectAllParticles()
   */
  void collectAllParticles(std::vector<Particle *> &ps) const override {
    ps.reserve(ps.size() + this->_particles.size());
    for (auto &particle : this->_particles) {
      // The cast here is required to remove the `const` modifier from the `Particle const &`. The particle should be
      // modifiable outside the octree and should therefore not be marked `const`, which requires the cast.
      ps.push_back((Particle *)&particle);
    }
  }

  /**
   * @copydoc OctreeNodeInterface::appendAllLeafBoxes()
   */
  void appendAllLeafBoxes(std::vector<std::pair<std::array<double, 3>, std::array<double, 3>>> &boxes) const override {
    auto minMax = std::make_pair(this->getBoxMin(), this->getBoxMax());
    boxes.push_back(minMax);
  }

  /**
   * @copydoc OctreeNodeInterface::clearChildren()
   */
  void clearChildren(std::unique_ptr<OctreeNodeInterface<Particle>> &ref) override { this->_particles.clear(); }

  /**
   * @copydoc OctreeNodeInterface::getNumberOfParticles()
   */
  unsigned int getNumberOfParticles() const override { return this->_particles.size(); }

  /**
   * @copydoc OctreeNodeInterface::hasChildren()
   */
  bool hasChildren() override { return false; }

  /**
   * @copydoc OctreeNodeInterface::getChild()
   */
  OctreeNodeInterface<Particle> *getChild(int index) override {
    throw std::runtime_error("[OctreeLeafNode::getChild()] Unable to return child by index in leaf");
  }

  std::vector<OctreeLeafNode<Particle> *> getLeavesFromDirections(
      const std::vector<octree::Vertex> &directions) override {
    return {this};
  }

  OctreeNodeInterface<Particle> *SON(octree::Octant O) override {
    throw std::runtime_error("Unable to get SON of leaf node");
  }

  void appendAllLeaves(std::vector<OctreeLeafNode<Particle> *> &leaves) const override {
    leaves.push_back((OctreeLeafNode<Particle> *)this);
  }

  std::set<OctreeLeafNode<Particle> *> getLeavesInRange(const std::array<double, 3> &min,
                                                        const std::array<double, 3> &max) override {
    if (this->getEnclosedVolumeWith(min, max) > 0.0) {
      return {this};
    } else {
      return {};
    }
  }

  /**
   * Get the assigned id of this leaf node
   * @return An ID (or -1 if there was no ID assigned to this node)
   */
  int getID() { return _id; }

  /**
   * Set the ID of this node
   * @param id An integer ID
   */
  void setID(int id) { this->_id = id; }

 private:
  /**
   * The ID assigned to this node (-1 if unassigned)
   */
  int _id{-1};
};
}  // namespace autopas
