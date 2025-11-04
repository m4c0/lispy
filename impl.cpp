module;
#include <stdio.h>

module lispy;
import rng;

using namespace jute::literals;

[[noreturn]] static void err(jute::view src, jute::heap msg, unsigned loc) {
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
  jute::view m_data;
  unsigned m_pos {};

public:
  explicit reader(jute::view data) : m_data { data } {}

  explicit operator bool() const {
    return m_pos < m_data.size();
  }

  jute::view data() const { return m_data; }
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

  [[noreturn]] void err(jute::heap msg) const {
    ::err(m_data, msg, m_pos);
  }

  jute::view token(const char * start) const {
    return { start, static_cast<unsigned>(mark() - start) };
  }
};
 
void lispy::err(jute::heap msg) { fail({ msg, 1, 1 }); }
void lispy::err(const lispy::node * n, jute::heap msg) { ::err(n->src, msg, n->loc); }
void lispy::err(const lispy::node * n, jute::heap msg, unsigned rloc) { ::err(n->src, msg, n->loc + rloc); }

hai::cstr lispy::to_file_err(jute::view filename, const lispy::parser_error & e) {
  char msg[128] {};
  auto len = snprintf(msg, sizeof(msg), "%.*s:%d:%d: %.*s",
      static_cast<unsigned>(filename.size()), filename.begin(),
      e.line, e.col,
      static_cast<unsigned>(e.msg.size()), e.msg.begin());
  if (len > 0) return jute::view { msg, static_cast<unsigned>(len) }.cstr();
  else return (*e.msg).cstr();
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
  if (!r.peek()) r.err("string not closed");
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
        r.err("character not allowed here");
        break;
      }
    }
  }
  return {};
}

static lispy::node * next_list(lispy::context * ctx, lispy::reader & r) {
  auto * res = new (ctx->allocator()) lispy::node {
    .src = r.data(),
    .loc = r.loc(),
    .ctx = ctx,
  };
  auto * n = &res->list;
  while (r) {
    auto token = next_token(r);
    if (token == ")") return res;
    if (token == "") break;

    auto nn = (token == "(") ?
      next_list(ctx, r) :
      new (ctx->allocator()) lispy::node {
        .atom = token,
        .src = r.data(),
        .loc = static_cast<unsigned>(r.loc() - token.size()),
        .ctx = ctx,
      };
    *n = nn;
    n = &(nn->next);
  }
  err(res, "unbalanced open parenthesis");
}
static lispy::node * next_node(lispy::context * ctx, lispy::reader & r) {
  if (!r) return {};

  auto token = next_token(r);
  if (token == "") {
    return {};
  } else if (token == "(") {
    return next_list(ctx, r);
  } else if (token == ")") {
    r.err("unbalanced close parenthesis");
  } else {
    return new (ctx->allocator()) lispy::node { 
      .atom = token,
      .src = r.data(),
      .loc = static_cast<unsigned>(r.loc() - token.size()),
      .ctx = ctx,
    };
  }
}

static auto ls(const lispy::node * n) {
  unsigned sz = 0;
  for (auto nn = n->list; nn; nn = nn->next) sz++;
  return sz;
}

static inline lispy::fn_t find_fn(const lispy::context * ctx, jute::view fn) {
  if (ctx->fns.has(fn)) return ctx->fns[fn];
  else if (ctx->parent) return find_fn(ctx->parent, fn);
  else return nullptr;
}

static inline const lispy::node * find_def(const lispy::context * ctx, jute::view fn) {
  if (ctx->defs.has(fn)) return ctx->defs[fn];
  else if (ctx->parent) return find_def(ctx->parent, fn);
  else return nullptr;
}

template<> [[nodiscard]] const lispy::node * lispy::eval<lispy::node>(lispy::context * ctx, const lispy::node * n) {
  if (!n->list) return n;
  if (!is_atom(n->list)) err(n->list, "expecting an atom as a function name");

  auto fn = n->list->atom;

  if (fn == "def") {
    if (ls(n) != 3) err(n, "def requires a name and a value");

    auto args = n->list->next;
    if (!is_atom(args)) err(args, "def name must be an atom");
    ctx->defs[args->atom] = args->next;
    return args->next;
  }

  const lispy::node * aa[128] {};
  if (ls(n) >= 127) err(n, "too many parameters");
  auto ap = aa;
  for (auto nn = n->list->next; nn; nn = nn->next) *ap++ = nn;

  if (auto f = find_fn(ctx, fn)) {
    return f(n, aa, ap - aa);
  } else if (fn == "do") {
    if (ap == aa) err(n, "'do' requires at least a parameter");
    const node * res;
    for (auto i = 0; i < ap - aa; i++) res = eval<node>(ctx, aa[i]);
    return res;
  } else if (fn == "random") {
    if (ap == aa) err(n, "random requires at least a parameter");
    return eval<node>(ctx, aa[rng::rand(ap - aa)]);
  } else if (auto d = find_def(ctx, fn)) {
    return eval<node>(ctx, d);
  } else {
    err(n, "invalid function name: "_hs + fn);
  }
}

template<> const lispy::node * lispy::run<lispy::node>(jute::view source, lispy::context * ctx) {
  reader r { source };
  const node * n = nullptr;
  while (r) {
    auto nn = next_node(ctx, r);
    if (nn) n = eval<node>(ctx, nn);
  }
  return n;
}

