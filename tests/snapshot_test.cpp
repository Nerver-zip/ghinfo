#include "ghinfo/snapshot.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace {

TEST(SnapshotStoreTest, StartsNotReady) {
    ghinfo::SnapshotStore store;

    EXPECT_FALSE(store.ready());
    EXPECT_EQ(store.get(), nullptr);
}

TEST(SnapshotStoreTest, PublishesImmutableSnapshot) {
    ghinfo::SnapshotStore store;

    auto snapshot = std::make_shared<ghinfo::Snapshot>();
    snapshot->generation = 7;
    store.publish(snapshot);

    ASSERT_TRUE(store.ready());
    const auto published = store.get();
    ASSERT_NE(published, nullptr);
    EXPECT_EQ(published->generation, 7U);
}

TEST(SnapshotStoreTest, IgnoresNullPublish) {
    ghinfo::SnapshotStore store;
    store.publish(nullptr);

    EXPECT_FALSE(store.ready());
}

} // namespace
