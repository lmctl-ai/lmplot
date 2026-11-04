# Build and test

Run scripts/sync-gnuplot.sh, then configure/build with CMake >=3.24 and a C11/C++17
compiler. test.sh builds the current source and runs pins, schema/registry parity,
native linkage, the README/gallery checks, 45 standalone examples, deterministic
reruns, numeric/string/data/image operations, refusals, output finalization,
configurable memory budgets and the protocol suite. Its optional sibling/video
checks still report their actual availability.

Use a private compiler and LMSCRIPT_BIN. Ordinary recipes and fixtures use run_lmcode.
There is no bytecode-export API: tests that need a program file take the private one
the compiler hands to the host, from a test shim named lmplot (test.sh) or from
tests/capture.py (a dry invocation with an explicit recording command). See
11-invocation-envelope.md.

See host-flags.md for strict manifests, dry dummies, result frames and exits
0/2/3/4. Both schema discovery forms emit the exact authored JSON. See 06-ci.md
for the five-platform green-main release flow. No SDK source is patched locally.
