#include "pch.h"

#include "RNActivation.h"
#include <winrt/ReactivateComponent.h>

#include <winstring.h>
#include <wil/com.h>
#include <iostream>

using namespace winrt;
using namespace winrt::ReactivateComponent;
using namespace winrt::Windows::Foundation;

int main()
{
	init_apartment();
	Uri uri(L"http://aka.ms/cppwinrt");
	printf("Hello, %ls!\n", uri.AbsoluteUri().c_str());


	RNActivation::ManifestBasedActivation activation;

	//HSTRING_HEADER hstringHeader;
	//HSTRING activationId;
	//HRESULT hr = WindowsCreateStringReference(
	//	L"ReactivateComponent.Calc",
	//	static_cast<UINT32>(std::size(L"ReactivateComponent.Calc") - 1),
	//	&hstringHeader,
	//	&activationId
	//);
	//wil::com_ptr<::IInspectable> comp;
	//hr = RoActivateInstance(activationId, comp.addressof());

	Calc calc;
	std::wcout << "Add(1, 2)=" << calc.Add(1, 2) << std::endl;
	std::wcout << "Mul(3, 4)=" << calc.Mul(3, 4) << std::endl;
}
