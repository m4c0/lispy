#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;

struct custom_node : public lispy::node {
  jute::view (custom_node::*attr) {};
  jute::view title {};
  jute::view author {};
};

template<jute::view (custom_node::*A)>
const node * mem_fn(const node * n, const node * const * aa, unsigned as) {
  auto nn = clone<custom_node>(n);
  nn->atom = aa[0]->atom;
  nn->attr = A;
  return nn;
}

int main() try {
  experimental::basic_context<custom_node> ctx {};
  ctx.fns["music"] = [](auto n, auto aa, auto as) -> const node * {
    experimental::basic_context<custom_node> ctx { n->ctx->allocator };
    ctx.fns["title"]  = mem_fn<&custom_node::title>;
    ctx.fns["author"] = mem_fn<&custom_node::author>;

    auto nn = clone<custom_node>(n);
    for (auto i = 0; i < as; i++) {
      auto nat = ctx.eval(aa[i]);
      if (nat->attr) nn->*(nat->attr) = nat->atom;
    }
    return nn;
  };
  auto res = ctx.run(R"(
    (music
      (title Thriller)
      (author M.Jackson))
  )");
  putan("title", res->title);
  putan("author", res->author);
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
}
