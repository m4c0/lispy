#ifdef LECO_TARGET_WASM
#pragma leco app
#else
#pragma leco test
#endif

import jute;
import lispy;
import print;

using namespace lispy;

static constexpr jute::view src = R"(
  (pr
    (add 3 5)
    (add (add 1 1) (add 2 1)))
)";

struct custom_node : node {
  int val;
  bool is_val;
};

void run() {
  context ctx {
    .allocator = lispy::allocator<custom_node>(),
  };
  ctx.fns["add"] = [](auto ctx, auto n, auto aa, auto as) -> const node * {
    if (as != 2) lispy::err(n, "add expects two coordinates");

    auto a = eval<custom_node>(ctx, aa[0]);
    auto b = eval<custom_node>(ctx, aa[1]);

    auto ai = a->is_val ? a->val : to_i(a);
    auto bi = b->is_val ? b->val : to_i(b);

    auto * nn = clone<custom_node>(&ctx, n);
    nn->val = ai + bi;
    nn->is_val = true;
    return nn;
  };
  ctx.fns["pr"] = [](auto ctx, auto n, auto aa, auto as) -> const node * {
    for (auto i = 0; i < as; i++) {
      auto a = eval<custom_node>(ctx, aa[i]);
      put(a->is_val ? a->val : to_i(a), " ");
    }
    putln();
    return n;
  };

  run(src, ctx);
}

#ifdef LECO_TARGET_WASM
int main() { run(); }
#else
int main() try {
  run();
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
} catch (...) {
  errln("unknown error");
  return 1;
}
#endif
