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
  (pr
    (add 3 5)
    (add (add 1 1) (add 2 1)))
  (pr
    (do
      (pr 1)
      (pr (random 2 3 4))))
  (pr (sub 69 34))
)";

struct custom_node : node {
  int val;
  bool is_val;
};

static const node * sub(const node * n, const custom_node * a, const custom_node * b) {
  auto ai = a->is_val ? a->val : to_i(a);
  auto bi = b->is_val ? b->val : to_i(b);

  auto * res = clone<custom_node>(n);
  res->val = ai - bi;
  res->is_val = true;
  return res;
}

void run() {
  temp_arena<custom_node> a {};
  temp_frame ctx {};
  ctx.fns["add"] = [](auto n, auto aa, auto as) -> const node * {
    if (as != 2) lispy::erred(n, "add expects two coordinates");

    auto a = eval<custom_node>(aa[0]);
    auto b = eval<custom_node>(aa[1]);

    auto ai = a->is_val ? a->val : to_i(a);
    auto bi = b->is_val ? b->val : to_i(b);

    auto * nn = clone<custom_node>(n);
    nn->val = ai + bi;
    nn->is_val = true;
    return nn;
  };
  ctx.fns["sub"] = wrap<custom_node, sub>;
  ctx.fns["pr"] = [](auto n, auto aa, auto as) -> const node * {
    auto res = static_cast<const custom_node *>(n);
    for (auto i = 0; i < as; i++) {
      res = eval<custom_node>(aa[i]);
      put(res->is_val ? res->val : to_i(res), " ");
    }
    putln();
    return res;
  };

  run<node>(src);
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
