#pragma leco test

import hai;
import jute;
import lispy;
import print;
import sv;

using namespace lispy;

static constexpr auto src = R"(
  (repl
    (dummy)
    (+ (* 60 2) 200)
    (- 200 80)
    900
  )
)"_sv;
static constexpr auto exp = R"(
  (repl
    test
    320
    120
    900
  )
)"_sv;

int main() try {
  jute::heap res {};
  unsigned prev = 0;

  temp_arena<node> a {};
  temp_frame ctx {};
  ctx.ptrs["res"] = &res;
  ctx.ptrs["prev"] = &prev;
  ctx.fns["dummy"] = [](auto * n, auto aa, auto as) -> const node * {
    auto nn = clone<node>(n);
    nn->atom = "test";
    return nn;
  };
  ctx.fns["repl"] = [](auto * n, auto aa, auto as) -> const node * {
    auto & res = *static_cast<jute::heap *>(context()->ptr("res"));
    auto & prev = *static_cast<unsigned *>(context()->ptr("prev"));

    for (auto i = 0; i < as; i++) {
      auto nn = eval<node>(aa[i]);
      if (!is_atom(nn)) erred(aa[i], "expecting atom as result");

      auto [s, e] = range_of(aa[i]);
      auto prefix = (*n->src).subview(prev, s - prev).middle;
      res = (res + prefix + nn->atom).heap();
      prev = e;
    }
    return n;
  };
  run<node>("no-file", src);

  res = (res + src.subview(prev).after).heap();

  if (res == exp) return 0;

  die("result did not match expectations: [", res, "] [", exp, "]");
} catch (const parser_error & e) {
  errln("error on line ", e.line, " - ", e.msg);
  return 1;
} catch (death) {
  return 2;
}
