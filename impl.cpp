module;
#include <stdio.h>

module lispy;
import rng;

using namespace jute::literals;

[[noreturn]] static void erred(jute::view src, jute::heap msg, unsigned loc) {
  unsigned l = 1;
  unsigned last = 0;
  for (auto i = 0; i < loc; i++) {
    if (src[i] == '\n') {
      last = i;
      l++;
    }
  }
  lispy::fail({ msg, l, loc - last });
}

namespace lispy { class reader; }
class lispy::reader : no::no {
  jute::heap m_data;
  unsigned m_pos {};

public:
  explicit reader(jute::heap data) : m_data { data } {}

  explicit operator bool() const {
    return m_pos < m_data.size();
  }

  jute::heap data() const { return m_data; }
  unsigned loc() const { return m_pos; }
  const char * mark() const { return m_data.begin() + m_pos; }

  char peek() {
    if (m_pos >= m_data.size()) return 0;
    return (*m_data)[m_pos];
  }
  char take() {
    if (m_pos >= m_data.size()) return 0;
    return (*m_data)[m_pos++];
  }

  [[noreturn]] void erred(jute::heap msg) const {
    ::erred(m_data, msg, m_pos);
  }

  jute::view token(const char * start) const {
    return { start, static_cast<unsigned>(mark() - start) };
  }
};
 
void lispy::erred(jute::heap msg) { fail({ msg, 1, 1 }); }
void lispy::erred(const lispy::node * n, jute::heap msg) { ::erred(n->src, msg, n->loc); }
void lispy::erred(const lispy::node * n, jute::heap msg, unsigned rloc) { ::erred(n->src, msg, n->loc + rloc); }

hai::cstr lispy::to_file_err(jute::view filename, const lispy::parser_error & e) {
  char msg[128] {};
  auto len = snprintf(msg, sizeof(msg), "%.*s:%d:%d: %.*s",
      static_cast<unsigned>(filename.size()), filename.begin(),
      e.line, e.col,
      static_cast<unsigned>(e.msg.size()), e.msg.begin());
  if (len > 0) return jute::view { msg, static_cast<unsigned>(len) }.cstr();
  else return jute::view { e.msg }.cstr(); // TODO: can we avoid the double-copy?
}

static bool is_atom_char(char c) {
  return c > ' ' && c <= '~' && c != ';' && c != '(' && c != ')';
}
static jute::view next_atom_token(const char * start, lispy::reader & r) {
  while (is_atom_char(r.peek())) r.take();
  return r.token(start);
}
static jute::view next_string_token(const char * start, lispy::reader & r) {
  while (r && r.peek() != '"') r.take();
  if (!r.peek()) r.erred("string not closed");
  auto res = r.token(start + 1);
  r.take(); // consume '"'
  return res;
}
static void comment(lispy::reader & r) {
  while (r && r.take() != '\n') continue;
}
static jute::view next_token(lispy::reader & r) {
  while (r) {
    auto start = r.mark();
    switch (char c = r.take()) {
      case '\n':
      case '\r':
      case '\t':
      case ' ': break;
      case '(': return r.token(start);
      case ')': return r.token(start);
      case '"': return next_string_token(start, r);
      case ';': comment(r); break;
      default: {
        if (is_atom_char(c)) return next_atom_token(start, r);
        r.erred("character not allowed here");
        break;
      }
    }
  }
  return {};
}

static lispy::node * next_list(lispy::reader & r) {
  auto * res = new (lispy::alloc()) lispy::node {
    .src = r.data(),
    .loc = r.loc(),
  };
  auto * n = &res->list;
  while (r) {
    auto token = next_token(r);
    if (token == ")") return res;
    if (token == "" && !r) break; // Diff. empty token from EOF

    auto nn = (token == "(") ?
      next_list(r) :
      new (lispy::alloc()) lispy::node {
        .atom = token,
        .src = r.data(),
        .loc = static_cast<unsigned>(r.loc() - token.size()),
      };
    *n = nn;
    n = &(nn->next);
  }
  erred(res, "unbalanced open parenthesis");
}
static lispy::node * next_node(lispy::reader & r) {
  if (!r) return {};

  auto token = next_token(r);
  if (token == "") {
    return {};
  } else if (token == "(") {
    return next_list(r);
  } else if (token == ")") {
    r.erred("unbalanced close parenthesis");
  } else {
    return new (lispy::alloc()) lispy::node { 
      .atom = token,
      .src = r.data(),
      .loc = static_cast<unsigned>(r.loc() - token.size()),
    };
  }
}

static auto ls(const lispy::node * n) {
  unsigned sz = 0;
  for (auto nn = n->list; nn; nn = nn->next) sz++;
  return sz;
}

static inline lispy::fn_t find_fn(jute::view fn, lispy::frame * ctx = lispy::context()) {
  if (!ctx) return nullptr;
  else if (ctx->fns.has(fn)) return ctx->fns[fn];
  else if (ctx->parent) return find_fn(fn, ctx->parent);
  else return nullptr;
}

static inline const lispy::node * find_def(jute::view fn, lispy::frame * ctx = lispy::context()) {
  if (!ctx) return nullptr;
  else if (ctx->defs.has(fn)) return ctx->defs[fn];
  else if (ctx->parent) return find_def(fn, ctx->parent);
  else return nullptr;
}

template<> [[nodiscard]] const lispy::node * lispy::eval<lispy::node>(const lispy::node * n) {
  if (!n->list) return n;
  if (!is_atom(n->list)) erred(n->list, "expecting an atom as a function name");

  auto fn = n->list->atom;

  if (fn == "def") {
    if (ls(n) != 3) erred(n, "def requires a name and a value");

    auto args = n->list->next;
    if (!is_atom(args)) erred(args, "def name must be an atom");
    if (!context()) erred(n, "missing lispy frame");
    context()->defs[args->atom] = args->next;
    return args->next;
  }

  const lispy::node * aa[128] {};
  if (ls(n) >= 127) erred(n, "too many parameters");
  auto ap = aa;
  for (auto nn = n->list->next; nn; nn = nn->next) *ap++ = nn;

  if (auto f = find_fn(fn)) {
    return f(n, aa, ap - aa);
  } else if (fn == "do") {
    if (ap == aa) erred(n, "'do' requires at least a parameter");
    const node * res;
    for (auto i = 0; i < ap - aa; i++) res = eval<node>(aa[i]);
    return res;
  } else if (fn == "random") {
    if (ap == aa) erred(n, "random requires at least a parameter");
    return eval<node>(aa[rng::rand(ap - aa)]);
  } else if (auto d = find_def(fn)) {
    return eval<node>(d);
  } else {
    erred(n, ("invalid function name: "_s + fn).heap());
  }
}

void lispy::each(jute::view src, hai::fn<void, const lispy::node *> fn) {
  reader r { jute::heap { src } };
  while (r) {
    const node * nn = next_node(r);
    if (nn) fn(nn);
  }
}

template<> const lispy::node * lispy::run<lispy::node>(jute::view source) {
  const node * n = nullptr;
  each(source, [&](auto nn) {
    n = eval<node>(nn);
  });
  return n;
}
