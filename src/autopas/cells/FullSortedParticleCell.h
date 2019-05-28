/**
 * @file FullSortedParticleCell.h
 * @date 18.01.2018
 * @author C. Menges
 */

#pragma once

#include <vector>
#include "autopas/cells/FullParticleCell.h"
#include "autopas/cells/ParticleCell.h"
#include "autopas/iterators/SingleCellIterator.h"
#include "autopas/utils/ArrayMath.h"
#include "autopas/utils/CudaSoA.h"
#include "autopas/utils/SoA.h"
#include "autopas/utils/WrapOpenMP.h"

namespace autopas {

/**
 * This class handles the storage of particles in their full form.
 * @tparam Particle
 */
template <class Particle>
class FullSortedParticleCell : public ParticleCell<Particle> {
 public:
  FullSortedParticleCell(ParticleCell<Particle>& cell, const std::array<double, 3>& r) : _cell(&cell) {
    _particles.reserve(cell.numParticles());
    for (auto p = cell.begin(); p.isValid(); ++p) {
      _particles.push_back(std::make_pair(ArrayMath::dot(p->getR(), r), &(*p)));
    }
    std::sort(_particles.begin(), _particles.end(),
              [](const auto& a, const auto& b) -> bool { return a.first < b.first; });
  }

  void addParticle(Particle& m) override {}

  virtual SingleCellIteratorWrapper<Particle> begin() override { return _cell->begin(); }

  unsigned long numParticles() const override { return _particles.size(); }

  bool isNotEmpty() const override { return numParticles() > 0; }

  void clear() override { _particles.clear(); }

  void deleteByIndex(size_t index) override {
    assert(index < numParticles());

    if (index < numParticles() - 1) {
      std::swap(_particles[index], _particles[numParticles() - 1]);
    }
    _particles.pop_back();
  }

  /**
   * type of the internal iterator
   */
  typedef internal::SingleCellIterator<Particle, FullSortedParticleCell<Particle>> iterator_t;
  std::vector<std::pair<double, Particle*>> _particles;

  ParticleCell<Particle>* _cell;

 private:
};
}  // namespace autopas
