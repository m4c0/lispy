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

  export struct context;
  export struct node : no::move {
    jute::view atom {};
    const node * list {};
    const node * next {};
    jute::heap src {};
    unsigned loc {};
    context * ctx {};

    void * operator new(traits::size_t n, void * p) { return p; }
  };

  export [[noreturn]] void err(jute::heap msg);
  export [[noreturn]] void err(const lispy::node * n, jute::heap msg);
  export [[noreturn]] void err(const lispy::node * n, jute::heap msg, unsigned rloc);

  export hai::cstr to_file_err(jute::view filename, const parser_error & e);

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
  export unsigned to_u32(const node * n) {
    if (!is_atom(n)) err(n, "expecting non-negative number");
    auto [v, ok] = jute::to_u32(n->atom);
    if (!ok) err(n, "invalid non-negative number");
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

  using fn_t = const node * (*)(const node * n, const node * const * aa, unsigned as);
  struct context {
    hai::fn<node *> allocator {};
    hashley::fin<const node *> defs { 127 };
    hashley::fin<fn_t> fns { 127 };
    const context * parent {};
  };

  export
  template<traits::base_is<node> T = node>
  class memory {
    hai::array<T> memory { 10240 };
    T * current = memory.begin();

  public:
    node * alloc() {
      using namespace jute::literals;
      if (current == memory.end()) fail({ .msg = "Lispy memory arena exhausted"_hs });
      return current++;
    }
  };
  export template<typename T=node> constexpr auto allocator() {
    return [mem=memory<T>()] mutable -> node * {
      return mem.alloc();
    };
  }

  export void each(jute::view src, lispy::context * ctx, hai::fn<void, lispy::context *, const lispy::node *> fn);

  export template<traits::base_is<node> N> N * clone(context * ctx, const node * n) {
    return new (ctx->allocator()) N { *n };
  }
  export template<traits::base_is<node> N> N * clone(const node * n) {
    return clone<N>(n->ctx, n);
  }

  export template<traits::base_is<node> N> [[nodiscard]] const N * eval(context * ctx, const node * n) {
    return static_cast<const N *>(eval<node>(ctx, n));
  }
  export template<> [[nodiscard]] const node * eval<node>(context * ctx, const node * n);

  export template<traits::base_is<node> N> const N * run(jute::view source, context * ctx) {
    return static_cast<const N *>(run<node>(source, ctx));
  }
  export template<> const node * run<node>(jute::view source, context * ctx);
}

namespace lispy::experimental {
  template<unsigned... Is> struct seq {};

  template<unsigned I, unsigned N, unsigned... Is> struct seq_helper {
    using type = typename seq_helper<I + 1, N, Is..., I>::type;
  };
  template<unsigned N, unsigned... Is> struct seq_helper<N, N, Is...> {
    using type = seq<Is...>;
  };
  template<typename... Args> constexpr auto make_seq() -> typename seq_helper<0, sizeof...(Args)>::type {
    return {};
  }

  template<typename Node, unsigned... I>
  const node * call(auto fn, const node * n, const node * const * aa, seq<I...>) {
    const Node * ee[] { eval<Node>(n->ctx, aa[I])... };
    return fn(n, ee[I]...);
  }

  template<typename Node, typename... Args>
  const node * wrap_fn(const node * (*fn)(const node *, Args...), const node * n, const node * const * aa, unsigned as) {
    if (as != sizeof...(Args)) err(n, "invalid number of parameters");
    return call<Node>(fn, n, aa, make_seq<Args...>());
  }
  export template<typename Node, auto Fn>
  const node * wrap(const node * n, const node * const * aa, unsigned as) {
    return wrap_fn<Node>(Fn, n, aa, as);
  }

  export template<typename Node>
  class basic_context : public context {
  public:
    basic_context() : basic_context { lispy::allocator<Node>() } {}
    explicit basic_context(hai::fn<node *> allocator) : context {
      .allocator = allocator,
    } {}

    auto clone(const node * n) { return clone<Node>(this, n); }
    auto eval(const node * n) { return eval<Node>(this, n); }
    auto run(jute::view src) { return run<Node>(src, this); }
  };

  template<typename T>
  auto clony(const node * n, auto (T::*A)) {
    return clone<T>(n);
  }
  export template<auto Attr, auto Fn>
  const node * mem_set(const node * n, const node * const * aa, unsigned as) {
    if (as != 0) lispy::err(n, "Expecting no parameter");

    auto nn = clony(n, Attr);
    nn->*Attr = Fn;
    return nn;
  }
  export template<auto Attr, auto A>
  const node * mem_flag(const node * n, const node * const * aa, unsigned as) {
    if (as != 0) lispy::err(n, "Expecting no parameter");

    auto nn = clony(n, Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = true; };
    return nn;
  }
  export template<auto Attr, auto A, auto ConvFn>
  const node * mem_fn(const node * n, const node * const * aa, unsigned as) {
    if (as != 1) lispy::err(n, "Expecting a single parameter");

    auto nn = clony(aa[0], Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = ConvFn(n); };
    return nn;
  }
  export template<auto Attr, auto A>
  const node * mem_attr(const node * n, const node * const * aa, unsigned as) {
    if (as != 1) lispy::err(n, "Expecting a single parameter");

    auto nn = clony(aa[0], Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = n->atom; };
    return nn;
  }

  export template<typename T>
  T * fill_clone(context * ctx, const node * n, auto aa, auto as) {
    auto nn = clone<T>(n);
    for (auto i = 0; i < as; i++) {
      auto nat = eval<T>(ctx, aa[i]);
      if (nat->attr) (nat->attr)(nn, nat);
    }
    return nn;
  }
}

