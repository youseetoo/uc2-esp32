// Host-side test of the stage-scan frame order (no Arduino, no PlatformIO).
// Build & run:  test/native/run.sh
// Mirrors ImSwitch imswitch/imcontrol/_test/unit/test_stage_scan_frame_table.py:
// the expected sequences below are the same ones that test asserts.
#include "../../main/src/motor/StageScanOrder.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Frame { int ix, iy, iz, ch; bool operator==(const Frame &o) const { return ix==o.ix && iy==o.iy && iz==o.iz && ch==o.ch; } };

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static std::vector<Frame> collect(uint16_t nX, uint16_t nY, uint16_t nZ, bool zicZac, const int *lasers, int led)
{
    std::vector<Frame> v;
    StageScanOrder::forEachFrame(nX, nY, nZ, zicZac, lasers, led,
        [&](uint16_t ix, uint16_t iy, uint16_t iz, int ch) { v.push_back({ix, iy, iz, ch}); return true; });
    return v;
}

static void test_grid_order_rows_snake_z_channels()
{
    const int lasers[5] = {0, 50, 0, 0, 100};
    std::vector<Frame> expect = {
        {0,0,0,1},{0,0,0,4},{0,0,1,1},{0,0,1,4},
        {1,0,0,1},{1,0,0,4},{1,0,1,1},{1,0,1,4},
        {1,1,0,1},{1,1,0,4},{1,1,1,1},{1,1,1,4},   // odd row: X reversed
        {0,1,0,1},{0,1,0,4},{0,1,1,1},{0,1,1,4},
    };
    CHECK(collect(2, 2, 2, true, lasers, 0) == expect);
    CHECK(StageScanOrder::frameCount(2, 2, 2, lasers, 0) == 16);
}

static void test_snake_only_on_odd_rows_and_raster()
{
    const int none[5] = {0, 0, 0, 0, 0};
    std::vector<int> snake, raster;
    for (auto &f : collect(3, 3, 1, true, none, 0)) snake.push_back(f.ix);
    for (auto &f : collect(3, 3, 1, false, none, 0)) raster.push_back(f.ix);
    CHECK((snake == std::vector<int>{0,1,2, 2,1,0, 0,1,2}));
    CHECK((raster == std::vector<int>{0,1,2, 0,1,2, 0,1,2}));
}

static void test_no_light_fires_once_per_position()
{
    const int none[5] = {0, 0, 0, 0, 0};
    auto v = collect(2, 1, 3, true, none, 0);
    CHECK(v.size() == 6);
    for (auto &f : v) CHECK(f.ch == StageScanOrder::kChannelNone);
    CHECK(StageScanOrder::frameCount(2, 1, 3, none, 0) == 6);
}

static void test_led_after_lasers()
{
    const int lasers[5] = {30, 0, 0, 0, 0};
    int seq[6];
    CHECK(StageScanOrder::channelSequence(lasers, 255, seq) == 2);
    CHECK(seq[0] == 0 && seq[1] == StageScanOrder::kChannelLed);
    CHECK(StageScanOrder::frameCount(1, 1, 1, lasers, 255) == 2);
}

static void test_visitor_can_stop()
{
    const int lasers[5] = {1, 1, 1, 1, 1};
    int seen = 0;
    bool finished = StageScanOrder::forEachFrame(4, 4, 4, true, lasers, 1,
        [&](uint16_t, uint16_t, uint16_t, int) { return ++seen < 7; });
    CHECK(!finished && seen == 7);
}

int main()
{
    test_grid_order_rows_snake_z_channels();
    test_snake_only_on_odd_rows_and_raster();
    test_no_light_fires_once_per_position();
    test_led_after_lasers();
    test_visitor_can_stop();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("stage scan order: all checks passed\n");
    return 0;
}
