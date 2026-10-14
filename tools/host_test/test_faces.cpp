// Host test for FaceEngine.
//
// There is no display here, so instead of looking at pixels this asserts on the
// bounding boxes every drawing primitive reports. That is enough to catch the
// failure modes that actually matter when adding expressions: geometry that
// runs off the 128x64 panel (where Adafruit_GFX would silently clip it), a face
// that renders nothing, and two faces that are accidentally identical.
//
// Build/run: tools/host_test/run.sh

#include "faces.h"
#include "config.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, ...)                    \
  do {                                      \
    g_checks++;                             \
    if (!(cond)) {                          \
      g_failures++;                         \
      std::printf("  FAIL  ");              \
      std::printf(__VA_ARGS__);             \
      std::printf("\n");                    \
    }                                       \
  } while (0)

int main() {
  Adafruit_GFX gfx(OLED_WIDTH, OLED_HEIGHT);
  FaceEngine engine;
  engine.begin(&gfx);

  std::printf("FaceEngine host tests (%u expressions)\n\n", faceCount());

  CHECK(faceCount() == FACE_COUNT, "faceCount() disagrees with FACE_COUNT");
  CHECK(faceCount() == 12, "expected 12 expressions, got %u", faceCount());

  // ---------------------------- naming -----------------------------------
  std::vector<std::string> names;
  for (uint8_t i = 0; i < faceCount(); i++) {
    const char *n = faceNameAt(i);
    CHECK(n && n[0], "face %u has no name", i);
    names.push_back(n ? n : "");

    const int back = faceFromName(n);
    CHECK(back == (int)i, "faceFromName(\"%s\") returned %d, expected %u",
          n ? n : "?", back, i);
  }

  for (size_t i = 0; i < names.size(); i++)
    for (size_t j = i + 1; j < names.size(); j++)
      CHECK(names[i] != names[j], "duplicate face name \"%s\"",
            names[i].c_str());

  CHECK(faceFromName("nonsense") == -1, "unknown name should return -1");
  CHECK(faceFromName(nullptr) == -1, "null name should return -1");
  CHECK(faceNameAt(200)[0] == '\0', "out-of-range index should be empty");
  CHECK(std::strcmp(faceName((FaceId)99), "neutral") == 0,
        "out-of-range FaceId should fall back to neutral");

  // --------------------------- rendering ---------------------------------
  std::printf("  %-10s %-16s %-14s %s\n", "FACE", "X RANGE", "Y RANGE", "PRIMS");

  std::vector<uint32_t> hashes;
  for (uint8_t i = 0; i < faceCount(); i++) {
    g_stats.reset();
    engine.setFace((FaceId)i);
    engine.render();

    std::printf("  %-10s %3d..%-11d %2d..%-10d %d\n", faceNameAt(i),
                g_stats.minX, g_stats.maxX, g_stats.minY, g_stats.maxY,
                g_stats.calls);

    CHECK(g_stats.calls > 0, "face %s drew nothing", faceNameAt(i));

    CHECK(!g_stats.outOfBounds,
          "face %s draws outside the panel: x %d..%d, y %d..%d", faceNameAt(i),
          g_stats.minX, g_stats.maxX, g_stats.minY, g_stats.maxY);

    // Both eyes sit at x=44 and x=84, so a sane frame always reaches near both
    // sides of the screen. This catches a face drawn in one corner.
    CHECK(g_stats.minX <= 40 && g_stats.maxX >= 88,
          "face %s looks off-centre: x %d..%d", faceNameAt(i), g_stats.minX,
          g_stats.maxX);

    hashes.push_back(g_stats.hash);
  }

  // ------------------------- distinctness --------------------------------
  std::printf("\n");
  for (size_t i = 0; i < hashes.size(); i++) {
    for (size_t j = i + 1; j < hashes.size(); j++) {
      CHECK(hashes[i] != hashes[j],
            "\"%s\" and \"%s\" render identically - one is a copy-paste of the "
            "other",
            names[i].c_str(), names[j].c_str());
    }
  }

  std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
