# Contributing

## Code style and naming

## C++

### Code

- `PascalCase` and `UpperCamelCase` mean the same naming style.
- class and type names: `PascalCase` / `UpperCamelCase`
- namespaces: `snake_case`
- functions and member functions: prefer lower camel case / `camelCase`; `PascalCase` / `UpperCamelCase` is also allowed.
- variables: prefer snake case / `snake_case`; lower camel case / `camelCase` is also allowed.
- data members: `fPascalCase`
- static variables: start with `g`, for example `gPascalCase`
- constants: start with `k`, for example `kPascalCase`, or use `SCREAMING_SNAKE_CASE`
- macro names: `SCREAMING_SNAKE_CASE`
- enum constants: `kPascalCase`, `PascalCase` / `UpperCamelCase`, or `SCREAMING_SNAKE_CASE`
- base namespace: nestdaq
- indent: 4 spaces
- function declarations and definitions: shoud be preferebly be ordered in lexicographical order.
- Use trailing return types for functions: `auto name(args) -> ReturnType`.


### File naming

- file extensions: prefer `.cpp` and `.hpp`; `.cxx`, `.h`, `.hh`, and `.hxx` are also allowed.
- templated classes: end with "T" (classes that inherit from them don't, unless they are also a template)
- interface classes: start with "I"

### Example

(This code sample is temporary and needs further development)

```c++
#include <vector>

/** Doxygen style for Foo */
class Foo {
    friend class AnotherClass;

public:
    Foo(some_arg) {
        fField = some_arg;
    }

private:
    int fField;
};

class AnotherClass {
public:

    static const int kMaxValue = 42;
    static const int kMinValue = 0;

    virtual void method1() = 0;
    virtual void method2() = 0;

    int  x() { return fX; }
    void setX(x) { fX = x; }

private:
    int fX;

};

```
