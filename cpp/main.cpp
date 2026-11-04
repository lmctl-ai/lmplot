// lmplot: gnuplot embedded in a single static executable, scriptable from
// lmscript lmcode programs. See durable-memory/ for the design.
#include "dslvm.hpp"
#include "gp_memory.h"
#include "memory_budget.hpp"
#include "notices_embedded.hpp"
#include "schema_embedded.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" {
int lmplot_gp_init(void);
int lmplot_gp_do(const char *cmd);
int lmplot_gp_eval(const char *expr, double *num, char **str);
const char *lmplot_gp_errmsg(void);
int lmplot_gp_output_ok(void);
void lmplot_abort_output(void);
int lmplot_finish_output(void);
void lmplot_gp_shutdown(void);
extern int lmplot_dry_run;
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
  // gnuplot's allocations cannot throw: a refused gp_alloc ends the command with "out of memory",
  // bare mallocs over the budget and png-terminal failures are only noted (gp_memory.h). Either
  // way the budget message is the error.
  static void check_budget() {
    if (const char *failure = lmplot_mem_failure()) throw dslvm::Error(failure);
  }
  void run(const std::string &cmd) {
    init();
    std::fflush(stdout);
    int rc = lmplot_gp_do(cmd.c_str());
    std::fflush(stdout);
    check_budget();
    if (rc != 0) {
      std::string msg = lmplot_gp_errmsg();
      throw dslvm::Error("gnuplot: " + (msg.empty() ? std::string("command failed: ") + cmd : msg));
    }
    if (!lmplot_gp_output_ok()) throw dslvm::Error("gnuplot: error writing the output file (disk full or I/O error)");
  }
  // Plot.num / Plot.str: evaluate, map the non-scalar kinds to errors.
  int eval(const std::string &who, const std::string &expr, double &num, std::string &str) {
    init();
    char *s = nullptr;
    int kind = lmplot_gp_eval(expr.c_str(), &num, &s);
    if (kind == 2) {
      try { str = s; } catch (...) { lmplot_mem_free(s); throw; }
      lmplot_mem_free(s);
    }
    check_budget();
    if (kind == 3) throw dslvm::Error(who + ": value is complex (non-zero imaginary part): " + expr);
    if (kind == 4) throw dslvm::Error(who + ": value is not a number or string (array, datablock?): " + expr);
    if (kind == 0) {
      std::string msg = lmplot_gp_errmsg();
      throw dslvm::Error("gnuplot: " + (msg.empty() ? "expression has no value: " + expr : msg));
    }
    return kind;
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
      // Exclusive creation: never write through a file or symlink someone else placed in a shared
      // temp directory under the (predictable) name.
#ifdef _WIN32
      std::FILE *f = std::fopen(tmp.string().c_str(), "wb");
#else
      int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
      std::FILE *f = fd < 0 ? nullptr : fdopen(fd, "wb");
#endif
      if (!f) throw dslvm::Error("cannot create temporary script " + tmp.string());
      bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size() && std::fputc('\n', f) != EOF;
      ok = std::fclose(f) == 0 && ok;
      if (!ok) { std::error_code ec; fs::remove(tmp, ec); throw dslvm::Error("cannot write temporary script " + tmp.string()); }
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
    for (size_t i = 0; i < name.size(); i++) {
      unsigned char c = (unsigned char)name[i];
      if (!(std::isalpha(c) || (i > 0 && (c == '_' || std::isdigit(c)))))
        throw dslvm::Error("Plot.data: name must start with a letter and contain only letters, digits and _: " + name);
    }
    std::string body = a[1].s;
    if (!body.empty() && body.back() == '\n') body.pop_back();
    // gnuplot ends a datablock at the first line that *starts with* the terminator, and runs the
    // rest as commands; pick a terminator that no body line starts with.
    std::string end = "EOD";
    for (int k = 1;; k++) {
      bool clash = body.compare(0, end.size(), end) == 0;
      for (size_t nl = body.find('\n'); !clash && nl != std::string::npos; nl = body.find('\n', nl + 1))
        clash = body.compare(nl + 1, end.size(), end) == 0;
      if (!clash) break;
      end = "EOD" + std::to_string(k);
    }
    h.run_script("$" + name + " << " + end + "\n" + body + "\n" + end);
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
    double num = 0;
    std::string str;
    if (h.eval("Plot.num", a[0].s, num, str) == 2) throw dslvm::Error("Plot.num: expression is a string: " + a[0].s);
    return Value::real(num);
  });
  checked(r, "str", {"string"}, "string", [&h](void *, Args &a) {
    double num = 0;
    std::string str;
    if (h.eval("Plot.str", a[0].s, num, str) == 2) return Value::string(str);
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
    Host::check_budget();
    if (kind != 2) throw dslvm::Error("GPVAL_TERMINALS unavailable");
    std::string s;
    try { s = str; } catch (...) { lmplot_mem_free(str); throw; }
    lmplot_mem_free(str);
    return Value::string(s);
  });
  checked(r, "echo", {"string"}, "void", [](void *, Args &a) { std::fflush(stdout); std::cout << a[0].s << std::endl; return Value::none(); });
  checked(r, "version", {}, "string", [](void *, Args &) { return Value::string("lmplot " LMPLOT_VERSION); });
  // assert_close(actual, expected, tol): the family's known-answer check (lmnum, lmphysics, lmcad).
  checked(r, "assert_close", {"float", "float", "float"}, "void", [](void *, Args &a) {
    for (int i = 0; i < 3; i++)
      if (!std::isfinite(a[i].f)) throw dslvm::Error("Plot.assert_close: argument " + std::to_string(i + 1) + " is not finite");
    if (a[2].f < 0) throw dslvm::Error("Plot.assert_close: tolerance is negative");
    if (std::fabs(a[0].f - a[1].f) > a[2].f) {
      char buf[160];
      std::snprintf(buf, sizeof buf, "assert_close failed: actual=%.17g, expected=%.17g, tol=%.17g", a[0].f, a[1].f, a[2].f);
      throw dslvm::Error(buf);
    }
    return Value::none();
  });
}

