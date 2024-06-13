/**
 * Tests that the batchSize field is sent to mongot correctly on GetMore requests.
 * @tags: [featureFlagSearchBatchSizeTuning]
 */
import {FeatureFlagUtil} from "jstests/libs/feature_flag_util.js";
import {checkSbeRestrictedOrFullyEnabled} from "jstests/libs/sbe_util.js";
import {getUUIDFromListCollections} from "jstests/libs/uuid_util.js";
import {
    mockAllRequestsWithBatchSizes,
    mongotCommandForQuery,
    MongotMock,
    mongotResponseForBatch
} from "jstests/with_mongot/mongotmock/lib/mongotmock.js";

const dbName = "test";
const collName = jsTestName();

// Start mock mongot.
const mongotMock = new MongotMock();
mongotMock.start();
const mockConn = mongotMock.getConnection();

// Start mongod.
const conn = MongoRunner.runMongod({setParameter: {mongotHost: mockConn.host}});
let db = conn.getDB(dbName);
let coll = db.getCollection(collName);
coll.drop();

if (checkSbeRestrictedOrFullyEnabled(db) &&
    FeatureFlagUtil.isPresentAndEnabled(db.getMongo(), 'SearchInSbe')) {
    jsTestLog("Skipping the test because it only applies to $search in classic engine.");
    MongoRunner.stopMongod(conn);
    mongotMock.stop();
    quit();
}

const mongotQuery = {
    query: "foo",
    path: "bar",
    returnStoredSource: true
};
const numDocs = 10000;
let docs = [];
let mongotDocs = [];
let searchScore = 0.60000;
for (let i = 0; i < numDocs; i++) {
    docs.push({_id: i, a: i % 1000, bar: "fooey"});
    mongotDocs.push({_id: i, $searchScore: searchScore, a: i % 1000, bar: "fooey"});

    // The documents with lower _id will have a higher search score.
    searchScore = searchScore - 0.00005;
}

assert.commandWorked(coll.insertMany(docs));
const collUUID = getUUIDFromListCollections(db, coll.getName());

// The batchSizeGrowthFactor is customizable as a cluster parameter. We'll assert that it's
// properly configurable and that the new growth factors are applied correctly.
function assertGrowthFactorSetAsExpected(expectedGrowthFactor) {
    assert.eq(expectedGrowthFactor,
              assert.commandWorked(db.adminCommand({getClusterParameter: "internalSearchOptions"}))
                  .clusterParameters[0]
                  .batchSizeGrowthFactor);
}

// Tests a pipeline that will exhaust all mongot results because of a blocking $group stage.
function testSearchGroupPipeline() {
    const res = coll.aggregate([{$search: mongotQuery}, {$group: {_id: "$bar", avg: {$avg: "$a"}}}])
                    .toArray();
    assert.eq(res.length, 1);
    assert.eq(res[0], {_id: "fooey", avg: 499.5});
}

// Tests a pipeline that will exhaust many but not all mongot results (at least up to _id=4000) due
// to a $limit preceded by a highly selective $match.
function testSearchMatchSmallLimitPipeline() {
    const res = coll.aggregate([{$search: mongotQuery}, {$match: {a: 0}}, {$limit: 5}]).toArray();
    assert.eq(res.length, 5);
    assert.eq(res, [
        {_id: 0, a: 0, bar: "fooey"},
        {_id: 1000, a: 0, bar: "fooey"},
        {_id: 2000, a: 0, bar: "fooey"},
        {_id: 3000, a: 0, bar: "fooey"},
        {_id: 4000, a: 0, bar: "fooey"}
    ]);
}

