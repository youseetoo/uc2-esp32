// Host test of strobe clamping, duty limiting and the enable state machine.
#include "../../main/src/laser/StrobeTiming.h"
#include "test_harness.h"

using namespace StrobeTiming;
static const Limits L = {1000, 100000, 50};

static void test_clamps()
{
    CHECK(clampWidth(0, L) == 1);
    CHECK(clampWidth(20, L) == 20);
    CHECK(clampWidth(5000, L) == 1000);
    CHECK(clampDelay(1200, L) == 1200);
    CHECK(clampDelay(500000, L) == 100000);
}

static void test_min_spacing()
{
    CHECK(minSpacingUs(20, 50) == 400);   // 20 us at 5 % -> 400 us
    CHECK(minSpacingUs(21, 50) == 420);
    CHECK(minSpacingUs(1, 3) == 334);     // rounded up, never above the limit
    CHECK(minSpacingUs(20, 1000) == 20);  // 100 %: back-to-back allowed
    CHECK(minSpacingUs(20, 0) == 20);     // 0 = no limit configured
}

static void test_duty_limiter()
{
    DutyLimiter d;
    CHECK(d.allow(1000, 20, 50));
    CHECK(!d.allow(1399, 20, 50));  // 399 us later: 5.01 % -> refused
    CHECK(d.allow(1400, 20, 50));   // exactly the limit
    CHECK(!d.allow(1500, 20, 50));  // refusal did not move the reference...
    CHECK(d.allow(1800, 20, 50));   // ...so 400 us after 1400 is fine
    // 30 fps, 20 us: far below the limit, every frame flashes
    DutyLimiter f;
    int ok = 0;
    for (int i = 0; i < 100; ++i)
        ok += f.allow((int64_t)i * 33333, 20, 50) ? 1 : 0;
    CHECK(ok == 100);
    // 1 kHz SYNC flood with a 1000 us flash: only every 20th passes (5 %)
    DutyLimiter g;
    ok = 0;
    for (int i = 0; i < 1000; ++i)
        ok += g.allow((int64_t)i * 1000, 1000, 50) ? 1 : 0;
    CHECK(ok == 50);
}

static void test_remaining_delay()
{
    CHECK(remainingDelay(1200, 1000, 1000) == 1200);
    CHECK(remainingDelay(1200, 1000, 1030) == 1170);  // 30 us spent handling the SYNC
    CHECK(remainingDelay(1200, 1000, 2300) == 0);     // already late
    CHECK(remainingDelay(1200, 1000, 900) == 1200);   // clock oddity: never extend
}

static void test_state_machine()
{
    Channel c;
    CHECK(apply(c, false, 0, 0, L) == Change::None);
    CHECK(apply(c, true, 1200, 20, L) == Change::Enable);
    CHECK(c.enabled && c.delayUs == 1200 && c.widthUs == 20);
    c.fired = 7;
    CHECK(apply(c, true, 1200, 20, L) == Change::None);
    CHECK(apply(c, true, 900, 5000, L) == Change::Update);
    CHECK(c.delayUs == 900 && c.widthUs == 1000);   // clamped
    CHECK(c.fired == 7);                            // update keeps counters
    CHECK(apply(c, false, 0, 0, L) == Change::Disable);
    CHECK(!c.enabled && c.fired == 7);              // disable keeps the count for read-back
    CHECK(apply(c, true, 10, 10, L) == Change::Enable);
    CHECK(c.fired == 0 && c.skipped == 0);          // re-enable starts from zero
}

int main()
{
    test_clamps();
    test_min_spacing();
    test_duty_limiter();
    test_remaining_delay();
    test_state_machine();
    FINISH("strobe timing");
}
