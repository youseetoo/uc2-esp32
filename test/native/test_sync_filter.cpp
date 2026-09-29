// Host test of SYNC detection, TPDO3 position decoding and the callback list.
#include "../../main/src/canopen/SyncFilter.h"
#include "test_harness.h"

static void test_sync_detection()
{
    CHECK(SyncFilter::isSync(0x080, false, false, 0, 0x080));
    CHECK(SyncFilter::isSync(0x080, false, false, 0, 0x40000080 & 0xFFFF)); // high bits of 0x1005 ignored
    CHECK(!SyncFilter::isSync(0x080, false, false, 1, 0x080));  // counter variant is not ours
    CHECK(!SyncFilter::isSync(0x080, true, false, 0, 0x080));   // extended frame
    CHECK(!SyncFilter::isSync(0x080, false, true, 0, 0x080));   // remote frame
    CHECK(!SyncFilter::isSync(0x081, false, false, 0, 0x080));  // EMCY of node 1
    CHECK(SyncFilter::isSync(0x090, false, false, 0, 0x090));   // configurable COB-ID
}

static void test_position_roundtrip()
{
    uint8_t d[8] = {0};
    const int32_t positions[] = {0, 1, -1, 123456, -987654, 2147483647, (-2147483647 - 1)};
    for (int32_t p : positions)
    {
        SyncFilter::encodePosition(p, 513, d);
        uint8_t node = 0;
        int32_t pos = 0;
        uint16_t count = 0;
        CHECK(SyncFilter::decodePosition(0x380 + 11, false, false, 6, d, node, pos, count));
        CHECK(node == 11 && pos == p && count == 513);
    }
    // Little endian, like the CANopen mapping of 0x200C/0x200D.
    SyncFilter::encodePosition(0x01020304, 0x0506, d);
    CHECK(d[0] == 0x04 && d[1] == 0x03 && d[2] == 0x02 && d[3] == 0x01 && d[4] == 0x06 && d[5] == 0x05);
}

static void test_position_rejects()
{
    uint8_t d[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t node;
    int32_t pos;
    uint16_t count;
    CHECK(!SyncFilter::decodePosition(0x180 + 11, false, false, 6, d, node, pos, count)); // TPDO1
    CHECK(!SyncFilter::decodePosition(0x480 + 11, false, false, 6, d, node, pos, count)); // TPDO4
    CHECK(!SyncFilter::decodePosition(0x380, false, false, 6, d, node, pos, count));      // node 0
    CHECK(!SyncFilter::decodePosition(0x38B, false, false, 5, d, node, pos, count));      // too short
    CHECK(!SyncFilter::decodePosition(0x38B, true, false, 6, d, node, pos, count));
    CHECK(!SyncFilter::decodePosition(0x38B, false, true, 6, d, node, pos, count));
    CHECK(SyncFilter::decodePosition(0x38B, false, false, 8, d, node, pos, count));       // extra bytes ok
}

static int calls = 0;
static void cbA(int64_t, void *) { ++calls; }
static void cbB(int64_t, void *) { calls += 10; }

static void test_callback_list()
{
    typedef void (*Fn)(int64_t, void *);
    SyncFilter::CallbackList<Fn, 2> list;
    int ctx1 = 0, ctx2 = 0;
    CHECK(list.add(cbA, &ctx1));
    CHECK(list.add(cbA, &ctx1)); // idempotent
    CHECK(list.size() == 1);
    CHECK(list.add(cbB, &ctx2));
    CHECK(!list.add(cbA, &ctx2)); // full
    for (int i = 0; i < 2; ++i)
        if (list.fn[i])
            list.fn[i](0, list.ctx[i]);
    CHECK(calls == 11);
    list.remove(cbA, &ctx1);
    CHECK(list.size() == 1);
    CHECK(list.add(cbA, &ctx2)); // slot reused
}

int main()
{
    test_sync_detection();
    test_position_roundtrip();
    test_position_rejects();
    test_callback_list();
    FINISH("sync filter");
}
