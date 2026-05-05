# Repository Guidelines

## Project Structure & Module Organization

`include/rpc_core/` contains the C++14 header-only RPC library. Public entry points are in `include/rpc_core.hpp` and `include/rpc_core/*.hpp`; implementation details live under `include/rpc_core/detail/`, serializers under `include/rpc_core/serialize/`, and optional integrations under `include/rpc_core/plugin/`. C++ tests live in `cpp/test/`, with serializer fixtures in `cpp/test/serialize/`, plugin fixtures in `cpp/test/plugin/`, and built-in C++ SDK TCP adapter checks in `cpp/test/rpc*.cpp`. The Rust crate is isolated in `rust/`, with library code in `rust/src/`, network support in `rust/src/net/`, examples in `rust/src/examples/`, and integration tests in `rust/src/tests/`.

## Build, Test, and Development Commands

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`: configure the C++ test build.
- `cmake --build build -j`: build `rpc_core_test`.
- `./build/rpc_core_test`: run the C++ test executable.
- `cmake -S . -B build -DRPC_CORE_TEST_PLUGIN=ON && cmake --build build --target rpc_core_test_init`: enable plugin tests and fetch local third-party headers under `cpp/thirdparty/`.
- `cd rust && cargo fmt -- --check`: verify Rust formatting.
- `cd rust && cargo build`: build the Rust crate.
- `cd rust && cargo test --test rpc`: run core Rust tests.
- `cd rust && cargo test --test net_tcp --features net && cargo test --test net_rpc --features net`: run network feature tests.

## Coding Style & Naming Conventions

C++ uses `.clang-format` with Google style, 150-column limit, and empty short functions/lambdas allowed on one line. Keep public headers self-contained, prefer `snake_case` file names matching existing headers, and place internal helpers under `detail/`. Rust uses standard `rustfmt`; keep module and file names `snake_case`, types `UpperCamelCase`, and constants `SCREAMING_SNAKE_CASE`.

## Testing Guidelines

Add C++ coverage in the nearest `cpp/test/test_*.cpp` file, or create a focused test file under `cpp/test/` and include it from `CMakeLists.txt`. Use the existing assertion helpers in `cpp/test/assert_def.h`. Add Rust integration coverage in `rust/src/tests/`; tests requiring Tokio networking must declare or use the `net` feature path and be runnable with `--features net`.

Every SDK that provides RPC networking must include three independently executable RPC checks: `rpc`, `rpc_s`, and `rpc_c`.
`rpc` should be a self-contained end-to-end client/server test. `rpc_s` should start a standalone RPC server process, and
`rpc_c` should start a standalone RPC client process that can talk to `rpc_s`. Keep the names or target/script names as close
to `rpc`, `rpc_s`, and `rpc_c` as the language ecosystem allows, so cross-language and manual compatibility testing stays
predictable.

## Commit & Pull Request Guidelines

Recent commits use Conventional Commit prefixes such as `feat:`, `fix:`, `refactor:`, `chore:`, and `doc:`. Use `type: [scope] summary` when a change is scoped to one language or module. Rust crate changes must use the `[rust]` scope, for example `feat: [rust] add tcp transport` or `fix: [rust] remove blocking api`. Keep messages short and scoped when helpful. Pull requests should describe behavioral changes, list C++ and Rust commands run, link related issues, and call out feature flags or platform-specific impact.

## Security & Configuration Tips

Do not commit generated build folders, IDE settings, or fetched `cpp/thirdparty/` dependencies. Keep external dependency downloads limited to the documented plugin test initialization path.
