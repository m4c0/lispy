#pragma leco add_impl impl
export module lispy;
import hai;
import hashley;
import jute;
import no;

#ifdef LECO_TARGET_WASM
import vaselin;
#endif

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
    auto [v, ok] = jute::to_f(n->atom);
    if (!ok) err(n, "invalid number");
    return v;
  }
  export int to_i(const node * n) {
    if (!is_atom(n)) err(n, "expecting number");
    auto [v, ok] = jute::to_u32(n->atom);
    if (!ok) err(n, "invalid number");
    return v;
  }

#ifdef LECO_TARGET_WASM
  export [[noreturn]] void fail(parser_error err) {
    vaselin::console_error(err.msg.begin(), err.msg.size());
    vaselin::raise_error();
  }
#else
  export [[noreturn]] void fail(parser_error err) {
    throw err;
  }
#endif

  export struct context;
  using fn_t = const node * (*)(context & ctx, const node * n, const node * const * aa, unsigned as);
  struct context {
    hai::fn<node *> allocator {};
    hashley::fin<const node *> defs { 127 };
    hashley::fin<fn_t> fns { 127 };
  };

  export template<typename T, typename C = context> struct ctx_w_mem {
    hai::array<T> memory { 10240 };
    T * current = memory.begin();
    C ctx {};

    ctx_w_mem() {
      ctx.allocator = [this] -> node * {
        if (current == memory.end()) fail({});
        return current++;
      };
    }
  };

  export [[nodiscard]] const node * eval(context & ctx, const node * n);

  export void run(jute::view source, context & ctx, hai::fn<void, const node *> callback);
  export void check(jute::view source, context & ctx) {
    run(source, ctx, [](auto) {});
  }
}