// Tests a pipeline that will exhaust all mongot results since the $match is so selective that the
// higher $limit will not be reached.
function testSearchMatchLargeLimitPipeline() {
    const res = coll.aggregate([
                        {$search: mongotQuery},
                        {$match: {a: 0}},
                        {$limit: 250},
                        {$project: {_id: 1, bar: 1}}
                    ])
                    .toArray();
    assert.eq(res.length, 10);
    assert.eq(res, [
        {_id: 0, bar: "fooey"},
        {_id: 1000, bar: "fooey"},
        {_id: 2000, bar: "fooey"},
        {_id: 3000, bar: "fooey"},
        {_id: 4000, bar: "fooey"},
        {_id: 5000, bar: "fooey"},
        {_id: 6000, bar: "fooey"},
        {_id: 7000, bar: "fooey"},
        {_id: 8000, bar: "fooey"},
        {_id: 9000, bar: "fooey"}
    ]);
}

function mockRequests(expectedBatchSizes) {
    mockAllRequestsWithBatchSizes({
        query: mongotQuery,
        collName,
        dbName,
        collectionUUID: collUUID,
        documents: mongotDocs,
        expectedBatchSizes,
        cursorId: NumberLong(99),
        mongotMockConn: mongotMock
    });
}

// Test first that the pipelines calculate each batchSize using the default growth factor of 2.
{
    // Assert the batchSizeGrowthFactor is set to default value of 2 at startup.
    assertGrowthFactorSetAsExpected(2.000);

    mockRequests([101, 202, 404, 808, 1616, 3232, 6464]);
    testSearchGroupPipeline();

    // batchSize starts at 101 since {$limit: 5} is less than the default batchSize of 101 and the
    // presence of the $match means the limit is not cleanly extractable. This query doesn't exhaust
    // all results since the limit will be satisfied once document with _id=4000 is retrieved.
    mockRequests([101, 202, 404, 808, 1616, 3232]);
    testSearchMatchSmallLimitPipeline();

    // batchSize starts at 250 due to {$limit: 250}.
    mockRequests([250, 500, 1000, 2000, 4000, 8000]);
    testSearchMatchLargeLimitPipeline();
}

// Confirm that the batchSizeGrowthFactor can be configured to 1.5 and that the same pipelines will
// calculate each batchSize using that growth factor.
{
    assert.commandWorked(db.adminCommand(
        {setClusterParameter: {internalSearchOptions: {batchSizeGrowthFactor: 1.5}}}));
    assertGrowthFactorSetAsExpected(1.5);

    mockRequests([101, 152, 228, 342, 513, 770, 1155, 1733, 2600, 3900]);
    testSearchGroupPipeline();

    mockRequests([101, 152, 228, 342, 513, 770, 1155, 1733]);
    testSearchMatchSmallLimitPipeline();

    mockRequests([250, 375, 563, 845, 1268, 1902, 2853, 4280]);
    testSearchMatchLargeLimitPipeline();
}

// Confirm that the batchSizeGrowthFactor can be configured to 2.8 and that the same pipelines will
// calculate each batchSize using that growth factor.
{
    assert.commandWorked(db.adminCommand(
        {setClusterParameter: {internalSearchOptions: {batchSizeGrowthFactor: 2.8}}}));
    assertGrowthFactorSetAsExpected(2.8);

    mockRequests([101, 283, 793, 2221, 6219, 17414]);
    testSearchGroupPipeline();

    mockRequests([101, 283, 793, 2221, 6219]);
    testSearchMatchSmallLimitPipeline();

    mockRequests([250, 700, 1960, 5488, 15367]);
    testSearchMatchLargeLimitPipeline();
}

// Confirm that the batchSizeGrowthFactor can be configured to 1 and that the same pipelines will
// calculate each batchSize using that growth factor.
{
    assert.commandWorked(db.adminCommand(
        {setClusterParameter: {internalSearchOptions: {batchSizeGrowthFactor: 1}}}));
    assertGrowthFactorSetAsExpected(1.00);

    // We need 100 batches with size 101 to exhaust all mongot results.
    mockRequests(Array(100).fill(101));
    testSearchGroupPipeline();

    mockRequests(Array(40).fill(101));
    testSearchMatchSmallLimitPipeline();

    mockRequests(Array(41).fill(250));
    testSearchMatchLargeLimitPipeline();
}

MongoRunner.stopMongod(conn);
mongotMock.stop();
