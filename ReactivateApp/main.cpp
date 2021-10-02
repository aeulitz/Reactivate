#include "pch.h"

#include "ManifestBasedActivation.h"
#include <winrt/ReactivateComponent.h>

#include <winstring.h>
#include <wil/com.h>
#include <iostream>

using namespace winrt;
using namespace winrt::ReactivateComponent;
using namespace winrt::Windows::Foundation;

#ifdef TESTBUILD

// Poor man's test class. This uses the 'assert' function to check test conditions and will thus
// only work in debug builds.
class ManifestBasedActivationTests
{
	static std::function<bool()> g_originalIsManifestBasedActivationRequired;
	static ManifestBasedActivation* g_heapActivation;

public:
	static void TestClassSetup()
	{
		g_originalIsManifestBasedActivationRequired = ManifestBasedActivation::IsManifestBasedActivationRequired;

		// mock this method to pretend we run on an OS version that doesn't support manifest-based activation
		ManifestBasedActivation::IsManifestBasedActivationRequired = []() { return true; };
	}

	static void TestClassTeardown()
	{
		ManifestBasedActivation::IsManifestBasedActivationRequired = g_originalIsManifestBasedActivationRequired;
	}

	static void TestSingleActivation()
	{
		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		// activation scope
		{
			auto [result, activation] = ManifestBasedActivation::Initialize();

			assert(result == S_OK);
			assert(ManifestBasedActivation::g_refCount == 1);
			assert(ManifestBasedActivation::g_detoursActive);

			Calc calc;
			assert(calc.Add(1, 2) == 3);
		}

		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		std::wcout << "TestSingleActivation passed" << std::endl;
	}

	static void TestDoubleActivation()
	{
		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		// activation scope
		{
			auto [result1, activation1] = ManifestBasedActivation::Initialize();
			auto [result2, activation2] = ManifestBasedActivation::Initialize();

			assert(result1 == S_OK);
			assert(result2 == S_OK);
			assert(ManifestBasedActivation::g_refCount == 2);
			assert(ManifestBasedActivation::g_detoursActive);

			Calc calc;
			assert(calc.Add(1, 2) == 3);
		}

		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		std::wcout << "TestDoubleActivation passed" << std::endl;
	}

	static void TestNestedActivation()
	{
		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		// outer activation scope
		{
			auto [outerResult, outerActivation] = ManifestBasedActivation::Initialize();
			assert(outerResult == S_OK);
			assert(ManifestBasedActivation::g_refCount == 1);
			assert(ManifestBasedActivation::g_detoursActive);

			// inner activation scope
			{
				auto [innerResult, innerActivation] = ManifestBasedActivation::Initialize();

				assert(innerResult == S_OK);
				assert(ManifestBasedActivation::g_refCount == 2);
				assert(ManifestBasedActivation::g_detoursActive);

				Calc calc;
				assert(calc.Add(1, 2) == 3);
			}

			assert(ManifestBasedActivation::g_refCount == 1);
			assert(ManifestBasedActivation::g_detoursActive);

			Calc calc;
			assert(calc.Add(1, 2) == 3);
		}

		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		std::wcout << "TestNestedActivation passed" << std::endl;
	}

	static void TestHeapOwnedActivation_1()
	{
		auto [result, activation] = ManifestBasedActivation::Initialize();
		g_heapActivation = new ManifestBasedActivation(std::move(activation));

		assert(result == S_OK);
		assert(ManifestBasedActivation::g_refCount == 1);
		assert(ManifestBasedActivation::g_detoursActive);
	}

	static void TestHeapOwnedActivation_2()
	{
		assert(ManifestBasedActivation::g_refCount == 1);
		assert(ManifestBasedActivation::g_detoursActive);

		Calc calc;
		assert(calc.Add(1, 2) == 3);

		delete g_heapActivation;
	}

	static void TestHeapOwnedActivation()
	{
		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		TestHeapOwnedActivation_1();
		TestHeapOwnedActivation_2();

		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		std::wcout << "TestHeapOwnedActivation passed" << std::endl;
	}

	static void TestActivationTransfer()
	{
		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		// outer activation scope
		{
			auto [outerResult, outerActivation] = ManifestBasedActivation::Initialize();
			assert(outerResult == S_OK);
			assert(ManifestBasedActivation::g_refCount == 1);
			assert(ManifestBasedActivation::g_detoursActive);

			// inner activation scope
			{
				ManifestBasedActivation innerActivation = std::move(outerActivation);

				assert(ManifestBasedActivation::g_refCount == 1);
				assert(ManifestBasedActivation::g_detoursActive);

				Calc calc;
				assert(calc.Add(1, 2) == 3);
			}

			assert(ManifestBasedActivation::g_refCount == 0);
			assert(!ManifestBasedActivation::g_detoursActive);
		}

		assert(ManifestBasedActivation::g_refCount == 0);
		assert(!ManifestBasedActivation::g_detoursActive);

		std::wcout << "TestActivationTransfer passed" << std::endl;
	}
};

ManifestBasedActivation* ManifestBasedActivationTests::g_heapActivation = nullptr;
std::function<bool()> ManifestBasedActivationTests::g_originalIsManifestBasedActivationRequired;

#endif

int main()
{
	init_apartment();

#ifdef TESTBUILD
	ManifestBasedActivationTests::TestClassSetup();

	ManifestBasedActivationTests::TestSingleActivation();
	ManifestBasedActivationTests::TestDoubleActivation();
	ManifestBasedActivationTests::TestNestedActivation();
	ManifestBasedActivationTests::TestHeapOwnedActivation();
	ManifestBasedActivationTests::TestActivationTransfer();

	ManifestBasedActivationTests::TestClassTeardown();
#endif

	auto [hr, activation] = ManifestBasedActivation::Initialize();

	Calc calc;
	std::wcout << "Add(1, 2)=" << calc.Add(1, 2) << std::endl;
	std::wcout << "Mul(3, 4)=" << calc.Mul(3, 4) << std::endl;
}
