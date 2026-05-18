# About Project
Hybrid (CPU + OpenGL) Photon Beam Splatter for Participating Media Rendering in C++

Details see `docs/project/overview.md`

# Rules
- Keep answers concise
- Keep code changes minimum, split target into smaller tasks if necessary

# Dev Workflow
We break the whole project into small chunk and each time we will only tackle one of them.
For each chunk, we will follow a TDD workflow (Red-Green-Refactor) with tests partially suggested and written by the agents.

# CMake
Always link external dependencies locally (assume they will be installed) but not fetch them via `FetchContent` or git submodules.

### Configure Command
```
cmake -S . -B build \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -G Ninja
```

### Build Command
```
cmake --build build -j8
```

# Coding Conventions
- For C++ conventions, see `docs/conventions/cpp.md`

- For shader (GLSL) conventions, see `docs/conventions/glsl.md`  

# Testing
We use [Catch2 v3](https://github.com/catchorg/Catch2).

- Test sources live in `tests/`, named `*_test.cpp`.
- Each `TEST_CASE` is auto-registered with CTest via `catch_discover_tests`.
- Tests build by default; disable with `-DBUILD_TESTING=OFF` at configure time.

### Run tests
```
cmake --build build -j8
ctest --test-dir build --output-on-failure
```