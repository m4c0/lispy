#pragma leco add_impl impl
export module lispy;
import hai;
import hashley;
import jute;
import no;
import sv;

#ifdef LECO_TARGET_WASM
import vaselin;
#endif

namespace lispy {
  export struct parser_error {
    jute::heap file;
    jute::heap msg;
    unsigned line;
    unsigned col;
  };

  export struct node : no::move {
    jute::heap atom {};
    const node * list {};
    const node * next {};
    jute::heap src {};
    jute::heap file {};
    unsigned loc {};

    void * operator new(traits::size_t n, void * p) { return p; }
  };

  export [[noreturn]] void erred(jute::heap msg);
  export [[noreturn]] void erred(const lispy::node * n, jute::heap msg);
  export [[noreturn]] void erred(const lispy::node * n, jute::heap msg, unsigned rloc);

  export hai::cstr to_file_err(const parser_error & e);

  export constexpr bool is_atom(const node * n) { return n->atom.size(); }

  export jute::view to_atom(const node * n) {
    if (!is_atom(n)) erred(n, "expecting atom");
    return n->atom;
  }
  export float to_f(const node * n) {
    if (!is_atom(n)) erred(n, "expecting number");
    auto [v, ok] = jute::to_f(n->atom);
    if (!ok) erred(n, "invalid number");
    return v;
  }
  export int to_i(const node * n) {
    if (!is_atom(n)) erred(n, "expecting number");
    auto [v, ok] = jute::to_u32(n->atom);
    if (!ok) erred(n, "invalid number");
    return v;
  }
  export unsigned to_u32(const node * n) {
    if (!is_atom(n)) erred(n, "expecting non-negative number");
    auto [v, ok] = jute::to_u32(n->atom);
    if (!ok) erred(n, "invalid non-negative number");
    return v;
  }

#ifdef LECO_TARGET_WASM
  export [[noreturn]] void fail(parser_error erred) {
    vaselin::console_error(erred.msg.begin(), erred.msg.size());
    vaselin::raise_error();
  }
#else
  export [[noreturn]] void fail(parser_error erred) {
    throw erred;
  }
#endif

  constexpr inline unsigned find_start(const node * n) {
    if (is_atom(n)) return n->loc;

    auto l = n->loc;
    while (l && (*n->src)[l] != '(') l--;
    return l;
  }
  constexpr inline unsigned find_end(const node * n) {
    if (is_atom(n)) return n->loc + to_atom(n).size();

    auto last = n->list;
    while (last->next) last = last->next;

    auto l = last->loc;
    while (l < n->src.size() && (*n->src)[l] != ')') l++;
    return l + 1;
  }
  export constexpr auto range_of(const node * n) {
    struct pair {
      unsigned sloc;
      unsigned eloc;
    } res {
      .sloc = find_start(n),
      .eloc = find_end(n),
    };
    return res;
  }

  using fn_t = const node * (*)(const node * n, const node * const * aa, unsigned as);
  export class frame {
  protected:
    frame() = default;

  public:
    hashley::fin<const node *> defs { 127 };
    hashley::fin<fn_t> fns { 127 };
    hashley::fin<void *> ptrs { 127 };

    // This class is meant for long-term storage. This minor uptr nuisance
    // forces users to notice they should either use temp_arena or keep this
    // reference around.
    static auto make() { return hai::uptr { new frame() }; }
  };

  export class frame_guard;
  export frame_guard * & context() {
    static thread_local frame_guard * i {};
    return i;
  }
  // TODO: read-only flag
  class frame_guard : no::no {
    frame_guard * m_prev_frame;
    frame * m_frame;

  public:
    frame_guard(frame * f) :
      m_prev_frame { context() }
    , m_frame { f }
    {
      context() = this;
    }
    frame_guard(hai::uptr<frame> & f) : frame_guard { &*f } {}
    ~frame_guard() { context() = m_prev_frame; }

    void def(sv name, const node * val) {
      m_frame->defs[name] = val;
    }
    void fn(sv name, fn_t val) {
      m_frame->fns[name] = val;
    }
    void ptr(sv name, void * val) {
      m_frame->ptrs[name] = val;
    }

    [[nodiscard]] const node * def(sv name) const {
      if (m_frame->defs.has(name)) return m_frame->defs[name];
      else if (m_prev_frame) return m_prev_frame->def(name);
      else return nullptr;
    }
    [[nodiscard]] fn_t fn(sv name) const {
      if (m_frame->fns.has(name)) return m_frame->fns[name];
      else if (m_prev_frame) return m_prev_frame->fn(name);
      else return nullptr;
    }
    [[nodiscard]] void * ptr(sv name) const {
      if (m_frame->ptrs.has(name)) return m_frame->ptrs[name];
      else if (m_prev_frame) return m_prev_frame->ptr(name);
      else return nullptr;
    }
  };

  export class temp_frame : public frame {
    frame_guard g { this };
  };

