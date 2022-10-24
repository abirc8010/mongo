/**
 *    Copyright (C) 2022-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/s/global_index_cache.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
namespace {

TEST(ComparableIndexVersionTest, VersionsEqual) {
    const auto uuid = UUID::gen();
    const CollectionIndexes v1(uuid, Timestamp(1, 0));
    const CollectionIndexes v2(uuid, Timestamp(1, 0));
    const auto version1 = ComparableIndexVersion::makeComparableIndexVersion(v1);
    const auto version2 = ComparableIndexVersion::makeComparableIndexVersion(v2);
    ASSERT(version1 == version2);
}

TEST(ComparableIndexVersionTest, VersionsEqualAfterCopy) {
    const CollectionIndexes indexVersion(UUID::gen(), Timestamp(1, 0));
    const auto version1 = ComparableIndexVersion::makeComparableIndexVersion(indexVersion);
    const auto version2 = version1;
    ASSERT(version1 == version2);
}

TEST(ComparableIndexVersionTest, CompareDifferentGenerations) {
    const CollectionIndexes v1(UUID::gen(), Timestamp(2, 0));
    const CollectionIndexes v2(UUID::gen(), Timestamp(1, 0));
    const auto version1 = ComparableIndexVersion::makeComparableIndexVersion(v1);
    const auto version2 = ComparableIndexVersion::makeComparableIndexVersion(v2);
    ASSERT(version2 != version1);
    ASSERT(version2 > version1);
    ASSERT_FALSE(version2 < version1);
}

TEST(ComparableIndexVersionTest, VersionGreaterSameUUID) {
    const auto uuid = UUID::gen();
    const CollectionIndexes v1(uuid, Timestamp(1, 0));
    const CollectionIndexes v2(uuid, Timestamp(1, 2));
    const CollectionIndexes v3(uuid, Timestamp(2, 0));
    const auto version1 = ComparableIndexVersion::makeComparableIndexVersion(v1);
    const auto version2 = ComparableIndexVersion::makeComparableIndexVersion(v2);
    const auto version3 = ComparableIndexVersion::makeComparableIndexVersion(v3);
    ASSERT(version2 != version1);
    ASSERT(version2 > version1);
    ASSERT_FALSE(version2 < version1);
    ASSERT(version3 != version2);
    ASSERT(version3 > version2);
    ASSERT_FALSE(version3 < version2);
}

TEST(ComparableIndexVersionTest, VersionLessSameUUID) {
    const auto uuid = UUID::gen();
    const CollectionIndexes v1(uuid, Timestamp(1, 0));
    const CollectionIndexes v2(uuid, Timestamp(1, 2));
    const CollectionIndexes v3(uuid, Timestamp(2, 0));
    const auto version1 = ComparableIndexVersion::makeComparableIndexVersion(v1);
    const auto version2 = ComparableIndexVersion::makeComparableIndexVersion(v2);
    const auto version3 = ComparableIndexVersion::makeComparableIndexVersion(v3);
    ASSERT(version1 != version2);
    ASSERT(version1 < version2);
    ASSERT_FALSE(version1 > version2);
    ASSERT(version3 != version2);
    ASSERT(version2 < version3);
    ASSERT_FALSE(version2 > version3);
}

TEST(ComparableIndexVersionTest, DefaultConstructedVersionsAreEqual) {
    const ComparableIndexVersion defaultVersion1{}, defaultVersion2{};
    ASSERT(defaultVersion1 == defaultVersion2);
    ASSERT_FALSE(defaultVersion1 < defaultVersion2);
    ASSERT_FALSE(defaultVersion1 > defaultVersion2);
}

TEST(ComparableIndexVersionTest, DefaultConstructedVersionIsAlwaysLessThanWithIndexesVersion) {
    const CollectionIndexes indexVersion(UUID::gen(), Timestamp(1, 0));
    const ComparableIndexVersion defaultVersion{};
    const auto withIndexesVersion =
        ComparableIndexVersion::makeComparableIndexVersion(indexVersion);
    ASSERT(defaultVersion != withIndexesVersion);
    ASSERT(defaultVersion < withIndexesVersion);
    ASSERT_FALSE(defaultVersion > withIndexesVersion);
}

TEST(ComparableIndexVersionTest, DefaultConstructedVersionIsAlwaysLessThanNoIndexesVersion) {
    const ComparableIndexVersion defaultVersion{};
    const auto noIndexesVersion = ComparableIndexVersion::makeComparableIndexVersion(boost::none);
    ASSERT(defaultVersion != noIndexesVersion);
    ASSERT(defaultVersion < noIndexesVersion);
    ASSERT_FALSE(defaultVersion > noIndexesVersion);
}

// TODO (SERVER-70195) Re-enable this test
// TEST(ComparableIndexVersionTest, TwoNoIndexesVersionsAreTheSame) {
//     const auto noIndexesVersion1 =
//     ComparableIndexVersion::makeComparableIndexVersion(boost::none); const auto noIndexesVersion2
//     = ComparableIndexVersion::makeComparableIndexVersion(boost::none); ASSERT(noIndexesVersion1
//     == noIndexesVersion2); ASSERT_FALSE(noIndexesVersion1 < noIndexesVersion2);
//     ASSERT_FALSE(noIndexesVersion1 > noIndexesVersion2);
// }

TEST(ComparableIndexVersionTest, NoIndexesGreaterThanDefault) {
    const auto noIndexesVersion = ComparableIndexVersion::makeComparableIndexVersion(boost::none);
    const ComparableIndexVersion defaultVersion{};
    ASSERT(noIndexesVersion != defaultVersion);
    ASSERT(noIndexesVersion > defaultVersion);
}

TEST(ComparableIndexVersionTest, CompareForcedRefreshVersionVersusValidCollectionIndexes) {
    const CollectionIndexes indexVersion(UUID::gen(), Timestamp(100, 0));
    const ComparableIndexVersion defaultVersionBeforeForce;
    const auto versionBeforeForce =
        ComparableIndexVersion::makeComparableIndexVersion(indexVersion);
    const auto forcedRefreshVersion =
        ComparableIndexVersion::makeComparableIndexVersionForForcedRefresh();
    const auto versionAfterForce = ComparableIndexVersion::makeComparableIndexVersion(indexVersion);
    const ComparableIndexVersion defaultVersionAfterForce;

    ASSERT(defaultVersionBeforeForce != forcedRefreshVersion);
    ASSERT(defaultVersionBeforeForce < forcedRefreshVersion);

    ASSERT(versionBeforeForce != forcedRefreshVersion);
    ASSERT(versionBeforeForce < forcedRefreshVersion);

    ASSERT(versionAfterForce != forcedRefreshVersion);
    ASSERT(versionAfterForce > forcedRefreshVersion);

    ASSERT(defaultVersionAfterForce != forcedRefreshVersion);
    ASSERT(defaultVersionAfterForce < forcedRefreshVersion);
}

TEST(ComparableIndexVersionTest, CompareTwoForcedRefreshVersions) {
    const auto forcedRefreshVersion1 =
        ComparableIndexVersion::makeComparableIndexVersionForForcedRefresh();
    ASSERT(forcedRefreshVersion1 == forcedRefreshVersion1);
    ASSERT_FALSE(forcedRefreshVersion1 < forcedRefreshVersion1);
    ASSERT_FALSE(forcedRefreshVersion1 > forcedRefreshVersion1);

    const auto forcedRefreshVersion2 =
        ComparableIndexVersion::makeComparableIndexVersionForForcedRefresh();
    ASSERT_FALSE(forcedRefreshVersion1 == forcedRefreshVersion2);
    ASSERT(forcedRefreshVersion1 < forcedRefreshVersion2);
    ASSERT_FALSE(forcedRefreshVersion1 > forcedRefreshVersion2);
}

}  // namespace
}  // namespace mongo
