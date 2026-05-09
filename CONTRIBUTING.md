# Contributing

## Code style and naming

## C++

### Code

- class names: UpperCamelCase
- member functions: UpperCamelCase
- members:  fUpperCamelCase
- local variables: snake_case
- constants, definitions: SCREAMING_SNAKE_CASE
- base namespace: nestdaq
- indent: 4 spaces
- function declarations and definitions: shoud be preferebly be ordered in lexicographical order.
- Use trailing return types for functions: `auto name(args) -> ReturnType`.


### File naming

- file extensions: cxx, h
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

    static const int MAX_VALUE = 42;
    static const int MIN_VALUE = 0;

    virtual void method1() = 0;
    virtual void method2() = 0;

    int  x() { return fX; }
    void setX(x) { fX = x; }

private:
    int fX;

};

```
