/**
 * @file VerletListsTest.cpp
 * @author seckler
 * @date 19.04.18
 */

#include "VerletListsTest.h"
#include "mocks/MockFunctor.h"
#include "mocks/MockVerletLists.h"

TEST_F(VerletListsTest, VerletListConstructor) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);
}

TEST_F(VerletListsTest, testVerletListBuild) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {2, 2, 2};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addParticle(p);
  std::array<double, 3> r2 = {1.5, 2, 2};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  EXPECT_EQ(list.size(), 2);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  EXPECT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testVerletList) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {2, 2, 2};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addParticle(p);
  std::array<double, 3> r2 = {1.5, 2, 2};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  MockFunctor<autopas::Particle, autopas::FullParticleCell<autopas::Particle>>
      mockFunctor;
  using ::testing::_;  // anything is ok
  EXPECT_CALL(mockFunctor, AoSFunctor(_, _, true));

  verletLists.iteratePairwiseAoS2(&mockFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  EXPECT_EQ(list.size(), 2);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  EXPECT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testVerletListInSkin) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {1.4, 2, 2};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addParticle(p);
  std::array<double, 3> r2 = {2.5, 2, 2};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  MockFunctor<autopas::Particle, autopas::FullParticleCell<autopas::Particle>>
      mockFunctor;
  using ::testing::_;  // anything is ok
  EXPECT_CALL(mockFunctor, AoSFunctor(_, _, true));

  verletLists.iteratePairwiseAoS2(&mockFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  EXPECT_EQ(list.size(), 2);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  EXPECT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testVerletListBuildTwice) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {2, 2, 2};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addParticle(p);
  std::array<double, 3> r2 = {1.5, 2, 2};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  EXPECT_EQ(list.size(), 2);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  EXPECT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testVerletListBuildFarAway) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {5, 5, 5};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {2, 2, 2};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addParticle(p);

  std::array<double, 3> r2 = {1.5, 2, 2};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  std::array<double, 3> r3 = {4.5, 4.5, 4.5};
  autopas::Particle p3(r3, {0., 0., 0.}, 2);
  verletLists.addParticle(p3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  ASSERT_EQ(list.size(), 3);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  ASSERT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testVerletListBuildHalo) {
  std::array<double, 3> min = {1, 1, 1};
  std::array<double, 3> max = {3, 3, 3};
  double cutoff = 1.;
  double skin = 0.2;
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists(min, max, cutoff, skin);

  std::array<double, 3> r = {0.9, 0.9, 0.9};
  autopas::Particle p(r, {0., 0., 0.}, 0);
  verletLists.addHaloParticle(p);
  std::array<double, 3> r2 = {1.1, 1.1, 1.1};
  autopas::Particle p2(r2, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  verletLists.iteratePairwiseAoS2(&emptyFunctor, true);

  auto& list = verletLists.getVerletListsAoS();

  ASSERT_EQ(list.size(), 2);
  EXPECT_EQ(list.size(), 2);
  int partners = 0;
  for (auto i : list) {
    partners += i.second.size();
  }
  ASSERT_EQ(partners, 1);
}

TEST_F(VerletListsTest, testRebuildFrequencyAlways) {
  MockVerletLists<autopas::Particle,
                  autopas::FullParticleCell<autopas::Particle>>
      mockVerletLists({0., 0., 0.}, {10., 10., 10.}, 1., 0.3, 1);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(4);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
}

TEST_F(VerletListsTest, testRebuildFrequencyEvery3) {
  MockVerletLists<autopas::Particle,
                  autopas::FullParticleCell<autopas::Particle>>
      mockVerletLists({0., 0., 0.}, {10., 10., 10.}, 1., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // 1
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // 2
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // 3
}

TEST_F(VerletListsTest, testForceRebuild) {
  using ::testing::_;
  using ::testing::Invoke;

  MockVerletLists<autopas::Particle,
                  autopas::FullParticleCell<autopas::Particle>>
      mockVerletLists({0., 0., 0.}, {10., 10., 10.}, 1., 0.3, 3);

  ON_CALL(mockVerletLists, addParticle(_))
      .WillByDefault(Invoke(
          &mockVerletLists,
          &MockVerletLists<autopas::Particle,
                           autopas::FullParticleCell<autopas::Particle>>::
              addParticleVerletLists));
  ON_CALL(mockVerletLists, addHaloParticle(_))
      .WillByDefault(Invoke(
          &mockVerletLists,
          &MockVerletLists<autopas::Particle,
                           autopas::FullParticleCell<autopas::Particle>>::
              addHaloParticleVerletLists));

  ON_CALL(mockVerletLists, updateContainer())
      .WillByDefault(Invoke(
          &mockVerletLists,
          &MockVerletLists<autopas::Particle,
                           autopas::FullParticleCell<autopas::Particle>>::
              updateContainerVerletLists));

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // here
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);

  EXPECT_CALL(mockVerletLists, updateContainer());
  mockVerletLists.updateContainer();
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // here
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);

  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  EXPECT_CALL(mockVerletLists, addParticle(_));
  mockVerletLists.addParticle(p);
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // here
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // here
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(0);

  autopas::Particle p2({-0.1, 1.2, 1.1}, {0., 0., 0.}, 2);
  EXPECT_CALL(mockVerletLists, addHaloParticle(_));
  mockVerletLists.addHaloParticle(p2);
  EXPECT_CALL(mockVerletLists, updateVerletListsAoS(true)).Times(1);
  mockVerletLists.iteratePairwiseAoS(&emptyFunctor, true);  // here
}

