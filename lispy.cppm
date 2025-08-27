#pragma leco add_impl impl
export module lispy;
import hai;
import hashley;
import jojo;
import jute;
import no;
import rng;
import silog;
import sires;

namespace lispy {
  export struct parser_error {
    jute::heap msg;
    unsigned line;
    unsigned col;
  };

  class reader;

  export struct node : no::move {
    jute::view atom {};
    const node * list {};
    const node * next {};
    const reader * r {};
    unsigned loc {};

    void * operator new(traits::size_t n, void * p) { return p; }
  };

  export [[noreturn]] void err(const lispy::node * n, jute::view msg);
  export [[noreturn]] void err(const lispy::node * n, jute::view msg, unsigned rloc);

  export constexpr bool is_atom(const node * n) { return n->atom.size(); }

  export float to_f(const node * n) {
    if (!is_atom(n)) err(n, "expecting number");
    try {
      return jute::to_f(n->atom);
    } catch (...) {
      err(n, "invalid number");
    }
  }
  export int to_i(const node * n) {
    if (!is_atom(n)) err(n, "expecting number");
    try {
      return jute::to_u32(n->atom);
    } catch (...) {
      err(n, "invalid number");
    }
  }

  export struct context;
  using fn_t = const node * (*)(context & ctx, const node * n, const node * const * aa, unsigned as);
  struct context {
    hai::fn<node *> allocator {};
    hashley::fin<const node *> defs { 127 };
    hashley::fin<fn_t> fns { 127 };
  };

  export template<typename T> struct ctx_w_mem {
    hai::array<T> memory { 10240 };
    T * current = memory.begin();
    context ctx {
      .allocator = [this] -> node * {
        if (current == memory.end()) throw 0;
        return current++;
      },
    };
  };

  export [[nodiscard]] const node * eval(context & ctx, const node * n);

  export void run(jute::view filename, context & ctx, hai::fn<void, const node *> callback);
  export void check(jute::view filename, context & ctx) {
    run(filename, ctx, [](auto) {});
  }
}

