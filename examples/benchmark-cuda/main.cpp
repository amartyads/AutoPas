/**
 * @file main.cpp
 * @date 10.02.2019
 * @author jspahl
 */

#include <chrono>
#include <iostream>
#include "autopas/autopasIncludes.h"
#include "autopas/containers/directSum/DirectSumTraversal.h"
#include "autopas/pairwiseFunctors/LJFunctor.h"

using namespace std;
using namespace autopas;

class MyMolecule : public Particle {
 public:
  MyMolecule() : Particle(), _myvar(0) {}

  MyMolecule(std::array<double, 3> r, std::array<double, 3> v, unsigned long i, int myvar)
      : Particle(r, v, i), _myvar(myvar) {}

  void print() {
    cout << "Molecule with position: ";
    for (auto &r : getR()) {
      cout << r << ", ";
    }
    cout << "and force: ";

    for (auto &f : getF()) {
      cout << f << ", ";
    }
    cout << "ID: " << getID();
    cout << " myvar: " << _myvar << endl;
  }

 private:
  int _myvar;
};

template <class Cell>
void fillSpaceWithGrid(Cell &pc, std::array<double, 3> boxMin, std::array<double, 3> boxMax, double gridsize) {
  int i = 0;

  for (double x = boxMin[0]; x < boxMax[0]; x += gridsize) {
    for (double y = boxMin[1]; y < boxMax[1]; y += gridsize) {
      for (double z = boxMin[2]; z < boxMax[2]; z += gridsize) {
        std::array<double, 3> arr({x, y, z});
        MyMolecule m(arr, {0., 0., 0.}, static_cast<unsigned long>(i), i);
        pc.addParticle(m);
        ++i;
      }
    }
  }
}

int main() {
  autopas::Logger::create();
  int maxIterations = 100;

  std::array<double, 3> boxMin({0., 0., 0.}), boxMax({10., 10., 10.});
  double cutoff = 3.0;
  double epsilon = 2.0;
  double sigma = 0.5;

  DirectSum<MyMolecule, FullParticleCell<MyMolecule>> dir(boxMin, boxMax, cutoff);
  fillSpaceWithGrid<>(dir, boxMin, boxMax, 0.7);

  typedef LJFunctor<MyMolecule, FullParticleCell<MyMolecule>> Func;

  Func func(cutoff, epsilon, sigma, 0.0);

  DirectSumTraversal<FullParticleCell<MyMolecule>, Func, true, false, true> traversal(&func);

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < maxIterations; ++i) {
    dir.iteratePairwiseSoACuda(&func, &traversal, false);
  }

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();

  cout << maxIterations << " iterations with " << dir.getNumParticles() << " particles took: " << duration
       << "microseconds" << endl;
  return EXIT_SUCCESS;
}
