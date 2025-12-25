#ifdef LECO_TARGET_WASM
#pragma leco app
#else
#pragma leco test
#endif

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

static constexpr jute::view src = R"(
  (+ 3 5 (+ 1 1) (+ 2 1))
)";

void run() {
  temp_arena<node> a {};
  temp_frame ctx {};
  auto r = run<node>("no-file", src);
  if (r->atom != "13") die("invalid result: ", r->atom);
}

#ifdef LECO_TARGET_WASM
int main() { run(); }
#else
int main() try {
  run();
} catch (const parser_error & e) {
  errln("line ", e.line, " col ", e.col, " - ", e.msg);
  return 1;
} catch (...) {
  errln("unknown error");
  return 1;
}
#endif
