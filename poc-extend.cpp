#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

struct cnode : node {};
void run() {
  temp_arena<cnode> a {};
  basic_context<cnode> ctx {};
  ctx.fns["pr"] = [](auto n, auto aa, auto as) -> const node * {
    for (auto i = 0; i < as; i++) put(eval<cnode>(n->ctx, aa[i])->atom);
    putln();
    return n;
  };
  ctx.run("(def X (random a b c)) (def Y (X))");

  basic_context<cnode> ctx2 {};
  ctx2.parent = &ctx;
  ctx2.fns["echo"] = [](auto n, auto aa, auto as) -> const node * {
    return eval<cnode>(n->ctx, aa[0]);
  };
  ctx2.run("(def Z (Y)) (pr (echo (X)) (Y) (Z))");
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
