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
  (add 3 5)
  (add (add 1 1) (sub 2 1))
)";

struct custom_node : node {
  int val;
  bool is_val;
};

struct ctx : context {
  custom_node * sub(auto n, auto a, auto b) {
    auto * nn = clone<custom_node>(this, n);
    nn->val = a->val + b->val;
    nn->is_val = true;
    return nn;
  }
};

const node * fn(context & ctx, const node * n, const node * const * aa, unsigned as) {
  if (as != 2) lispy::err(n, "sub expects two coordinates");

  return static_cast<struct ctx &>(ctx).sub(static_cast<const custom_node *>(n),
    eval<custom_node>(ctx, aa[0]),
    eval<custom_node>(ctx, aa[1])
  );
} 

void run() {
  ctx_w_mem<custom_node> cm {};
  cm.ctx.fns["add"] = [](auto ctx, auto n, auto aa, auto as) -> const node * {
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
  cm.ctx.fns["sub"] = fn;

  run(src, cm.ctx, [&](auto * node) {
    auto nn = static_cast<const custom_node *>(node);
    if (nn->is_val) putln("result: ", nn->val);
  });
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
