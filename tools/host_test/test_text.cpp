// Host test for TextScreen: UTF-8 folding, word wrap, scrolling and duration.
//
// Compiled against tools/host_test/shim/ so text.cpp runs natively - this is
// the one non-trivial piece of pure logic in the firmware, and it is worth
// proving before it ever reaches the OLED.
//
// Build/run: tools/host_test/run.sh

#include "text.h"
#include "config.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, ...)                            \
  do {                                              \
    g_checks++;                                     \
    if (!(cond)) {                                  \
      g_failures++;                                 \
      std::printf("  FAIL  ");                      \
      std::printf(__VA_ARGS__);                     \
      std::printf("\n");                            \
    }                                               \
  } while (0)

static std::vector<std::string> renderRows(TextScreen &s, uint32_t elapsed) {
  g_calls.clear();
  Adafruit_GFX gfx(128, 64);
  s.render(&gfx, elapsed);
  std::vector<std::string> rows;
  for (const GfxCall &c : g_calls) rows.push_back(c.text);
  return rows;
}

static void test_short() {
  std::printf("short message\n");
  TextScreen s;
  s.setText("Hello world");
  CHECK(!s.empty(), "text should not be empty");

  auto rows = renderRows(s, 0);
  CHECK(rows.size() == 1, "expected 1 row, got %zu", rows.size());
  if (!rows.empty()) CHECK(rows[0] == "Hello world", "row was \"%s\"", rows[0].c_str());

  CHECK(s.durationMs() == 6000, "expected 6000 ms, got %u", s.durationMs());

  // 11 chars * 6 px = 66 px, centred in 128 -> x = 31
  CHECK(g_calls.size() == 1 && g_calls[0].x == 31,
        "expected x=31, got %d", g_calls.empty() ? -1 : g_calls[0].x);
}

static void test_wrap() {
  std::printf("word wrapping\n");
  TextScreen s;
  s.setText("the quick brown fox jumps over the lazy dog and then keeps "
            "running for quite a while indeed");
  auto rows = renderRows(s, 0);
  CHECK(rows.size() > 1, "expected wrapping, got %zu row(s)", rows.size());
  for (const auto &r : rows) {
    CHECK(r.size() <= TEXT_COLS, "row \"%s\" is %zu chars", r.c_str(), r.size());
  }
  // Nothing may be lost: rejoined words must match the input.
  std::string joined;
  for (const auto &r : rows) {
    if (!joined.empty()) joined += " ";
    joined += r;
  }
  CHECK(joined == "the quick brown fox jumps over the lazy dog and then keeps "
                  "running for quite a while indeed",
        "text was altered:\n    got  \"%s\"", joined.c_str());
}

static void test_accents() {
  std::printf("accent folding\n");
  TextScreen s;

  s.setText("Caf\xC3\xA9 d\xC3\xA9j\xC3\xA0 vu");
  CHECK(std::strcmp(s.raw(), "Cafe deja vu") == 0,
        "expected \"Cafe deja vu\", got \"%s\"", s.raw());

  // \xC3\xAF is i-diaeresis, the 3-byte em dash should vanish entirely.
  s.setText("na\xC3\xAFve \xE2\x80\x94 ok");
  CHECK(std::strcmp(s.raw(), "naive  ok") == 0,
        "expected \"naive  ok\", got \"%s\"", s.raw());

  // A 4-byte emoji must be dropped, not truncated into mojibake.
  s.setText("hi \xF0\x9F\x98\x80 there");
  CHECK(std::strcmp(s.raw(), "hi  there") == 0,
        "expected \"hi  there\", got \"%s\"", s.raw());
}

static void test_newlines() {
  std::printf("explicit newlines\n");
  TextScreen s;
  s.setText("Line one\nLine two");
  auto rows = renderRows(s, 0);
  CHECK(rows.size() == 2, "expected 2 rows, got %zu", rows.size());
  if (rows.size() == 2) {
    CHECK(rows[0] == "Line one", "row 0 was \"%s\"", rows[0].c_str());
    CHECK(rows[1] == "Line two", "row 1 was \"%s\"", rows[1].c_str());
  }
}

static void test_long_word() {
  std::printf("over-long word\n");
  TextScreen s;
  s.setText("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");   // 30 chars, no spaces
  auto rows = renderRows(s, 0);
  CHECK(rows.size() == 2, "expected a hard split into 2 rows, got %zu", rows.size());
  if (rows.size() == 2) {
    CHECK(rows[0].size() == TEXT_COLS, "first chunk was %zu chars", rows[0].size());
    CHECK(rows[1].size() == 10, "second chunk was %zu chars", rows[1].size());
  }
}

static void test_scrolling() {
  std::printf("scrolling long text\n");
  TextScreen s;
  // Must wrap to MORE than TEXT_VISIBLE_ROWS rows or there is nothing to scroll.
  s.setText("one two three four five six seven eight nine ten eleven twelve "
            "thirteen fourteen fifteen sixteen seventeen eighteen nineteen "
            "twenty");

  auto atStart = renderRows(s, 0);
  CHECK(atStart.size() == TEXT_VISIBLE_ROWS,
        "expected %d visible rows, got %zu", TEXT_VISIBLE_ROWS, atStart.size());
  std::string firstTop = atStart.empty() ? "" : atStart[0];

  // After the 1100 ms hold plus one step, the window must have moved down.
  auto scrolled = renderRows(s, 1100 + TEXT_PER_LINE_MS + 10);
  CHECK(!scrolled.empty() && scrolled[0] != firstTop,
        "content did not scroll (still \"%s\")", scrolled[0].c_str());

  const uint32_t extra = (uint32_t)(/*rows*/ 8 - TEXT_VISIBLE_ROWS);
  CHECK(s.durationMs() == 6000 + extra * TEXT_PER_LINE_MS,
        "expected %u ms for a scrolling message, got %u",
        6000 + extra * TEXT_PER_LINE_MS, s.durationMs());
}

static void test_empty() {
  std::printf("empty message\n");
  TextScreen s;
  s.setText("");
  CHECK(s.empty(), "empty text should report empty");
  auto rows = renderRows(s, 0);
  CHECK(rows.empty(), "nothing should be drawn, got %zu calls", rows.size());
  CHECK(s.durationMs() == 6000, "empty should still report a sane duration");
}

static void test_null() {
  std::printf("null input\n");
  TextScreen s;
  s.setText(nullptr);
  CHECK(s.empty(), "null should be treated as empty");
}

int main() {
  std::printf("TextScreen host tests (TEXT_COLS=%d, TEXT_VISIBLE_ROWS=%d)\n\n",
              TEXT_COLS, TEXT_VISIBLE_ROWS);

  test_short();
  test_wrap();
  test_accents();
  test_newlines();
  test_long_word();
  test_scrolling();
  test_empty();
  test_null();

  std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
