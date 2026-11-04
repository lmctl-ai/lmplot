# Invocation envelope and run_lmcode (2026-11-04)

The current compiler (lmscript 0.2.460) has no CLI bytecode files. A script runs a block with r = run_lmcode(p): lmscript compiles it into a private program file, starts lmplot --lmcode FILE and reads one terminal frame back. r is that whole envelope (r.ok, r.result, r.phase, r.code, r.message), so a failure is data and a script must check r.ok. What the host prints with plot.echo goes to lmscript's stdout; it is not in r.

Host side (cpp/main.cpp): SDK lmcode-cpp 93bddf1. --lmcode builds one InvocationResults. Every file is read and loaded first; then each slot is selected, run and completed in order; the first failure stops the run; write_frame prints the single frame. Gnuplot output is finalized after the last part. Exit codes: 0 success; 2 cli (usage, invalid metadata or memory setting); 3 load or run, including an unreadable program file and the memory budget; 4 application (APP_FAIL) or a frame that cannot be written.

A 1 MiB reserve is released before each failure is recorded. Everything gnuplot allocated stays live until shutdown, so without it a budget refusal could throw again inside a catch handler.

Dry mode comes only from invocation metadata. The SDK dry context skips every Plot callback and supplies typed dummies, so nothing renders. The --dry-run flag stays refused. lmplot gnuplot is separate: it refuses invocation metadata and stays a plain gnuplot command line.

Several blocks in one invocation use repeated selectors: b = lmcode_build(["lmplot"]); b.add(p, ["--lmcode", "$DSLFILE"]). They share one gnuplot session.

Tests: test.sh puts a shim named lmplot first on PATH. It runs the fresh build, records the exit code and keeps each private program file. Fixtures run through run_lmcode and print the envelope. The memory checks capture a program from a dry invocation and call the host directly, so ulimit and LMPLOT_MAX_MEMORY_MB apply to the host and not to node. tests/capture.py does the same for the Python protocol and parity tests; the historical parity host cannot answer run_lmcode, so both hosts run the same captured file.

Other hosts bind only when a compatible one is already on PATH (it answers --lmcode-schema --json); nothing here installs a renderer.