  using alloc_t = hai::fn<void *>;
  alloc_t & memory() {
    static thread_local alloc_t i = [] -> void * {
      using namespace jute::literals;
      fail({ .msg = "Trying to use uninitialised lispy memory"_hs });
    };
    return i;
  }
  auto alloc() { return memory()(); }

  class memory_guard : no::no {
    alloc_t m_prev_alloc;

  public:
    memory_guard(alloc_t t) : m_prev_alloc { t } {}
    ~memory_guard() { memory() = m_prev_alloc; }
  };

  // Uses uninitialised memory and only deletes pointers actually allocated
  export
  template<traits::base_is<node> T>
  class arena : no::no {
    struct entry {
      alignas(T) unsigned char buf[sizeof(T)] {};
    };
    static constexpr const auto buffer_size = 10240;
    static constexpr const auto entry_size = sizeof(entry);
    entry * m_buffer = ::new entry[buffer_size];
    entry * m_current = m_buffer;

    void * alloc() {
      using namespace jute::literals;
      if (m_current == m_buffer + buffer_size) fail({ .msg = "Lispy memory arena exhausted"_hs });
      return m_current++;
    }

  protected:
    arena() = default;

  public:
    ~arena() {
      for (auto p = m_buffer; p != m_current; p++) {
        reinterpret_cast<T *>(p)->~T();
      }
      delete[] m_buffer;
    }

    [[nodiscard]] auto use() {
      alloc_t prev = memory();
      memory() = [this] { return this->alloc(); };
      return memory_guard { prev };
    }

    // This class is meant for long-term storage. This minor uptr nuisance
    // forces users to notice they should either use temp_arena or keep this
    // reference around.
    static auto make() { return hai::uptr { new arena() }; }
  };

  export
  template<traits::base_is<node> T>
  class temp_arena : arena<T> {
    memory_guard g = arena<T>::use();
  };

  export void each(jute::view filename, jute::view src, hai::fn<void, const lispy::node *> fn);

  export template<traits::base_is<node> N> N * clone(const node * n) {
    return new (alloc()) N { *n };
  }

  export template<traits::base_is<node> N> [[nodiscard]] const N * eval(const node * n) {
    return static_cast<const N *>(eval<node>(n));
  }
  export template<> [[nodiscard]] const node * eval<node>(const node * n);

  export template<traits::base_is<node> N> const N * run(jute::view filename, jute::view source) {
    return static_cast<const N *>(run<node>(filename, source));
  }
  export template<> const node * run<node>(jute::view filename, jute::view source);
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
    const Node * ee[] { eval<Node>(aa[I])... };
    return fn(n, ee[I]...);
  }

  template<typename Node, typename... Args>
  const node * wrap_fn(const node * (*fn)(const node *, Args...), const node * n, const node * const * aa, unsigned as) {
    if (as != sizeof...(Args)) erred(n, "invalid number of parameters");
    return call<Node>(fn, n, aa, make_seq<Args...>());
  }
  export template<typename Node, auto Fn>
  const node * wrap(const node * n, const node * const * aa, unsigned as) {
    return wrap_fn<Node>(Fn, n, aa, as);
  }

  template<typename T>
  auto clony(const node * n, auto (T::*A)) {
    return clone<T>(n);
  }
  template<typename T>
  auto clone_eval(const node * n, auto (T::*A)) {
    return clone<T>(eval<T>(n));
  }
  export template<auto Attr, auto Fn>
  const node * mem_set(const node * n, const node * const * aa, unsigned as) {
    if (as != 0) erred(n, "Expecting no parameter");

    auto nn = clony(n, Attr);
    nn->*Attr = Fn;
    return nn;
  }
  export template<auto Attr, auto A>
  const node * mem_flag(const node * n, const node * const * aa, unsigned as) {
    if (as != 0) erred(n, "Expecting no parameter");

    auto nn = clony(n, Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = true; };
    return nn;
  }
  export template<auto Attr, auto A, auto ConvFn>
  const node * mem_fn(const node * n, const node * const * aa, unsigned as) {
    if (as != 1) erred(n, "Expecting a single parameter");

    auto nn = clone_eval(aa[0], Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = ConvFn(n); };
    return nn;
  }
  export template<auto Attr, auto A>
  const node * mem_attr(const node * n, const node * const * aa, unsigned as) {
    if (as != 1) erred(n, "Expecting a single parameter");

    auto nn = clone_eval(aa[0], Attr);
    nn->*Attr = [](auto * self, auto * n) { self->*A = n->atom; };
    return nn;
  }

  export template<typename T>
  T * fill_clone(const node * n, auto aa, auto as) {
    auto nn = clone<T>(n);
    for (auto i = 0; i < as; i++) {
      auto nat = eval<T>(aa[i]);
      if (nat->attr) (nat->attr)(nn, nat);
    }
    return nn;
  }
}

