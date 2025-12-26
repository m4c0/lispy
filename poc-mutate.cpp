#pragma leco test

import hai;
import jute;
import lispy;
import print;
import sv;

using namespace lispy;

static constexpr auto src = R"(
  (repl
    (do)
    (+ (* 60 2) 200)
    (- 200 80)
    900
  )
)"_sv;
// static constexpr auto exp = R"(
//   (repl
//     310
//     120
//     900
//   )
// )"_sv;

int find_start(const node * n) {
  if (is_atom(n)) return n->loc;

  auto l = n->loc;
  while (l && (*n->src)[l] != '(') l--;
  return l;
}
int find_end(const node * n) {
  if (is_atom(n)) return n->loc + to_atom(n).size();

  auto last = n->list;
  while (last->next) last = last->next;

  auto l = last->loc;
  while (l < n->src.size() && (*n->src)[l] != ')') l++;
  return l + 1;
}

int main() try {
  struct repl {
    int start;
    int end;
    jute::heap txt;
  };
  hai::chain<repl> repls { 16 };

  temp_arena<node> a {};
  temp_frame ctx {};
  ctx.ptrs["repls"] = &repls;
  ctx.fns["repl"] = [](auto * n, auto aa, auto as) -> const node * {
    auto repls = static_cast<hai::chain<repl> *>(context()->ptr("repls"));
    for (auto i = 0; i < as; i++) {
      auto start = find_start(aa[i]);
      auto end = find_end(aa[i]);
      repls->push_back({ start, end, "uga" });
    }
    return n;
  };
  run<node>("no-file", src);

  auto prev = 0;
  for (auto [s, e, txt] : repls) {
    auto prefix = src.subview(prev, s - prev).middle;
    put(prefix, txt);

    prev = e;
  }
  put(src.subview(prev).after);
} catch (const parser_error & e) {
  errln("error on line ", e.line, " - ", e.msg);
  return 1;
} catch (death) {
  return 2;
}
