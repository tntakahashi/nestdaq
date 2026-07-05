# Contribution Guidelines

This document describes recommended and prohibited practices for contributing
to NestDAQ.

## Forking workflow

- Do not push directly to the upstream NestDAQ repository.
- Push changes to your own fork.
- Open a Pull Request or Draft Pull Request from your fork to the upstream
  repository.
- Use Draft Pull Requests when the change is not ready for final review but
  early feedback is useful.

## Commits and Pull Requests

- Avoid combining several unrelated changes into one large commit.
- Split commits by intent when separate changes can be reviewed independently.
- Keep Pull Requests small enough to review carefully.
- Prefer opening Pull Requests frequently instead of waiting until many unrelated
  changes have accumulated.

## Formatting

- Apply a formatter before opening a Pull Request or moving a Draft Pull
  Request to ready-for-review.
- For C/C++ files, apply `astyle`.
- Format only files touched by your change.
- Do not reformat unrelated files.

## Static analysis

- Run `clang-tidy` before opening a Pull Request.
- Use the repository-local `.clang-tidy` configuration.
- Do not enable extra checks for project code unless the Pull Request is about
  clang-tidy policy itself.
- To run `clang-tidy` through CMake, configure with
  `-DNESTDAQ_ENABLE_CLANG_TIDY=ON`.

## Code style and naming

### C++

- Indent with 4 spaces.

### Naming

- `PascalCase` and `UpperCamelCase` mean the same naming style.
- Class and type names: `PascalCase` / `UpperCamelCase`.
- Namespaces: `snake_case`.
- Functions and member functions: prefer lower camel case / `camelCase`;
  `PascalCase` / `UpperCamelCase` is allowed for existing-style consistency.
- Variables: prefer `snake_case`; lower camel case / `camelCase` is allowed for
  existing-style consistency.
- `using` alias names are outside the naming rule scope. They may follow local
  readability, external library conventions, or common short forms.
- Public `struct` data fields: `snake_case`.
- Private and protected class data members: `fPascalCase`.
- Static data members: start with `fg`, for example `fgPascalCase`.
- Static variables: start with `g`, for example `gPascalCase`.
- Constants: start with `k`, for example `kPascalCase`, or use
  `SCREAMING_SNAKE_CASE`.
- Macro names: `SCREAMING_SNAKE_CASE`.
- Enum constants: `kPascalCase`, `PascalCase` / `UpperCamelCase`, or
  `SCREAMING_SNAKE_CASE`.
- Base namespace for NestDAQ code: `nestdaq`.

### File naming

- Preferred file extensions: `.cpp` and `.hpp`.
- Allowed file extensions: `.cxx`, `.h`, `.hh`, and `.hxx`.
