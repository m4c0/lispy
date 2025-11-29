#pragma leco test

import hai;
import jute;
import lispy;
import print;

using namespace lispy;

using jit_t = hai::fn<void>;
jit_t compile(jute::view source) {
  context ctx {};

  hai::chain<jit_t> subs { 16 };
  lispy::each(source, &ctx, [](auto, auto) {
    putln("o");
  });

  return [subs=traits::move(subs)]() mutable {
    for (auto & fn : subs) {
      fn();
    }
  };
}

static constexpr jute::view src = R"(
(do
  (pr (add 3 5))
  (pr (add (add 1 1) (add 2 1)))
)
)";

using fn_t = hai::fn<int>;
using sfn_t = hai::sptr<fn_t>;
struct custom_node : node {
  sfn_t fn {};
};

void run() {
  lispy::temp_arena<custom_node> mem {};
  lispy::context ctx {};
  ctx.fns["do"] = [](auto n, auto aa, auto as) -> const node * {
    hai::array<sfn_t> fns { as };
    for (auto i = 0; i < as; i++) {
      fns[i] = eval<custom_node>(n->ctx, aa[i])->fn;
    }

    auto * nn = clone<custom_node>(n);
    nn->fn = sfn_t{new fn_t{[fns=traits::move(fns)] mutable {
      auto res = 0;
      for (auto fn : fns) {
        res = (*fn)();
      }
      return res;
    }}};
    return nn;
  };
  ctx.fns["add"] = [](auto n, auto aa, auto as) -> const node * {
    if (as != 2) lispy::err(n, "add expects two coordinates");

    auto a = eval<custom_node>(n->ctx, aa[0]);
    auto b = eval<custom_node>(n->ctx, aa[1]);

    auto ai = a->fn ? a->fn : sfn_t{new fn_t{[i=to_i(a)] { return i; }}};
    auto bi = b->fn ? b->fn : sfn_t{new fn_t{[i=to_i(b)] { return i; }}};

    auto * nn = clone<custom_node>(n);
    nn->fn = sfn_t{new fn_t{[ai, bi] mutable {
      return (*ai)() + (*bi)();
    } }};
    return nn;
  };
  ctx.fns["pr"] = [](auto n, auto aa, auto as) -> const node * {
    if (as != 1) lispy::err(n, "pr expects a single argument");

    auto a = eval<custom_node>(n->ctx, aa[0]);
    auto ai = a->fn ? a->fn : sfn_t{new fn_t{[i=to_i(a)] { return i; }}};

    auto * nn = clone<custom_node>(n);
    nn->fn = sfn_t{new fn_t{[ai] mutable {
      auto i = (*ai)();
      putln(i);
      return i;
    }}};
    return nn;
  };

  compile(src)();
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
