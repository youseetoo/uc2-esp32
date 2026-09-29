// Host test of the strobed-sweep decision logic and report format.
#include "../../main/src/motor/StrobeSweepLogic.h"
#include "test_harness.h"
#include <cstring>

using namespace StrobeSweepLogic;

static void test_validation()
{
    Params p;
    CHECK(validate(p) == nullptr);
    Params a = p; a.axis = 4;            CHECK(validate(a) != nullptr);
    Params b = p; b.laser = 10;          CHECK(validate(b) != nullptr);
    Params c = p; c.periodUs = 1000;     CHECK(validate(c) != nullptr);  // above 500 fps
    Params d = p; d.periodUs = 2000; d.trigUs = 1800; CHECK(validate(d) != nullptr);
    Params e = p; e.laser = 4; e.delayUs = 1200;      CHECK(validate(e) != nullptr); // width missing
    Params f = p; f.delayUs = 1200; f.widthUs = 20;   CHECK(validate(f) != nullptr); // no laser
    Params g = p; g.laser = 4; g.delayUs = 1200; g.widthUs = 20; CHECK(validate(g) == nullptr);
    Params h = p; h.speed = 0;           CHECK(validate(h) != nullptr);
    Params i = p; i.speed = 0; i.maxFrames = 10; CHECK(validate(i) == nullptr); // calibration, no motion
    Params j = p; j.report = 0;          CHECK(validate(j) == nullptr && j.report == 1);
    Params k = p; k.report = 500;        CHECK(validate(k) == nullptr && k.report == kMaxReport);
    Params m = p; m.trigUs = 0;          CHECK(validate(m) != nullptr);
}

static void test_can_decision()
{
    // HAT topology: LED and motor on CAN
    CHECK(needsCanSync(true, true, true, true));
    // standalone board: everything local
    CHECK(!needsCanSync(true, false, true, false));
    // no flash, remote motor, latch -> SYNC needed for the position
    CHECK(needsCanSync(false, false, true, true));
    // no flash, remote motor, no latch -> nothing on CAN
    CHECK(!needsCanSync(false, false, false, true));
    // local flash, remote motor with latch -> CAN (mixed)
    CHECK(needsCanSync(true, false, true, true));
}

static void test_exit_rules()
{
    CHECK(exitReason(false, 0, 5, false, false) == Exit::Continue);
    CHECK(exitReason(true, 0, 5, true, true) == Exit::Stopped);         // stop wins
    CHECK(exitReason(false, 0, 5, true, false) == Exit::Arrived);
    CHECK(exitReason(false, 10, 5, true, false) == Exit::Continue);     // maxFrames ignores arrival
    CHECK(exitReason(false, 10, 10, false, false) == Exit::FramesDone);
    CHECK(exitReason(false, 0, 5, false, true) == Exit::Timeout);
}

static void test_ticks_and_timeout()
{
    CHECK(nextTick(0, 0, 33333) == 33333);
    CHECK(nextTick(33333, 34000, 33333) == 66666);
    CHECK(nextTick(33333, 100000, 33333) == 133333);  // fell behind: no burst
    CHECK(timeoutUs(10, 33333, 0, 0) == 10LL * 33333 + 5000000);
    CHECK(timeoutUs(0, 33333, -200000, 20000) == 30000000LL + 10000000);  // 10 s travel x3 + 10 s
}

static void test_report_format()
{
    ReportBatch b;
    CHECK(!b.push(1, 1000, 3));
    CHECK(!b.push(2, -1500, 3));
    CHECK(b.push(4, 2147483647, 3));  // n=3 lost on the bus: gap stays visible
    char buf[256];
    const int n = b.format(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(std::strcmp(buf, "{\"strobesweep\":{\"n\":[1,2,4],\"x\":[1000,-1500,2147483647]}}") == 0);
    char tiny[20];
    CHECK(b.format(tiny, sizeof(tiny)) == -1);
    b.clear();
    CHECK(b.format(buf, sizeof(buf)) > 0 && std::strcmp(buf, "{\"strobesweep\":{\"n\":[],\"x\":[]}}") == 0);
    // a full batch of the largest values fits the firmware's 1400-byte buffer
    ReportBatch big;
    for (int i = 0; i < kMaxReport; ++i)
        big.push(65535, (-2147483647 - 1), kMaxReport);
    static char out[1400];
    CHECK(big.format(out, sizeof(out)) > 0);
}

static void test_success()
{
    CHECK(success(false, false, 10));
    CHECK(!success(true, false, 10));
    CHECK(!success(false, true, 10));
    CHECK(!success(false, false, 0));
}

int main()
{
    test_validation();
    test_can_decision();
    test_exit_rules();
    test_ticks_and_timeout();
    test_report_format();
    test_success();
    FINISH("strobe sweep logic");
}
