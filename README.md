# Analyzer

Analyzer is a separate Git repository from `main`. Its remote, branches, and history must remain independent. The
standalone Linux build consumes headers and already-built libraries from a neighbouring `main` checkout; it does not
add Analyzer to the `main` CMake project and does not alter either repository's Git configuration.

## Prerequisites

- Linux with Ninja, CMake, GCC, and the same Qt 5 kit used to build `main`.
- A configured and built `main` tree. Analyzer requires `main`'s generated `version.h`, Qwt archive, and shared
  libraries.
- Matching build types, compilers, and Qt installations for `main` and Analyzer. CMake checks the build type and
  compiler and stops on a mismatch.

The current workspace's active `main` kit is Qt 5.15.2/GCC, Release, with this build directory:

```text
/home/ad/git_repo/build/Desktop_Qt_5_15_2_GCC_64bit-Release
```

## Configure and build

Run the commands from the workspace root. Use a build directory outside the Analyzer source repository:

```sh
/opt/Qt/Tools/CMake/bin/cmake \
  -S /home/ad/git_repo/labs/apps/analyzer \
  -B /home/ad/git_repo/build/analyzer-Release \
  -G Ninja \
  -DMAIN_BUILD_DIR=/home/ad/git_repo/build/Desktop_Qt_5_15_2_GCC_64bit-Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM=/opt/Qt/Tools/Ninja/ninja

/opt/Qt/Tools/CMake/bin/cmake --build /home/ad/git_repo/build/analyzer-Release
```

The build produces `analyser` and, by default, the `rdb_false_tracks_check` command-line tool. To omit the tool, add
`-DANALYZER_BUILD_TOOLS=OFF` when configuring.

`MAIN_SOURCE_DIR` defaults to the workspace's neighbouring `main/` directory. Override it only when the repositories
use a different layout:

```text
-DMAIN_SOURCE_DIR=/path/to/main
```

`MAIN_LIBRARY_DIR` and `MAIN_QWT_LIBRARY` are derived from the selected main build but can be overridden for a
non-standard main output layout. The old `.pro` files are retained only as historical records; CMake is the supported
build authority.
