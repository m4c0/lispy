#pragma leco test

import hai;
import lispy;
import print;

using namespace lispy;

struct custom_node : node {
  int val;
  bool is_val;
};

int main() try {
  hai::array<custom_node> buf { 10240 };
  auto cur = buf.begin();
  alloc_node = [&] -> void * {
    if (cur == buf.end()) throw 0;
    return cur++;
  };

  context ctx {};
  ctx.fns["add"] = [](auto ctx, auto n, auto aa, auto as) -> const node * {
    if (as != 2) lispy::err(n, "add expects two coordinates");

    auto a = static_cast<const custom_node *>(eval(ctx, aa[0]));
    auto b = static_cast<const custom_node *>(eval(ctx, aa[1]));

    auto ai = a->is_val ? a->val : to_i(a);
    auto bi = b->is_val ? b->val : to_i(b);

    auto * nn = new custom_node { *n };
    nn->val = ai + bi;
    nn->is_val = true;
    return nn;
  };

  run("poc.lsp", ctx, [&](auto * node) {
    auto nn = static_cast<const custom_node *>(node);
    if (nn->is_val) putln("result: ", nn->val);
  });

  alloc_node = [] -> void * { return nullptr; };
} catch (const parser_error & e) {
  errfn("poc.lsp:%d:%d: %s", e.line, e.col, e.msg.begin());
  return 1;
} catch (...) {
  errln("poc.lsp: unknown error");
  return 1;
}

