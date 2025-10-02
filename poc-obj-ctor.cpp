#pragma leco test

import jute;
import lispy;
import print;

using namespace lispy;

struct custom_node : public lispy::node {
};

int main() try {
  experimental::basic_context<custom_node> ctx {};
  ctx.fns["music"] = [](auto n, auto aa, auto as) -> const node * {
    return n;
  };
  ctx.run(R"(
    (music)
  )");
} catch (const parser_error & e) {
  errfn("line %d col %d: %s", e.line, e.col, e.msg.begin());
  return 1;
}
