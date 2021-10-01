#pragma once
#include "Calc.g.h"

namespace winrt::ReactivateComponent::implementation
{
    struct Calc : CalcT<Calc>
    {
        Calc() = default;

        int32_t Add(int32_t a, int32_t b);
        int32_t Mul(int32_t a, int32_t b);
    };
}
namespace winrt::ReactivateComponent::factory_implementation
{
    struct Calc : CalcT<Calc, implementation::Calc>
    {
    };
}
