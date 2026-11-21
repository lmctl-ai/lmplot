# AGENTS.md — lmplot

Read durable-memory/index.md first: purpose, gnuplot embedding, schema, PNG terminal, build and CI, limits, the invocation envelope and the browser (WebAssembly) build. When you finish work another agent would need to know about, add a chapter in durable-memory/ and a line in the index in the same commit.

- Build and test: see durable-memory/05-build-run-test.md (`./test.sh`); browser build: scripts/build-web.sh (durable-memory/12-browser-wasm.md).
- Published docs and demo: https://lmctl.com/tools/lmplot/ (source in lmctl-ai/lmctl, tools/lmplot/; publish with the deploy-static workflow, which runs only by dispatch).
