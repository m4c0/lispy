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
  bool approved {};
  bool checked : 1 {};
};

int main() try {
  basic_context<custom_node> ctx {};
  ctx.fns["music"] = [](auto n, auto aa, auto as) -> const node * {
    basic_context<custom_node> ctx { n->ctx->allocator };
    ctx.fns["title"]  = mem_attr<&custom_node::attr, &custom_node::title>;
    ctx.fns["author"] = mem_attr<&custom_node::attr, &custom_node::author>;
    ctx.fns["rate"] = mem_fn<&custom_node::attr, &custom_node::rate, to_i>;
    ctx.fns["approved"] = mem_flag<&custom_node::attr, &custom_node::approved>;
    ctx.fns["checked"] = mem_set<&custom_node::attr, [](auto * self, auto * n) {
      self->checked = true;
    }>;
    return fill_clone<custom_node>(&ctx, n, aa, as);
  };
  auto res = ctx.run(R"(
    (music
      (approved)
      (checked)
      (rate 1)
      (title Thriller)
      (author M.Jackson))
  )");
  if (res->title != "Thriller") err("missing title");
  if (res->author != "M.Jackson") err("missing author");
  if (!res->approved) err("not approved");
  if (!res->checked) err("not checked");
  if (res->rate != 1) err("wrong rate");
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
}
