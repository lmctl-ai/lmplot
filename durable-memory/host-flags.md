# Current lmcode host protocol

The public host accepts `--lmcode-schema [--json]` (identical JSON bytes with or
without the option), `--lmcode-signatures`, and repeated `--lmcode FILE` pairs.
Native `gnuplot` passthrough, `--version` and `--notices` remain. Retired host
selectors and schema spellings have no aliases; `--first`, `--last`, and
`--dry-run` also fail with CLI exit 2 in the native route before dispatch.

The SDK is vendored with sync-sdk.sh at full reviewed C++ SHA
`eed67ea5f059152809784846d82bd510bb5bcdf2` (wp188 native runtime qualification).
Per-file SHA256s in sdk.lock verify the unmodified copy. SDK fixes belong to
its owning lane; do not patch the vendored runtime here.

`LMCODE_INVOCATION_FILE` selects invocation manifest v1. Missing/nonboolean
`dry`, unknown keys, mismatched registrations and malformed metadata fail with
exit 2 before host creation, plot setup or file access. There is no retry or
implicit repair. Without a manifest, standalone execution is live and slots
follow command occurrence order. Manifest indices place builder results even
when the submitted file order differs. Domain execution shares one gnuplot
session across parts; each part has an independently sealed owned result sink.

The SDK dry context skips domain callbacks and supplies typed dummies. Thus
plot commands, temporary script files, renders and shell commands never execute
in dry mode; there is no fallback to the former native dry interpreter.
Pure std arithmetic and explicit result assembly remain available. No optional
host preview is registered; dry plotting values are dummies, not predictions.

Exactly one terminal LMCODE_RETURN_BEGIN / LMCODE_RETURN_END frame carries
success/null/structured results or a structured failure without a partial result.
Exit codes are 0 success, 2 CLI/manifest, 3 load/runtime and 4 application failure
(e.g. unreadable bytecode). First failure identifies its original builder part
and prevents later dispatch. Native output finalization errors remain run errors;
partial output is aborted before terminal framing.

No host deadlines or output-byte ceilings apply. The configurable total-memory
budget is retained; platform integer/size overflow checks remain. The renderer
no longer rejects a dimension at 30,000 pixels or a 100-million-pixel area.
Its 100,000-vertex path buffer flushes and continues, so it is a streaming buffer,
not a point-count ceiling. Library format/platform representation limits remain.

`test.sh` runs the current compiler/host checks, operation parity and the retained
rendering/error/PNG/font/budget suite on the same five native CI platforms. The
compiler is installed in a private prefix, pinned to 0.2.456. Tests construct
private compiler-library fixtures through compile-test.mjs; ordinary examples
use only run_lmcode, without exporting bytecode.

A green main push publishes the version in CMakeLists.txt through the repo's CI.
Existing release versions are immutable: later documentation pushes never replace
their assets. Pin `install="github:lmctl-ai/lmplot@v0.3.0"` downstream.