TEST_F(VerletListsTest, testCheckNeighborListsAreValidAfterBuild) {
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists({0., 0., 0.}, {10., 10., 10.}, 2., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;

  // addtwo particles in proper distance
  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p);
  autopas::Particle p2({3.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p2);

  // this will build the verlet list
  verletLists.iteratePairwiseAoS2(&emptyFunctor);

  // check validity - should return true
  EXPECT_TRUE(verletLists.checkNeighborListsAreValid());
}

TEST_F(VerletListsTest, testCheckNeighborListsAreValidAfterSmallMove) {
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists({0., 0., 0.}, {10., 10., 10.}, 2., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;

  // addtwo particles in proper distance
  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p);
  autopas::Particle p2({3.5, 1.1, 1.1}, {0., 0., 0.}, 2);
  verletLists.addParticle(p2);

  // this will build the verlet list
  verletLists.iteratePairwiseAoS2(&emptyFunctor);

  for (auto iter = verletLists.begin(); iter.isValid(); ++iter) {
    if (iter->getID() == 1) {
      iter->setR({1.4, 1.1, 1.1});
    }
  }

  // check validity - should return true
  EXPECT_TRUE(verletLists.checkNeighborListsAreValid());
}

TEST_F(VerletListsTest, testCheckNeighborListsAreInvalidAfterMoveLarge) {
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists({0., 0., 0.}, {10., 10., 10.}, 2., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;

  // addtwo particles in proper distance
  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p);
  autopas::Particle p2({3.5, 1.1, 1.1}, {0., 0., 0.}, 2);
  verletLists.addParticle(p2);

  // this will build the verlet list
  verletLists.iteratePairwiseAoS2(&emptyFunctor);

  for (auto iter = verletLists.begin(); iter.isValid(); ++iter) {
    if (iter->getID() == 1) {
      iter->setR({1.6, 1.1, 1.1});
    }
  }

  // check validity - should return true
  EXPECT_FALSE(verletLists.checkNeighborListsAreValid());
}

TEST_F(VerletListsTest, testCheckNeighborListsInvalidMoveFarOutsideCell) {
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists({0., 0., 0.}, {10., 10., 10.}, 2., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;

  // addtwo particles in proper distance
  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p);
  autopas::Particle p2({7.5, 1.1, 1.1}, {0., 0., 0.}, 2);
  verletLists.addParticle(p2);

  // this will build the verlet list
  verletLists.iteratePairwiseAoS2(&emptyFunctor);

  for (auto iter = verletLists.begin(); iter.isValid(); ++iter) {
    if (iter->getID() == 1) {
      // this sets the particle more than skin/2 outside of cell (xmax_cell=2.3)
      iter->setR({2.7, 1.1, 1.1});
    }
  }

  // check validity - should return true
  EXPECT_FALSE(verletLists.checkNeighborListsAreValid());
}

TEST_F(VerletListsTest, testCheckNeighborListsValidMoveLittleOutsideCell) {
  autopas::VerletLists<autopas::Particle,
                       autopas::FullParticleCell<autopas::Particle>>
      verletLists({0., 0., 0.}, {10., 10., 10.}, 2., 0.3, 3);

  autopas::Functor<autopas::Particle,
                   autopas::FullParticleCell<autopas::Particle>>
      emptyFunctor;

  // addtwo particles in proper distance
  autopas::Particle p({1.1, 1.1, 1.1}, {0., 0., 0.}, 1);
  verletLists.addParticle(p);
  autopas::Particle p2({7.5, 1.1, 1.1}, {0., 0., 0.}, 2);
  verletLists.addParticle(p2);

  // this will build the verlet list
  verletLists.iteratePairwiseAoS2(&emptyFunctor);

  for (auto iter = verletLists.begin(); iter.isValid(); ++iter) {
    if (iter->getID() == 1) {
      // this sets the particle less than skin/2 outside of cell (xmax_cell=2.3)
      iter->setR({2.4, 1.1, 1.1});
    }
  }

  // check validity - should return true
  EXPECT_TRUE(verletLists.checkNeighborListsAreValid());
}