const char *usage =
    "usage: lmplot --version | --notices | --lmcode-schema [--json] | --lmcode-signatures | --lmcode FILE [--lmcode FILE ...]\n"
    "       lmplot gnuplot [GNUPLOT-OPTIONS] [FILE ...]\n"
    "environment: LMPLOT_MAX_MEMORY_MB total memory budget in MiB (default 4096)\n";

int host_main(int argc, char **argv) {
  // Retired host roles/options must never reach the native gnuplot route either.
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--first" || arg == "--last" || arg == "--dry-run" ||
        arg == "--dsl" || arg == "--dsl-schema" || arg == "--dsl-signatures" ||
        arg == "--cli-schema")
      return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", usage));
  }
  if (argc >= 2 && std::string(argv[1]) == "gnuplot") {
    // Invocation metadata is only meaningful on the lmcode execution route.
    // Never let a custom native command bypass its dry/registration contract.
    if (std::getenv("LMCODE_INVOCATION_FILE"))
      return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", "native gnuplot does not accept invocation metadata"));
    if (!std::getenv("GNUTERM"))
#ifdef _WIN32
      _putenv_s("GNUTERM", "dumb");
#else
      setenv("GNUTERM", "dumb", 0);
#endif
    if (!membudget::configure("lmplot", "LMPLOT_MAX_MEMORY_MB"))
      return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", "invalid memory budget"));
    std::vector<char *> args{argv[0]};
    for (int i = 2; i < argc; i++) args.push_back(argv[i]);
    return gnuplot_cli_main(static_cast<int>(args.size()), args.data());
  }
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmplot " LMPLOT_VERSION "\n";
    return std::cout ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmplot::embedded_notices),
                    static_cast<std::streamsize>(lmplot::embedded_notices_size));
    return std::cout ? 0 : 4;
  }
  const std::string schema(reinterpret_cast<const char *>(lmplot::embedded_schema), lmplot::embedded_schema_size);
  if ((argc == 2 || (argc == 3 && std::string(argv[2]) == "--json")) &&
      std::string(argv[1]) == "--lmcode-schema") {
    std::cout << schema;
    return std::cout ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--lmcode-signatures") {
    Host host;
    dslvm::Registry reg;
    register_host(reg, host);
    for (const auto &sig : reg.signatures()) std::cout << sig << "\n";
    return std::cout ? 0 : 4;
  }
  std::vector<std::string> files;
  if (argc < 3 || argc % 2 == 0)
    return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", usage));
  for (int i = 1; i < argc; i += 2) {
    if (std::string(argv[i]) != "--lmcode" || std::string(argv[i+1]).empty())
      return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", usage));
    files.emplace_back(argv[i+1]);
  }
  // Validate the manifest before allocating a host or touching a plot/input file.
  auto results = dslvm::InvocationResults::from_command(files);
  if (!membudget::configure("lmplot", "LMPLOT_MAX_MEMORY_MB"))
    return dslvm::failure_frame("cli", dslvm::Error("CLI_USAGE", "invalid memory budget"));
  Host host;
  dslvm::Registry reg;
  register_host(reg, host);
  dslvm::Context context;
  context.set_dry(results.dry());
  context.set("plot", &host);
  const auto contract = dslvm::ResultContract::from_schema(schema);
  const auto registrations = results.registrations();
  // A program that exhausted the memory budget still needs a failure frame, and everything gnuplot
  // allocated stays live until shutdown: this reserve is released first, so recording the failure
  // cannot throw out of a handler.
  auto reserve = std::make_unique<char[]>(1 << 20);
  // Load and verify every file first: a bad later file must not leave earlier output behind.
  std::vector<std::pair<size_t, dslvm::Program>> programs;
  for (const auto &registration : registrations) {
    // An unreadable program file is a load failure like any other (phase load, exit 3), not an application failure.
    std::string phase = "load";
    try {
      std::ifstream input(std::filesystem::u8path(registration.file), std::ios::binary);
      if (!input) throw dslvm::Error("cannot read bytecode file");
      std::string bytes((std::istreambuf_iterator<char>(input)), {});
      if (input.bad()) throw dslvm::Error("cannot read bytecode file");
      programs.emplace_back(registration.index, dslvm::Program::load(bytes, schema, reg));
    } catch (const dslvm::Error &error) {
      reserve.reset();
      std::cerr << "lmplot: " << error.what() << "\n";
      results.fail(registration.index, phase, error);
      return results.write_frame(std::cout);
    } catch (const std::exception &error) {
      reserve.reset();
      std::cerr << "lmplot: " << error.what() << "\n";
      results.fail(registration.index, phase, dslvm::Error(error.what()));
      return results.write_frame(std::cout);
    }
  }
  for (size_t part = 0; part < programs.size(); ++part) {
    const auto &[index, program] = programs[part];
    std::string phase = "run";
    try {
      results.select(index, reg, contract);
      program.run(context, reg);
      if (part + 1 == programs.size() && host.ready) {
        int fin = lmplot_finish_output();
        if (fin == 1) throw dslvm::Error("gnuplot: multiplot not closed (missing unset multiplot)");
        if (fin == 2) throw dslvm::Error("gnuplot: output file closed with no plots drawn");
        if (fin == 3) throw dslvm::Error("gnuplot: error writing the output file (disk full or I/O error)");
      }
      results.complete(index);
    } catch (const dslvm::Error &error) {
      if (host.ready) lmplot_abort_output();
      reserve.reset();
      if (error.code == "APP_FAIL") phase = "app";
      std::cerr << "lmplot: " << error.what() << "\n";
      results.fail(index, phase, error);
      break;
    } catch (const std::exception &error) {
      if (host.ready) lmplot_abort_output();
      reserve.reset();
      std::cerr << "lmplot: " << error.what() << "\n";
      results.fail(index, phase, dslvm::Error(error.what()));
      break;
    }
  }
  if (host.ready) {
    std::fflush(stdout);
    lmplot_gp_shutdown();
  }
  return results.write_frame(std::cout);
}
} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  try {
#ifdef _WIN32
    int wide_count = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (!wide) throw dslvm::Error("CLI_USAGE", "cannot decode command arguments");
    std::vector<std::string> utf8;
    for (int i = 0; i < wide_count; ++i) {
      int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, nullptr, 0, nullptr, nullptr);
      if (!size) { LocalFree(wide); throw dslvm::Error("CLI_USAGE", "invalid argument encoding"); }
      std::string value(static_cast<size_t>(size), '\0');
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1, value.data(), size, nullptr, nullptr);
      value.pop_back(); utf8.push_back(std::move(value));
    }
    LocalFree(wide);
    std::vector<char *> pointers;
    for (auto &arg : utf8) pointers.push_back(arg.data());
    return host_main(wide_count, pointers.data());
#else
    return host_main(argc, argv);
#endif
  }
  catch (const dslvm::Error &error) { return dslvm::failure_frame("cli", error); }
  catch (const std::exception &error) {
    return dslvm::failure_frame("app", dslvm::Error("APP_FAIL", error.what()));
  }
}
