# Contribution Guidelines

[English](CONTRIBUTING.md) | [日本語](CONTRIBUTING.ja.md)

This document describes recommended and prohibited practices for contributing
to NestDAQ.

## Forking workflow

- The `main` branch contains the latest released version of NestDAQ.
- The `develop` branch contains the latest development version.
- Before starting development, fork the upstream `spadi-alliance/nestdaq`
  repository to your own GitHub account.
- Synchronize your fork with the upstream `develop` branch, then create a
  working branch from `develop` in your fork.
- You may create working branches freely in your own fork. Do not create
  working branches in the upstream repository.
- The upstream `main` and `develop` branches are protected and do not accept
  direct pushes.
- Open a Pull Request or Draft Pull Request from the working branch in your
  fork to the upstream `develop` branch.
- Use Draft Pull Requests when the change is not ready for final review but
  early feedback is useful.

The upstream repository is the canonical NestDAQ repository. Your fork is your
personal GitHub copy and holds branches that you push. The local clone on your
PC is the working copy where you check out a branch, edit files, build, and run
checks.

```mermaid
flowchart LR
  subgraph Upstream["Upstream repository<br/>spadi-alliance/nestdaq"]
    UpstreamDevelop["develop branch"]
  end

  subgraph Fork["Your GitHub fork<br/>your-account/nestdaq"]
    ForkDevelop["develop branch"]
    ForkWorking["working branch"]
  end

  subgraph Local["Local PC<br/>working clone"]
    LocalClone["clone of your fork"]
    LocalWorking["checked-out working branch"]
  end

  UpstreamDevelop -->|fork or synchronize| ForkDevelop
  ForkDevelop -->|git clone| LocalClone
  LocalClone -->|git checkout -b| LocalWorking
  LocalWorking -->|git push| ForkWorking
  ForkWorking -->|Pull Request| UpstreamDevelop
```

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
