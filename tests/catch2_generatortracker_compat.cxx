/**
 * @file catch2_generatortracker_compat.cxx
 * @brief Compatibility definitions for the installed Catch2 static library.
 */

#include <catch2/generators/catch_generators.hpp>
#include <catch2/interfaces/catch_interfaces_generatortracker.hpp>

namespace Catch::Generators {

void GeneratorUntypedBase::skipToNthElementImpl(std::size_t n)
{
    for (auto i = m_currentElementIndex; i < n; ++i) {
        if (!next()) {
            Detail::throw_generator_exception("Could not jump to Nth element: not enough elements");
        }
    }
}

void GeneratorUntypedBase::skipToNthElement(std::size_t n)
{
    if (n < m_currentElementIndex) {
        Detail::throw_generator_exception("Tried to jump generator backwards");
    }
    if (n == m_currentElementIndex) {
        return;
    }

    skipToNthElementImpl(n);
    m_currentElementIndex = n;
    m_stringReprCache.clear();
}

auto GeneratorUntypedBase::isFinite() const -> bool
{
    return true;
}

} // namespace Catch::Generators
