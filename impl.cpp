module lispy;

using namespace jute::literals;

class lispy::reader : no::no {
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

void lispy::err(const lispy::node * n, jute::view msg) { n->r->err(msg, n->loc); }
void lispy::err(const lispy::node * n, jute::view msg, unsigned rloc) { n->r->err(msg, n->loc + rloc); }

static bool is_atom_char(char c) {
  return c > ' ' && c <= '~' && c != ';' && c != '(' && c != ')';
}
static jute::view next_atom_token(const char * start, lispy::reader & r) {
  while (is_atom_char(r.peek())) r.take();
  return r.token(start);
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

static lispy::node * next_list(lispy::reader & r) {
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
static lispy::node * next_node(lispy::reader & r) {
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

static auto ls(const lispy::node * n) {
  unsigned sz = 0;
  for (auto nn = n->list; nn; nn = nn->next) sz++;
  return sz;
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
  } else if (ctx.defs.has(fn)) {
    return eval(ctx, ctx.defs[fn]);
  } else {
    err(n, *("invalid function name: "_hs + fn));
  }
}

void lispy::run(jute::view filename, lispy::context & ctx, hai::fn<void, const lispy::node *> callback) {
  auto code = jojo::read_cstr(filename);
  reader r { code };
  while (r) callback(eval(ctx, next_node(r)));
}

