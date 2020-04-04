#include "hash_table.h"
#include <gtest/gtest.h>

TEST(HashTable, API) {
  HashTable<std::string, std::string> ht(1);

  std::string value;
  ASSERT_FALSE(ht.Lookup("key", value));
  ASSERT_TRUE(ht.Insert("key", "value"));
  ASSERT_TRUE(ht.Lookup("key", value));
  ASSERT_EQ(value, "value");
  ASSERT_TRUE(ht.Remove("key"));
  ASSERT_FALSE(ht.Lookup("key", value));
}

TEST(HashTable, RemoveAbsent) {
  HashTable<std::string, std::string> ht(3);
  ASSERT_FALSE(ht.Remove("NotFound"));
}

TEST(HashTable, Overwrite) {
  HashTable<std::string, std::string> ht(7);

  ASSERT_TRUE(ht.Insert("key", "old"));
  ASSERT_TRUE(ht.Remove("key"));
  ASSERT_TRUE(ht.Insert("key", "new"));

  std::string value;
  ASSERT_TRUE(ht.Lookup("key", value));
  ASSERT_EQ(value, "new");
}

TEST(HashTable, RepeatedUpdates) {
  HashTable<int, std::string> ht(17);
  ASSERT_TRUE(ht.Insert(1, "One"));
  ASSERT_FALSE(ht.Insert(1, "Two"));

  std::string value;
  ASSERT_TRUE(ht.Lookup(1, value));
  ASSERT_EQ(value, "One");

  ASSERT_TRUE(ht.Remove(1));
  ASSERT_FALSE(ht.Remove(1));

  ASSERT_FALSE(ht.Lookup(1, value));
}

TEST(HashTable, MultipleKeys) {
  HashTable<std::string, int> ht(3);

  const int kRange = 17;

  for (int i = 0; i < kRange; i += 2) {
    ASSERT_TRUE(ht.Insert(std::to_string(i), i));
  }

  for (int i = 0; i < kRange; ++i) {
    int value;
    if (i % 2 == 0) {
      ASSERT_TRUE(ht.Lookup(std::to_string(i), value));
      ASSERT_EQ(value, i);
      ASSERT_TRUE(ht.Remove(std::to_string(i)));
    } else {
      ASSERT_FALSE(ht.Lookup(std::to_string(i), value));
    }
  }

  for (int i = 0; i < kRange; ++i) {
    int value;
    ASSERT_FALSE(ht.Lookup(std::to_string(i), value));
  }
}
