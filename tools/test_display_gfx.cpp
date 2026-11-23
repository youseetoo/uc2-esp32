// Host-side check of main/src/display/DisplayGfx.h (no ESP toolchain needed):
//   g++ -std=c++17 -Wall -Imain/src/display tools/test_display_gfx.cpp -o /tmp/test_display_gfx && /tmp/test_display_gfx
// Prints the framebuffer as ASCII art so the layout can be eyeballed, and asserts
// the properties the firmware relies on (clipping, marquee timing, page layout).
#include <cassert>
#include <cstdio>
#include "DisplayGfx.h"

using namespace DisplayGfx;

static bool px(const Frame &f, int x, int y) { return f.buf[(y >> 3) * W + x] & (1u << (y & 7)); }

static void dump(const Frame &f, const char *title)
{
    printf("--- %s\n", title);
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x) putchar(px(f, x, y) ? '#' : '.');
        putchar('\n');
    }
}

int main()
{
    static_assert(sizeof(FONT5X7) == 95 * 5, "font table must cover 0x20..0x7E");

    // 'A' is the well-known 0x7E,0x11,0x11,0x11,0x7E; every printable glyph is non-empty except space
    assert(FONT5X7[('A' - 0x20) * 5 + 0] == 0x7E);
    for (int c = 0x21; c <= 0x7E; ++c)
    {
        int sum = 0;
        for (int i = 0; i < 5; ++i) sum += FONT5X7[(c - 0x20) * 5 + i];
        assert(sum != 0);
    }

    assert(textWidth("", 1) == 0);
    assert(textWidth("openuc2", 2) == (7 * 6 - 1) * 2);

    // --- layout used by DisplayController: big title / status / info or progress bar
    Frame f;
    f.clear();
    const char *title = "openuc2";
    f.drawText((W - textWidth(title, 2)) / 2, 0, title, 2);
    f.drawText(0, 17, "Scanning 12/340", 1);
    f.drawRect(0, 24, W, 8);
    f.fillRect(2, 26, (W - 4) * 37 / 100, 4);
    dump(f, "title + status + 37% progress bar");

    // the 14 px title leaves rows 14/15 empty (page 1 bottom), the status line starts at row 17
    for (int x = 0; x < W; ++x)
        assert(!px(f, x, 14) && !px(f, x, 15) && !px(f, x, 16));

    // --- marquee window: text scrolled partly off the left edge (drawn at -20)
    Frame g;
    g.clear();
    g.drawText(-20, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 2, 0, W);
    dump(g, "marquee window, offset 20 px (2x)");
    assert(px(g, 0, 3) || px(g, 1, 3) || px(g, 2, 3) || px(g, 3, 3));  // something is visible at the left edge

    // clip band: nothing may be drawn outside [10, 50)
    Frame h;
    h.clear();
    h.drawText(-30, 0, "CLIPPING TEST LONG", 2, 10, 50);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (x < 10 || x >= 50) assert(!px(h, x, y));

    // non-printable -> '?'
    Frame q1, q2;
    q1.clear(); q2.clear();
    q1.drawText(0, 0, "\x01", 1);
    q2.drawText(0, 0, "?", 1);
    assert(memcmp(q1.buf, q2.buf, sizeof(q1.buf)) == 0);

    // --- marquee: holds, then steps by STEP_PX every STEP_MS, wraps after width + GAP
    Marquee m;
    m.reset(textWidth("openuc2-metal-slope-23501", 2), 1000);   // 298 px > 128 => scrolls
    assert(m.scrolls());
    assert(!m.step(1000 + Marquee::HOLD_MS - 1));               // still holding
    assert(m.step(1000 + Marquee::HOLD_MS));                    // first step
    assert(m.offset == Marquee::STEP_PX);
    uint32_t t = 1000 + Marquee::HOLD_MS;
    int steps = 0;
    while (m.offset != 0 && steps < 10000) { t += Marquee::STEP_MS; if (m.step(t)) ++steps; }
    assert(m.offset == 0);                                      // wrapped back to the start
    assert(steps == (m.width + Marquee::GAP_PX) / Marquee::STEP_PX - 1);  // 298 + 24 px / 2 px, first step done above
    assert(!m.step(t + 1));                                     // and holds again

    Marquee fits;
    fits.reset(textWidth("openuc2", 2), 0);
    assert(!fits.scrolls());
    assert(!fits.step(100000));                                 // short text never scrolls

    // wrap-around of the 32 bit millis() counter must not freeze or spin the marquee
    Marquee w;
    w.reset(300, 0xFFFFFF00u);
    assert(!w.step(0xFFFFFF10u));
    assert(w.step(0xFFFFFF00u + Marquee::HOLD_MS + 10));
    assert(w.step(0x00000100u + Marquee::HOLD_MS));

    puts("display gfx: all checks passed");
    return 0;
}
