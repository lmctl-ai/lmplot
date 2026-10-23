// lmplot: gnuplot embedded in a single static executable, scriptable from
// lmscript `_dsl_` bytecode. See durable-memory/ for the design.
#include "dslvm.hpp"
#include "notices_embedded.hpp"
#include "schema_embedded.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

extern "C" {
int lmplot_gp_init(void);
int lmplot_gp_do(const char *cmd);
int lmplot_gp_eval(const char *expr, double *num, char **str);
const char *lmplot_gp_errmsg(void);
void lmplot_gp_shutdown(void);
int gnuplot_cli_main(int argc, char **argv);
}

namespace {
using dslvm::Args;
using dslvm::Value;

std::string quote(const std::string &s) {
  // gnuplot double-quoted strings: backslash and quote escapes.
  std::string out = "\"";
  for (char c : s) {
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  return out + "\"";
}

struct Host {
  bool ready = false;
  int seq = 0;
  void init() {
    if (ready) return;
    if (lmplot_gp_init() != 0) throw dslvm::Error("gnuplot initialization failed");
    ready = true;
  }
  void run(const std::string &cmd) {
    init();
    std::fflush(stdout);
    int rc = lmplot_gp_do(cmd.c_str());
    std::fflush(stdout);
    if (rc != 0) {
      std::string msg = lmplot_gp_errmsg();
      throw dslvm::Error("gnuplot: " + (msg.empty() ? std::string("command failed: ") + cmd : msg));
    }
  }
  // Multi-line text goes through a temp file and `load` so datablocks and
  // bracketed clauses work exactly as in a .gp file.
  void run_script(const std::string &text) {
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() /
                   ("lmplot-" + std::to_string(
#ifdef _WIN32
                                    _getpid()
#else
                                    getpid()
#endif
                                        ) + "-" + std::to_string(seq++) + ".gp");
    {
      std::ofstream f(tmp, std::ios::binary);
      if (!f) throw dslvm::Error("cannot create temporary script " + tmp.string());
      f << text << "\n";
    }
    struct Cleanup {
      fs::path p;
      ~Cleanup() { std::error_code ec; fs::remove(p, ec); }
    } cleanup{tmp};
    run("load " + quote(tmp.generic_string()));
  }
};

template <class F> Value translate(F &&fn) {
  try {
    return fn();
  } catch (const dslvm::Error &) {
    throw;
  } catch (const std::exception &e) {
    throw dslvm::Error(e.what());
  }
}

void checked(dslvm::Registry &r, const char *name, std::vector<std::string> params, std::string returns,
             dslvm::Method cb) {
  r.method("Plot", name, params, returns, [params, cb, name](void *self, Args &a) {
    for (size_t i = 0; i < params.size(); i++) {
      const auto &want = params[i];
      const auto &v = a[i];
      bool ok = want == "float"    ? v.kind == Value::FLOAT
                : want == "int"    ? v.kind == Value::INT
                : want == "string" ? v.kind == Value::STR
                                   : false;
      if (!ok)
        throw dslvm::Error(std::string("Plot.") + name + " argument " + std::to_string(i + 1) + " must be " + want);
    }
    return translate([&] { return cb(self, a); });
  });
}

void register_host(dslvm::Registry &r, Host &h) {
  r.context("plot", "Plot");
  checked(r, "cmd", {"string"}, "void", [&h](void *, Args &a) { h.run(a[0].s); return Value::none(); });
  checked(r, "script", {"string"}, "void", [&h](void *, Args &a) { h.run_script(a[0].s); return Value::none(); });
  checked(r, "load", {"string"}, "void", [&h](void *, Args &a) { h.run("load " + quote(a[0].s)); return Value::none(); });
  checked(r, "data", {"string", "string"}, "void", [&h](void *, Args &a) {
    const std::string &name = a[0].s;
    if (name.empty() || name[0] == '$') throw dslvm::Error("Plot.data: name is given without the leading $");
    std::string body = a[1].s;
    if (!body.empty() && body.back() == '\n') body.pop_back();
    h.run_script("$" + name + " << EOD\n" + body + "\nEOD");
    return Value::none();
  });
  checked(r, "png", {"string", "int", "int"}, "void", [&h](void *, Args &a) {
    h.run("set terminal png size " + std::to_string(a[1].i) + "," + std::to_string(a[2].i) + "; set output " + quote(a[0].s));
    return Value::none();
  });
  checked(r, "svg", {"string", "int", "int"}, "void", [&h](void *, Args &a) {
    h.run("set terminal svg size " + std::to_string(a[1].i) + "," + std::to_string(a[2].i) + "; set output " + quote(a[0].s));
    return Value::none();
  });
  checked(r, "close", {}, "void", [&h](void *, Args &) { h.run("set output"); return Value::none(); });
  checked(r, "num", {"string"}, "float", [&h](void *, Args &a) {
    h.init();
    double num = 0;
    char *str = nullptr;
    int kind = lmplot_gp_eval(a[0].s.c_str(), &num, &str);
    if (kind == 2) { std::free(str); throw dslvm::Error("Plot.num: expression is a string: " + a[0].s); }
    if (kind != 1) throw dslvm::Error(std::string("gnuplot: ") + lmplot_gp_errmsg());
    return Value::real(num);
  });
  checked(r, "str", {"string"}, "string", [&h](void *, Args &a) {
    h.init();
    double num = 0;
    char *str = nullptr;
    int kind = lmplot_gp_eval(a[0].s.c_str(), &num, &str);
    if (kind == 2) { std::string s = str; std::free(str); return Value::string(s); }
    if (kind != 1) throw dslvm::Error(std::string("gnuplot: ") + lmplot_gp_errmsg());
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.15g", num);
    return Value::string(buf);
  });
  checked(r, "terminals", {}, "string", [&h](void *, Args &) {
    h.init();
    double num = 0;
    char *str = nullptr;
    h.run("lmplot_terms__ = GPVAL_TERMINALS");
    int kind = lmplot_gp_eval("lmplot_terms__", &num, &str);
    if (kind != 2) throw dslvm::Error("GPVAL_TERMINALS unavailable");
    std::string s = str;
    std::free(str);
    return Value::string(s);
  });
  checked(r, "echo", {"string"}, "void", [](void *, Args &a) { std::fflush(stdout); std::cout << a[0].s << std::endl; return Value::none(); });
  checked(r, "version", {}, "string", [](void *, Args &) { return Value::string("lmplot " LMPLOT_VERSION); });
}

const char *usage =
    "usage: lmplot --version | --notices | --dsl-schema [--json] | --dsl-signatures | --dsl FILE [FILE ...]\n"
    "       lmplot gnuplot [GNUPLOT-OPTIONS] [FILE ...]   (the embedded gnuplot command line)\n";

}  // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  if (argc >= 2 && std::string(argv[1]) == "gnuplot") {
    std::vector<char *> args;
    args.push_back(argv[0]);
    for (int i = 2; i < argc; i++) args.push_back(argv[i]);
    return gnuplot_cli_main(static_cast<int>(args.size()), args.data());
  }
  Host host;
  dslvm::Registry reg;
  register_host(reg, host);
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmplot " LMPLOT_VERSION "\n";
    return std::cout ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmplot::embedded_notices),
                    static_cast<std::streamsize>(lmplot::embedded_notices_size));
    return std::cout ? 0 : 4;
  }
  if (argc >= 2 && std::string(argv[1]) == "--dsl-schema") {
    if (argc == 2) {
      for (const auto &s : reg.signatures()) std::cout << s << "\n";
      return std::cout ? 0 : 4;
    }
    if (argc == 3 && std::string(argv[2]) == "--json") {
      std::cout.write(reinterpret_cast<const char *>(lmplot::embedded_schema),
                      static_cast<std::streamsize>(lmplot::embedded_schema_size));
      return std::cout ? 0 : 4;
    }
    std::cerr << usage;
    return 2;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &s : reg.signatures()) std::cout << s << "\n";
    return 0;
  }
  if (argc < 3 || std::string(argv[1]) != "--dsl") {
    std::cerr << usage;
    return 2;
  }
  for (int i = 2; i < argc; i++) {
    if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) {
      std::cerr << usage;
      return 2;
    }
  }
  const std::string schema(reinterpret_cast<const char *>(lmplot::embedded_schema), lmplot::embedded_schema_size);
  dslvm::Context context;
  context.set("plot", &host);
  // Load and verify every file first so a corrupt later file cannot leave
  // earlier output half-written.
  std::vector<dslvm::Program> programs;
  for (int i = 2; i < argc; i++) {
    try {
      programs.push_back(dslvm::Program::load(dslvm::read_file(argv[i]), schema, reg));
    } catch (const dslvm::Error &error) {
      return dslvm::report_error("load", error);
    }
  }
  int status = 0;
  for (const auto &program : programs) {
    try {
      program.run(context, reg);
    } catch (const dslvm::Error &error) {
      status = dslvm::report_error("run", error);
      break;
    }
    dslvm::report_ok();
  }
  if (host.ready) {
    std::fflush(stdout);
    lmplot_gp_shutdown();
  }
  return status;
}
