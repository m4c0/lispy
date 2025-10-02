#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

struct custom_node : public lispy::node {
  void (*attr)(custom_node *, const custom_node *);
  jute::view title {};
  jute::view author {};
  unsigned rate {};
};

int main() try {
  basic_context<custom_node> ctx {};
  ctx.fns["music"] = [](auto n, auto aa, auto as) -> const node * {
    basic_context<custom_node> ctx { n->ctx->allocator };
    ctx.fns["title"]  = mem_attr<&custom_node::attr, &custom_node::title>;
    ctx.fns["author"] = mem_attr<&custom_node::attr, &custom_node::author>;
    ctx.fns["rate"] = mem_fn<&custom_node::attr, &custom_node::rate, to_i>;
    return fill_clone<custom_node>(&ctx, n, aa, as);
  };
  auto res = ctx.run(R"(
    (music
      (rate 1)
      (title Thriller)
      (author M.Jackson))
  )");
  if (res->title != "Thriller") err("missing title");
  if (res->author != "M.Jackson") err("missing author");
  if (res->rate != 1) err("wrong rate");
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
}
