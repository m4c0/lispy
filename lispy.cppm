export module lispy;
import hai;
import hashley;
import jojo;
import jute;
import no;
import rng;
import silog;
import sires;

using namespace jute::literals;

namespace lispy {
  export struct parser_error {
    jute::heap msg;
    unsigned line;
    unsigned col;
  };
}

class reader : no::no {
  jute::view m_data;
  unsigned m_pos {};

public:
  explicit reader(jute::view data) : m_data { data } {}

  explicit operator bool() const {
    return m_pos < m_data.size();
  }

  unsigned loc() const { return m_pos; }
  const char * mark() const { return m_data.begin() + m_pos; }

  char peek() {
    if (m_pos >= m_data.size()) return 0;
    return m_data[m_pos];
  }
  char take() {
    if (m_pos >= m_data.size()) return 0;
    return m_data[m_pos++];
  }

  [[noreturn]] void err(jute::view msg, unsigned loc) const {
    unsigned l = 1;
    unsigned last = 0;
    for (auto i = 0; i < loc; i++) {
      if (m_data[i] == '\n') {
        last = i;
        l++;
      }
    }
    throw lispy::parser_error { msg, l, loc - last };
  }
  [[noreturn]] void err(jute::view msg) const {
    err(msg, m_pos);
  }

  jute::view token(const char * start) const {
    return { start, static_cast<unsigned>(mark() - start) };
  }
};

static bool is_atom_char(char c) {
  return c > ' ' && c <= '~' && c != ';' && c != '(' && c != ')';
}
static jute::view next_atom_token(const char * start, reader & r) {
  while (is_atom_char(r.peek())) r.take();
  return r.token(start);
}
static void comment(reader & r) {
  while (r && r.take() != '\n') continue;
}
static jute::view next_token(reader & r) {
  while (r) {
    auto start = r.mark();
    switch (char c = r.take()) {
      case '\n':
      case '\r':
      case '\t':
      case ' ': break;
      case '(': return r.token(start);
      case ')': return r.token(start);
      case ';': comment(r); break;
      default: {
        if (is_atom_char(c)) return next_atom_token(start, r);
        r.err("character not allowed here");
        break;
      }
    }
  }
  return {};
}

namespace lispy {
  export struct node : no::move {
    jute::view atom {};
    const node * list {};
    const node * next {};
    const reader * r {};
    unsigned loc {};

    void * operator new(traits::size_t n);
  };

  export [[noreturn]] void err(const lispy::node * n, jute::view msg) { n->r->err(msg, n->loc); }
}

static lispy::node * next_list(reader & r) {
  auto * res = new lispy::node {
    .r = &r,
    .loc = r.loc(),
  };
  auto * n = &res->list;
  while (r) {
    auto token = next_token(r);
    if (token == ")") return res;
    if (token == "") break;

    auto nn = (token == "(") ?
      next_list(r) :
      new lispy::node {
        .atom = token,
        .r = &r,
        .loc = static_cast<unsigned>(r.loc() - token.size()),
      };
    *n = nn;
    n = &(nn->next);
  }
  err(res, "unbalanced open parenthesis");
}
static lispy::node * next_node(reader & r) {
  if (!r) return {};

  auto token = next_token(r);
  if (token == "(") {
    return next_list(r);
  } else if (token == ")") {
    r.err("unbalanced close parenthesis");
  } else {
    return new lispy::node { 
      .atom = token,
      .r = &r,
      .loc = static_cast<unsigned>(r.loc() - token.size()),
    };
  }
}

namespace lispy {
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
}

static auto ls(const lispy::node * n) {
  unsigned sz = 0;
  for (auto nn = n->list; nn; nn = nn->next) sz++;
  return sz;
}

namespace lispy {
  export struct context;
  using fn_t = const node * (*)(context & ctx, const node * n, const node * const * aa, unsigned as);
  struct context {
    hashley::fin<const node *> defs { 127 };
    hashley::fin<fn_t> fns { 127 };
  };

  export [[nodiscard]] const node * eval(context & ctx, const node * n);
}
[[nodiscard]] const lispy::node * lispy::eval(lispy::context & ctx, const lispy::node * n) {
  if (!n->list) return n;
  if (!is_atom(n->list)) err(n->list, "expecting an atom");

  auto fn = n->list->atom;

  if (fn == "def") {
    if (ls(n) != 3) err(n, "def requires a name and a value");

    auto args = n->list->next;
    if (!is_atom(args)) err(args, "def name must be an atom");
    ctx.defs[args->atom] = args->next;
    return args->next;
  }

  const lispy::node * aa[128] {};
  if (ls(n) >= 127) err(n, "too many parameters");
  auto ap = aa;
  for (auto nn = n->list->next; nn; nn = nn->next) *ap++ = nn;
  
  if (ctx.fns.has(fn)) {
    return ctx.fns[fn](ctx, n, aa, ap - aa);
  } else if (fn == "random") {
    if (ls(n) == 0) err(n, "rand requires at least a parameter");
    return eval(ctx, aa[rng::rand(ls(n) - 1)]);
  } else if (ctx.defs.has(fn)) {
    return eval(ctx, ctx.defs[fn]);
  } else {
    err(n, *("invalid function name: "_hs + fn));
  }
}

namespace lispy {
  export hai::fn<void *> alloc_node {};

  export void run(jute::view filename, context & ctx, auto && callback) {
    auto code = jojo::read_cstr(filename);
    reader r { code };
    while (r) callback(eval(ctx, next_node(r)));
  }
  export void check(jute::view filename, context & ctx) {
    run(filename, ctx, [](auto) {});
  }
}

void * lispy::node::operator new(traits::size_t sz) { return lispy::alloc_node(); }
