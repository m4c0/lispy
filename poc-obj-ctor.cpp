#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;
using namespace lispy::experimental;

struct custom_node : public lispy::node {
  jute::view (custom_node::*attr) {};
  jute::view title {};
  jute::view author {};
};

int main() try {
  basic_context<custom_node> ctx {};
  ctx.fns["music"] = [](auto n, auto aa, auto as) -> const node * {
    basic_context<custom_node> ctx { n->ctx->allocator };
    ctx.fns["title"]  = mem_fn<&custom_node::attr, &custom_node::title>;
    ctx.fns["author"] = mem_fn<&custom_node::attr, &custom_node::author>;
    return fill_clone<custom_node>(&ctx, n, aa, as);
  };
  auto res = ctx.run(R"(
    (music
      (title Thriller)
      (author M.Jackson))
  )");
  if (res->title != "Thriller") err("missing title");
  if (res->author != "M.Jackson") err("missing author");
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
}
