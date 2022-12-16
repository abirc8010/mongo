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

#include "mongo/db/cluster_role.h"
#include "mongo/db/catalog_shard_feature_flag_gen.h"
#include "mongo/db/feature_flag.h"

namespace mongo {

bool ClusterRole::operator==(const ClusterRole& other) const {
    if (gFeatureFlagCatalogShard.isEnabledAndIgnoreFCV() && _value == ClusterRole::ConfigServer) {
        return other._value == ClusterRole::ConfigServer ||
            other._value == ClusterRole::ShardServer;
    }

    return _value == other._value;
}

bool ClusterRole::isShardRole() {
    return _value == ClusterRole::ShardServer ||
        (gFeatureFlagCatalogShard.isEnabledAndIgnoreFCV() && _value == ClusterRole::ConfigServer);
}

bool ClusterRole::isExclusivelyShardRole() {
    return _value == ClusterRole::ShardServer;
}

bool ClusterRole::isExclusivelyConfigSvrRole() {
    return _value == ClusterRole::ConfigServer && !gFeatureFlagCatalogShard.isEnabledAndIgnoreFCV();
}
}  // namespace mongo
