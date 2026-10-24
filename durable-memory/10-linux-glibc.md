# Linux system glibc policy (Rule 14)

From v0.2.1, Linux uses dynamic glibc >= 2.35. gnuplot_core is explicitly STATIC; the rasterizer, codecs and fonts compile into that archive. There are no third-party shared-library searches. CMake keeps BUILD_SHARED_LIBS off and restricts Linux find_library suffixes to .a. The host uses -static-libstdc++ -static-libgcc and never full -static on Linux. Windows keeps full -static; macOS keeps its existing system-library policy.

Both published Linux architectures build on Ubuntu 22.04. test.sh uses scripts/check-linux-linkage.py to require dynamic libc.so.6, permit only the glibc library/loader allowlist, and reject GLIBC symbol requirements above 2.35. When readelf is unavailable, ldd checks dependency names and missing libraries; a note explicitly says that the symbol-version audit requires readelf. CI has binutils/readelf. Eight checker regressions cover allowed and unexpected dependencies, a static executable, too-new glibc, and ldd success/failure paths.

Local Ubuntu 22.04 verification: all 29 test.sh checks pass, including 38 examples and 111 byte-identical images. The three README hero scripts (kepler_rv_fit, potential_vortex_surface, quant_trading_dashboard) also pass in separate fresh directories using build/lmplot on PATH. The stripped Linux x64 binary is 5,584,200 bytes; NEEDED is libm.so.6, libc.so.6, ld-linux-x86-64.so.2, with maximum GLIBC_2.35. No relinkable-object asset is required or generated.

--version remains exactly lmplot X.Y.Z for the installer and release-tag check. The minimum Linux runtime is documented next to the README quickstart and in --notices.

## Published v0.2.1 verification

All five tag-platform jobs and release job passed in [CI](https://github.com/lmctl-ai/lmplot/actions/runs/40056248703). The release has all five binaries, checksums, notices and the required gnuplot patch archive, with no relink-object asset. Empty-directory lmctl autorun with a fresh isolated tool cache selected v0.2.1 and wrote images/kepler_rv_fit.png (76,262 bytes). Both downloaded Linux binaries passed the dependency/ABI checker and SHA256SUMS verification. Linux x64 is 5,584,200 bytes; SHA-256: 238feed2600f7c9ea3ddcd3dcb6391cd73b322a630b22c88b0e31a9a25491e15.
