#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

struct cnode : node {};
void run() {
  temp_arena<cnode> a {};
  temp_frame ctx {};
  ctx.fns["pr"] = [](auto n, auto aa, auto as) -> const node * {
    for (auto i = 0; i < as; i++) put(eval<cnode>(aa[i])->atom);
    putln();
    return n;
  };
  run<cnode>("file-1", "(def X (random a b c)) (def Y (X))");

  temp_frame ctx2 {};
  ctx2.fns["echo"] = [](auto n, auto aa, auto as) -> const node * {
    return eval<cnode>(aa[0]);
  };
  run<cnode>("file-2", "(def Z (Y)) (pr (echo (X)) (Y) (Z))");
}

int main() try {
  run();
} catch (const parser_error & e) {
  errfn("line %d col %d -- %s", e.line, e.col, e.msg.begin());
  return 1;
} catch (...) {
  errln("unknown error");
  return 1;
}
