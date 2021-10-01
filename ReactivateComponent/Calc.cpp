#include "pch.h"
#include "Calc.h"
#include "Calc.g.cpp"

namespace winrt::ReactivateComponent::implementation
{
    int32_t Calc::Add(int32_t a, int32_t b)
    {
        return a + b;
    }
    int32_t Calc::Mul(int32_t a, int32_t b)
    {
        return a * b;
    }
}
