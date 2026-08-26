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

TEST(SnapshotStoreTest, TracksPollStateWithoutMutatingSnapshot) {
    ghinfo::SnapshotStore store;
    auto snapshot = std::make_shared<ghinfo::Snapshot>();
    snapshot->generation = 4;
    store.publish(snapshot);

    store.record_poll_attempt("2026-08-26T18:00:00Z");
    store.record_poll_failure("2026-08-26T18:00:00Z", "http", "2026-08-26T18:00:05Z");

    const auto failed = store.poll_state();
    EXPECT_TRUE(failed.last_attempt.has_value());
    EXPECT_EQ(failed.last_attempt.value(), "2026-08-26T18:00:00Z");
    EXPECT_TRUE(failed.stale);
    EXPECT_EQ(failed.consecutive_failures, 1U);
    ASSERT_TRUE(failed.last_error_kind.has_value());
    EXPECT_EQ(failed.last_error_kind.value(), "http");
    ASSERT_TRUE(failed.next_retry_at.has_value());
    EXPECT_EQ(failed.next_retry_at.value(), "2026-08-26T18:00:05Z");
    EXPECT_EQ(store.get()->generation, 4U);

    store.record_poll_success("2026-08-26T18:00:10Z");
    const auto recovered = store.poll_state();
    EXPECT_FALSE(recovered.stale);
    EXPECT_EQ(recovered.consecutive_failures, 0U);
    EXPECT_FALSE(recovered.last_error_kind.has_value());
    EXPECT_FALSE(recovered.next_retry_at.has_value());
    ASSERT_TRUE(recovered.last_successful.has_value());
    EXPECT_EQ(recovered.last_successful.value(), "2026-08-26T18:00:10Z");
}

} // namespace
