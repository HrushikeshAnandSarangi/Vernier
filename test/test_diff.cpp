#include "Vernier/Diff.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace vernier;

int main() {
  // Test 1: Identical strings produce empty diff
  {
    std::string Text = "line1\nline2\nline3\n";
    std::string Diff = computeUnifiedDiff("foo", Text, Text);
    assert(Diff.empty() && "Identical text must produce empty diff");
  }

  // Test 2: Simple addition/deletion
  {
    std::string Before = "line1\nline2\nline3\n";
    std::string After = "line1\nline2_modified\nline3\n";
    std::string Diff = computeUnifiedDiff("foo", Before, After);
    assert(!Diff.empty() && "Modified text must produce non-empty diff");
    assert(Diff.find("--- foo (before)") != std::string::npos);
    assert(Diff.find("+++ foo (after)") != std::string::npos);
    assert(Diff.find("- line2") != std::string::npos);
    assert(Diff.find("+ line2_modified") != std::string::npos);
  }

  // Test 3: Fallback on line cap exceeded
  {
    std::string Before;
    std::string After;
    for (int i = 0; i < 20; ++i) {
      Before += "old_line_" + std::to_string(i) + "\n";
      After += "new_line_" + std::to_string(i) + "\n";
    }
    DiffOptions Opts;
    Opts.MaxDiffLines = 5; // Very small cap
    std::string Diff = computeUnifiedDiff("cap_test", Before, After, Opts);
    assert(!Diff.empty());
    assert(Diff.find("exceeds 5 max diff threshold") != std::string::npos);
  }

  std::cout << "test_diff passed all assertions!\n";
  return 0;
}
