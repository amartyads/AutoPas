/**
 * @file ClusterTower.h
 * @author humig
 * @date 27.07.19
 */

#pragma once

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/ParticleDeletedObserver.h"
#include "autopas/containers/verletClusterLists/Cluster.h"
#include "autopas/particles/OwnershipState.h"

namespace autopas::internal {

/**
 * This class represents one tower for clusters in the VerletClusterLists container.
 *
 * A ClusterTower contains multiple clusters that are stacked on top (z-direction) of each other. It saves all particles
 * in a FullParticleCell, provides methods to generate and work on the clusters contained, and handles the generation of
 * dummy particles to make sure that each cluster is full.
 *
 * Only the last cluster of the ClusterTower is filled up with dummy particles, since all others are guaranteed to
 * already be full.
 *
 * To use this container:
 *  1. Use addParticle() to add all particles you want.
 *  2. Then call generateClusters(). This copies the last particle as often as it is necessary to fill up the last
 * cluster. (maximum clusterSize-1 times).
 *  3. Generate your neighbor lists somehow.
 *  4. Call fillUpWithDummyParticles() to replace the copies of the last particle made in generateClusters() with
 * dummies.
 *
 * If you want to add more particles after calling generateClusters(), definitely make sure to call clear() before
 * calling addParticle() again, since doing otherwise will mess up the dummy particles and actual particles will likely
 * get lost.
 *
 * @tparam Particle
 * @tparam clusterSize
 */
template <class Particle, size_t clusterSize>
class ClusterTower : public ParticleCell<Particle> {
 public:
  /**
   * Adds a particle to the cluster tower. If generateClusters() has already been called on this ClusterTower, clear()
   * must be called first, or dummies are messed up and particles get lost!
   *
   * Is allowed to be called in parallel since a lock is used on the internal cell.
   *
   * @param particle The particle to add.
   */
  void addParticle(const Particle &particle) override { _particles.addParticle(particle); }

  /**
   * Clears all particles from the tower and resets it to be ready for new particles.
   */
  void clear() override {
    _clusters.clear();
    _particles.clear();
    _numDummyParticles = 0;
  }

  /**
   * Generates the clusters for the particles in this cluster tower.
   *
   * Copies the last particle as often as necessary to fill up the last cluster. This makes sure that iteration over
   * clusters already works after this, while the bounding box of the last cluster is also not messed up by dummy
   * particles. This is necessary for rebuilding the neighbor lists.
   *
   * @return Returns the number of clusters in the tower.
   */
  size_t generateClusters() {
    if (getNumActualParticles() > 0) {
      _particles.sortByDim(2);

      auto sizeLastCluster = (_particles.numParticles() % clusterSize);
      _numDummyParticles = sizeLastCluster != 0 ? clusterSize - sizeLastCluster : 0;

      const auto lastParticle = _particles[_particles.numParticles() - 1];
      for (size_t i = 0; i < _numDummyParticles; i++) {
        _particles.addParticle(lastParticle);
      }

      size_t numClusters = _particles.numParticles() / clusterSize;
      _clusters.reserve(numClusters);
      for (size_t index = 0; index < numClusters; index++) {
        _clusters.emplace_back(&(_particles[clusterSize * index]));
      }
    }

    return getNumClusters();
  }

  /**
   * Replaces the copies of the last particle made in generateClusters() with dummies. Dummy particles have ID 0.
   *
   * @param dummyStartX The x-coordinate for all dummies.
   * @param dummyDistZ The distance in z-direction that all generated dummies will have from each other.
   */
  void fillUpWithDummyParticles(double dummyStartX, double dummyDistZ) {
    auto &lastCluster = getCluster(getNumClusters() - 1);
    for (size_t index = 1; index <= _numDummyParticles; index++) {
      lastCluster[clusterSize - index] = lastCluster[0];  // use first Particle in last cluster as dummy particle!
      lastCluster[clusterSize - index].setOwnershipState(OwnershipState::dummy);
      lastCluster[clusterSize - index].setR({dummyStartX, 0, dummyDistZ * index});
      lastCluster[clusterSize - index].setID(std::numeric_limits<size_t>::max());
    }
  }

  /**
   * More or less inverse operation of fillUpWithDummyParticles().
   * It sets the positions of the dummy particles to the position of the last actual particle in the tower.
   */
  void setDummyParticlesToLastActualParticle() {
    if (_numDummyParticles > 0) {
      auto &lastCluster = getCluster(getNumClusters() - 1);
      auto lastActualParticle = lastCluster[clusterSize - _numDummyParticles - 1];
      for (size_t index = 1; index <= _numDummyParticles; index++) {
        lastCluster[clusterSize - index] = lastActualParticle;
      }
    }
  }

  /**
   * Loads the particles into the SoA stored in this tower and generates the SoAView for each cluster.
   * @tparam Functor The type of the functor to use.
   * @param functor The functor to use for loading the particles into the SoA.
   */
  template <class Functor>
  void loadSoA(Functor *functor) {
    functor->SoALoader(_particles, _particles._particleSoABuffer);
    for (size_t index = 0; index < getNumClusters(); index++) {
      auto &cluster = getCluster(index);
      cluster.setSoAView({&(_particles._particleSoABuffer), index * clusterSize, (index + 1) * clusterSize});
    }
  }

  /**
   * Extracts the SoA into the particles/clusters.
   * @tparam Functor The type of the functor to use.
   * @param functor The functor to use for extracting the SoA into the particles/clusters.
   */
  template <class Functor>
  void extractSoA(Functor *functor) {
    functor->SoAExtractor(_particles, _particles._particleSoABuffer);
  }

