# Contributing

Bug reports, questions and pull requests are welcome.

## Reporting a problem

Open an issue and include:

- the compiler and its version (`g++ --version`),
- the flags you compile with,
- a short program that shows the problem, and
- the complete compiler output, if it is a compile error.

A [Compiler Explorer](https://godbolt.org) link is the quickest way to share
a program. [Installation](docs/installation.md#try-it-in-the-browser)
explains how to include the library there.

If you are not sure whether something is a bug in the library or in the
compiler, open the issue anyway.
[Known compiler problems](docs/troubleshooting.md#known-compiler-problems)
lists the ones found so far.

## Building

You need GCC 16 or newer, CMake 3.28 or newer, and Python 3 for the single
header. [Installation](docs/installation.md#get-a-compiler) shows how to get
the compiler.

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Without GCC 16 on your machine, the same works in a container:

```sh
docker run --rm -it -v "$PWD":/src -w /src gcc:16 bash
apt-get update && apt-get install -y cmake ninja-build python3
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
```

Every push and every pull request runs these steps on GitHub Actions, see
[.github/workflows/ci.yml](.github/workflows/ci.yml).

## Layout

| Path | Contents |
| --- | --- |
| `include/reflect/` | The library. One header per topic, `reflect.hpp` includes all of them. |
| `include/reflect/detail/` | Helpers that are not part of the API. |
| `single_include/reflect/reflect.hpp` | The library as one file. Generated. |
| `tests/` | One program per header, plus the snippets of the documentation. |
| `examples/` | Complete programs. They run as tests too. |
| `docs/` | The documentation. |
| `tools/amalgamate.py` | Generates the single header. |

## Making a change

1. **Change the headers in `include/reflect/`**, never the single header.

2. **Add a test.** A test is a plain program in `tests/`. Use `static_assert`
   for everything that is known at compile time and `CHECK(...)` from
   `tests/check.hpp` for the rest. A new file has to be added to
   `tests/CMakeLists.txt`.

3. **Regenerate the single header:**

   ```sh
   python tools/amalgamate.py
   ```

   Commit the result together with the header change.
   `python tools/amalgamate.py --check` tells you whether you forgot.

4. **Update the documentation.** A new function belongs in the
   [API Reference](docs/reference.md). If it solves a task a user would ask
   about, it also belongs in the
   [Getting Started Guide](docs/getting-started.md).

## Documentation

Code in the documentation has to compile, and output has to be real.

- Snippets in the Getting Started Guide come from `tests/guide.cpp`.
- Snippets in the README come from `tests/readme.cpp`.
- The cookbook quotes the programs in `examples/`.

When you add or change a snippet, change it in the program first, run it,
and copy code and output from there.

Write for someone who has not used reflection before. Short sentences, one
idea per sentence, and an example before the explanation.

## Style

- Format code like the code around it: four spaces, `snake_case`, braces on
  the same line.
- Qualify every call into `std::meta`.
- Public names get a `///` comment that says what the function does, from
  the point of view of the caller.
- Comments explain why, not what.
- The headers have to compile without warnings under
  `-Wall -Wextra -Wpedantic`.

## Commits

One commit per change, with a subject in the form `type: summary`:

```text
feat: add static field proxy
refactor: put the access policy first in tie
docs: add troubleshooting page
test: compile the snippets of the getting started guide
build: add a generated single header
```

Use the imperative, keep the subject under about 60 characters, and add a
body when the reason for the change is not obvious from the subject.

## Design changes

For anything that changes the shape of the API, open an issue first.
[Design notes](docs/design.md) describes the rules the API follows and the
questions that are still open.
