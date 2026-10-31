# Host flag cutover

Discovery and execution accept only --lmcode-schema, --lmcode-signatures and
--lmcode. Retired options are usage errors before opening bytecode or rendering.
The normal gate proves all three refusals and exact new discovery output.

The SDK is copied with sync-sdk.sh from canonical main 20325f7; its VM uses v6.
Source compiler builds are checked by actual signature and bytecode capabilities,
not their historical package version. DSLB magic and .dslb filenames are unchanged.

Linux's complete 31-check offline gate passes with a clean HOME and the built
host first on PATH. Rendering, image determinism, malformed PNG/font/output,
loader errors, memory budgets and dry-run file protection remain covered.
Cross-host combo examples follow the gate's existing optional dependency rules;
no stale globally installed renderer was used as the product under test.