  /**
   * Returns a rvalue reference to a std::vector containing all particles of this tower that are not dummies.
   *
   * clear() has to called afterwards!
   * @return
   */
  std::vector<Particle> &&collectAllActualParticles() {
    if (not _particles._particles.empty()) {
      // Workaround to remove requirement of default constructible particles.
      // This function will always only shrink the array, particles are not actually inserted.
      _particles._particles.resize(getNumActualParticles(), _particles._particles[0]);
    }
    return std::move(_particles._particles);
  }

  /**
   * Returns the number of dummy particles in the tower (that all are in the last cluster).
   * @return the number of dummy particles in the tower.
   */
  [[nodiscard]] size_t getNumDummyParticles() const { return _numDummyParticles; }

  /**
   * Returns the number of particles in the tower that are not dummies.
   * @return the number of particles in the tower that are not dummies.
   */
  [[nodiscard]] size_t getNumActualParticles() const { return _particles.numParticles() - _numDummyParticles; }

  /**
   * Returns the number of clusters in the tower.
   * @return the number of clusters in the tower.
   */
  [[nodiscard]] size_t getNumClusters() const { return _clusters.size(); }

  /**
   * Returns a reference to the std::vector holding the clusters of this container.
   * @return a reference to the std::vector holding the clusters of this container.
   */
  [[nodiscard]] auto &getClusters() { return _clusters; }

  /**
   * Returns the cluster at position index.
   * @param index The index of the cluster to return.
   * @return the cluster at position index.
   */
  [[nodiscard]] auto &getCluster(size_t index) { return _clusters[index]; }

  /**
   * @copydoc getCluster(size_t)
   */
  [[nodiscard]] auto &getCluster(size_t index) const { return _clusters[index]; }

  /**
   * @copydoc getNumActualParticles()
   */
  [[nodiscard]] unsigned long numParticles() const override { return getNumActualParticles(); }

  /**
   * Returns an iterator over all non-dummy particles contained in this tower.
   * @return an iterator over all non-dummy particles contained in this tower.
   */
  [[nodiscard]] SingleCellIteratorWrapper<Particle, true> begin() override {
    return SingleCellIteratorWrapper<Particle, true>{
        new SingleCellIterator<Particle, ClusterTower<Particle, clusterSize>, true>(this)};
  }

  /**
   * Returns an iterator over all non-dummy particles contained in this tower.
   * @return an iterator over all non-dummy particles contained in this tower.
   */
  [[nodiscard]] SingleCellIteratorWrapper<Particle, false> begin() const override {
    return SingleCellIteratorWrapper<Particle, false>{
        new SingleCellIterator<Particle, ClusterTower<Particle, clusterSize>, false>(this)};
  }

  /**
   * Returns the particle at position index. Needed by SingleCellIterator.
   * @param index the position of the particle to return.
   * @return the particle at position index.
   */
  Particle &at(size_t index) { return _particles._particles.at(index); }

  /**
   * Returns the const particle at position index. Needed by SingleCellIterator.
   * @param index the position of the particle to return.
   * @return the particle at position index.
   */
  const Particle &at(size_t index) const { return _particles._particles.at(index); }

  // Methods from here on: Only to comply with ParticleCell interface. SingleCellIterators work on ParticleCells, and
  // while those methods would not be needed, still complying to the whole interface should be helpful, if
  // maybe someday new necessary pure virtual methods are introduced there.

  [[nodiscard]] bool isNotEmpty() const override { return getNumActualParticles() > 0; }

  void deleteDummyParticles() override {
    _particles.deleteDummyParticles();
    _numDummyParticles = 0;
  }

  void deleteByIndex(size_t index) override {
    /// @note The implementation of this function prevents a regionIterator to make sorted assumptions of particles
    /// inside a cell! supporting this would mean that the deleted particle should be swapped to the end of the valid
    /// particles. See also https://github.com/AutoPas/AutoPas/issues/435

    // swap particle that should be deleted to end of actual particles.
    std::swap(_particles._particles[index], _particles._particles[getNumActualParticles() - 1]);
    if (getNumDummyParticles() != 0) {
      // swap particle that should be deleted (now at end of actual particles) with last dummy particle.
      std::swap(_particles._particles[getNumActualParticles() - 1],
                _particles._particles[_particles._particles.size() - 1]);
    }
    _particles._particles.pop_back();

    if (_particleDeletionObserver) {
      _particleDeletionObserver->notifyParticleDeleted();
    }
  }

  void setCellLength(std::array<double, 3> &) override {
    autopas::utils::ExceptionHandler::exception("ClusterTower::setCellLength(): Not supported!");
  }

  [[nodiscard]] std::array<double, 3> getCellLength() const override {
    autopas::utils::ExceptionHandler::exception("ClusterTower::getCellLength(): Not supported!");
    return {0, 0, 0};
  }

  /**
   * Set the ParticleDeletionObserver, which is called, when a particle is deleted.
   * @param observer
   */
  void setParticleDeletionObserver(internal::ParticleDeletedObserver *observer) {
    _particleDeletionObserver = observer;
  };

 private:
  /**
   * The clusters that are contained in this tower.
   */
  std::vector<Cluster<Particle, clusterSize>> _clusters;
  /**
   * The particle cell to store the particles and SoA for this tower.
   */
  FullParticleCell<Particle> _particles;
  /**
   * The number of dummy particles in this tower.
   */
  size_t _numDummyParticles{};

  internal::ParticleDeletedObserver *_particleDeletionObserver{nullptr};
};

}  // namespace autopas::internal
