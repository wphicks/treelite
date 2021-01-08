#include <gtest/gtest.h>
#include <treelite/fastmap.h>

using namespace treelite::containers;

TEST(FastMapTest, DefaultConstructor) {
  FastMap<int, int> map{};
  ASSERT_EQ(map.size(), 0);
  ASSERT_EQ(map.size_hint(), 2048);
}

TEST(FastMapTest, SizedConstructor) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  ASSERT_EQ(map.size_hint(), 5);
}

TEST(FastMapTest, IndexerInsertion) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  map[3];
  ASSERT_EQ(map.size(), 1);
  map[8];
  ASSERT_EQ(map.size(), 2);
}

TEST(FastMapTest, IndexerValueInsertion) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  // Insert values
  for (int i = 0; i < 6; ++i) {
    map[5 - i] = i;
    ASSERT_EQ(map.size(), i + 1);
  }
  for (int i = 0; i < 6; ++i) {
    ASSERT_EQ(map[5 - i], i);
  }
  // Update values
  for (int i = 0; i < 6; ++i) {
    map[i] = i * 2;
  }
  ASSERT_EQ(map.size(), 6);
  for (int i = 0; i < 6; ++i) {
    ASSERT_EQ(map[i], i * 2);
  }
}

TEST(FastMapTest, at) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  ASSERT_THROW(map.at(2), std::out_of_range);
  ASSERT_EQ(map.size(), 0);
  map[2] = 3;
  ASSERT_EQ(map.at(2), 3);
}

TEST(FastMapTest, clear) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  for (int i = 0; i < 6; ++i) {
    map[i] = i;
  }
  ASSERT_EQ(map.size(), 6);
  ASSERT_EQ(map.at(2), 2);

  map.clear();
  ASSERT_THROW(map.at(2), std::out_of_range);
  ASSERT_EQ(map.size(), 0);
}

TEST(FastMapTest, erase) {
  FastMap<int, int> map{5};
  ASSERT_EQ(map.size(), 0);
  for (int i = 0; i < 6; ++i) {
    map[i] = i;
  }
  ASSERT_EQ(map.size(), 6);
  ASSERT_EQ(map.at(3), 3);

  map.erase(3);
  ASSERT_EQ(map.size(), 5);
  for (int i = 0; i < 6; ++i) {
    if (i == 3) {
      ASSERT_THROW(map.at(3), std::out_of_range);
    } else {
      ASSERT_EQ(map.at(i), i);
    }
  }
}

TEST(FastMapTest, Iterators) {
  FastMap<int, int> map{5};
  size_t count = 0;
  for (auto iter = map.begin(); iter != map.end(); ++iter) {
    ++count;
  }
  ASSERT_EQ(map.size(), 0);
  ASSERT_EQ(count, 0);

  count = 0;
  for (auto iter = map.cbegin(); iter != map.cend(); ++iter) {
    ++count;
  }
  ASSERT_EQ(map.size(), 0);
  ASSERT_EQ(count, 0);

  for (int i = 0; i < 6; ++i) {
    map[i] = i;
  }

  count = 0;
  for (auto iter = map.cbegin(); iter != map.cend(); ++iter) {
    ASSERT_EQ(iter->second, count);
    ++count;
  }

  count = 0;
  for (auto iter = map.begin(); iter != map.end(); ++iter) {
    ASSERT_EQ(iter->second, count);
    iter->second = 7;
    ASSERT_EQ(map[count], 7);
    ++count;
  }
}
