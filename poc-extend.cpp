#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

struct cnode : node {};
void run() {
  basic_context<cnode> ctx {};
  ctx.fns["pr"] = [](auto n, auto aa, auto as) -> const node * {
    for (auto i = 0; i < as; i++) put(eval<cnode>(n->ctx, aa[i])->atom);
    putln();
    return n;
  };
  ctx.run("(def X x) (def Y (X))");

  ctx.run("(def Z (Y)) (pr (X) (Y) (Z))");
}

int main() try {
  run();
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
} catch (...) {
  errln("unknown error");
  return 1;
}
