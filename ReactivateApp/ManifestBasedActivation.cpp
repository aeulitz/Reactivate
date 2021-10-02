#include "pch.h"

#include "ManifestBasedActivation.h"

#include <wil/com.h>
#include <detours.h>

#include <shlwapi.h>
#include <winstring.h>

typedef HRESULT(__stdcall* ActivationFactoryGetter)(HSTRING, IActivationFactory**);

constexpr int WIN1019H1_BLDNUM = 18362;

inline bool IsWindowsVersionOrGreaterEx(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor, WORD wBuildNumber)
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi) };
	DWORDLONG const dwlConditionMask =
		VerSetConditionMask(
			VerSetConditionMask(
				VerSetConditionMask(
					VerSetConditionMask(
						0, VER_MAJORVERSION, VER_GREATER_EQUAL),
					VER_MINORVERSION, VER_GREATER_EQUAL),
				VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL),
			VER_BUILDNUMBER, VER_GREATER_EQUAL);

	osvi.dwMajorVersion = wMajorVersion;
	osvi.dwMinorVersion = wMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;
	osvi.dwBuildNumber = wBuildNumber;

	return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_BUILDNUMBER, dwlConditionMask) != FALSE;
}

inline bool IsWindows1019H1OrGreater()
{
	return IsWindowsVersionOrGreaterEx(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0, WIN1019H1_BLDNUM);
}

ManifestBasedActivation::ManifestBasedActivation() noexcept
	: m_active{ true }
{
	++g_refCount;
}

ManifestBasedActivation::ManifestBasedActivation(ManifestBasedActivation&& other) noexcept
	: m_active{ other.m_active }
{
	other.m_active = false;
}

ManifestBasedActivation::~ManifestBasedActivation() noexcept
{
	if (m_active) --g_refCount;
	Uninitialize();
}

std::pair<HRESULT, ManifestBasedActivation> ManifestBasedActivation::Initialize() noexcept
{
	// TODO: sequentialize access

	HRESULT hr = S_OK;
	ManifestBasedActivation activation{};

	if (!IsManifestBasedActivationRequired())
	{
		// nothing to do
		return std::make_pair(S_OK, std::move(activation));
	}

	if (g_refCount > 1)
	{
		// other scopes are already active
		return std::make_pair(S_OK, std::move(activation));
	}

	if (FAILED(hr = LoadCatalog()))
	{
		return std::make_pair(hr, std::move(activation));
	}

	if (FAILED(hr = InitializeDetours()))
	{
		return std::make_pair(hr, std::move(activation));
	}

	return std::make_pair(S_OK, std::move(activation));
}

HRESULT ManifestBasedActivation::Uninitialize() noexcept
{
	if (!IsManifestBasedActivationRequired())
	{
		// nothing to do
		return S_OK;
	}

	if (g_refCount > 0)
	{
		// other scopes still active
		return S_OK;
	}

	// REVIEW: We could call FreeLibrary here and destroy g_catalog.

	return UninitializeDetours();
}

HRESULT ManifestBasedActivation::LoadCatalog() noexcept
{
	RETURN_HR_IF(S_OK, g_catalog);

	g_catalog = std::make_unique<CatalogType>();

	HRSRC resourceHandle = FindResourceW(NULL /* executable module */, MAKEINTRESOURCEW(1), RT_MANIFEST);
	if (!resourceHandle)
	{
		resourceHandle = FindResourceW(NULL /* executable module */, MAKEINTRESOURCEW(2), RT_MANIFEST);
		RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !resourceHandle);
	}

	HGLOBAL embeddedManifest = LoadResource(NULL /* executable module */, resourceHandle);
	RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !embeddedManifest);

	DWORD length = SizeofResource(NULL /* executable module */, resourceHandle);
	RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), length == 0);

	void* manifestRawData = LockResource(embeddedManifest);
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !manifestRawData);

	wil::com_ptr<IStream> manifestStream = SHCreateMemStream(static_cast<BYTE*>(manifestRawData), length);
	RETURN_HR_IF_NULL(E_OUTOFMEMORY, manifestStream);

	wil::com_ptr<IXmlReaderInput> manifestReaderInput;
	RETURN_IF_FAILED(CreateXmlReaderInputWithEncodingName(manifestStream.get(), nullptr, L"utf-8", FALSE, nullptr, manifestReaderInput.addressof()));

	wil::com_ptr<IXmlReader> manifestReader;
	RETURN_IF_FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(manifestReader.addressof()), nullptr));

	RETURN_IF_FAILED(manifestReader->SetInput(manifestReaderInput.get()));

	return ParseFileElements(*manifestReader.get());
}

HRESULT ManifestBasedActivation::ParseFileElements(IXmlReader& reader) noexcept
{
	// The code below is intended to skip over all elements except <file/> elements.
	while (reader.Read(nullptr) == S_OK)
	{
		RETURN_IF_FAILED(ParseFileElement(reader));
	}

	return S_OK;
}

HRESULT ManifestBasedActivation::ParseFileElement(IXmlReader& reader) noexcept
{
	XmlNodeType nodeType;

	RETURN_IF_FAILED(reader.GetNodeType(&nodeType));
	if (nodeType != XmlNodeType_Element)
	{
		return S_FALSE;
	}

	PCWSTR localName = nullptr;
	RETURN_IF_FAILED((reader.GetLocalName(&localName, nullptr)));
	if (localName == nullptr || _wcsicmp(localName, L"file") != 0)
	{
		return S_FALSE;
	}

	HRESULT hr = reader.MoveToAttributeByName(L"name", nullptr);
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_SXS_MANIFEST_PARSE_ERROR), hr != S_OK);

	PCWSTR fileName = nullptr;
	RETURN_IF_FAILED(reader.GetValue(&fileName, nullptr));
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_SXS_MANIFEST_PARSE_ERROR), fileName == nullptr || !fileName[0]);

	while (reader.Read(&nodeType) == S_OK && nodeType != XmlNodeType_EndElement)
	{
		// ignore other elements, but bail if an <activatableClass/> element is incorrect

		const wchar_t* className;
		RETURN_IF_FAILED(hr = ParseActivatableClassElement(reader, &className));
		if (hr == S_OK)
		{
			g_catalog->emplace(className, ComponentInfo{ fileName });
		}

	}
	return S_OK;
}

HRESULT ManifestBasedActivation::ParseActivatableClassElement(IXmlReader& reader, const wchar_t** className) noexcept
{
	RETURN_HR_IF(E_INVALIDARG, className == nullptr);
	*className = nullptr;

	XmlNodeType nodeType;
	RETURN_IF_FAILED(reader.GetNodeType(&nodeType));
	if (nodeType != XmlNodeType_Element)
	{
		return S_FALSE;
	}

	PCWSTR localName = nullptr;
	RETURN_IF_FAILED(reader.GetLocalName(&localName, nullptr));
	if (localName == nullptr || _wcsicmp(localName, L"activatableClass") != 0)
	{
		return S_FALSE;
	}

	// Using this pattern intead of calling multiple MoveToAttributeByName improves performance
	for (HRESULT hr = reader.MoveToFirstAttribute();
		hr == S_OK;
		hr = reader.MoveToNextAttribute())
	{
		RETURN_IF_FAILED(reader.GetLocalName(&localName, nullptr));

		const wchar_t* attributeValue;
		RETURN_IF_FAILED(reader.GetValue(&attributeValue, NULL));

		if (localName != nullptr && _wcsicmp(L"name", localName) == 0)
		{
			*className = attributeValue;
		}
		else if (localName != nullptr && _wcsicmp(L"threadingModel", localName) == 0)
		{
			// We currently only handle thread-agile components
			if (attributeValue == nullptr || _wcsicmp(L"both", attributeValue) != 0)
			{
				return E_FAIL;
			}
		}
		// ignore other attributes
	}

	if (*className == nullptr || (*className)[0] == L'\0')
	{
		return HRESULT_FROM_WIN32(ERROR_SXS_MANIFEST_PARSE_ERROR);
	}

	return S_OK;
}

HRESULT ManifestBasedActivation::InitializeDetours() noexcept
{
	RETURN_HR_IF(S_OK, g_detoursActive);

	// TODO: check for errors

	// This function expects the caller to check whether detouring is necessary.
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)g_originalRoActivateInstance, DetouredRoActivateInstance);
	DetourAttach(&(PVOID&)g_originalRoGetActivationFactory, DetouredRoGetActivationFactory);
	DetourTransactionCommit();

	g_detoursActive = true;
	return S_OK;
}

HRESULT ManifestBasedActivation::UninitializeDetours() noexcept
{
	RETURN_HR_IF(S_OK, !g_detoursActive);
	// TODO: check for errors

	// This function expects the caller to check whether detouring needs to be undone.
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)g_originalRoActivateInstance, DetouredRoActivateInstance);
	DetourDetach(&(PVOID&)g_originalRoGetActivationFactory, DetouredRoGetActivationFactory);
	DetourTransactionCommit();

	g_detoursActive = false;
	return S_OK;
}

HRESULT ManifestBasedActivation::DetouredRoActivateInstance(HSTRING activatableClassId, IInspectable** instance) noexcept
{
	wil::com_ptr<IActivationFactory> activationFactory;
	RETURN_IF_FAILED(DetouredRoGetActivationFactory(activatableClassId, __uuidof(IActivationFactory), reinterpret_cast<void**>(activationFactory.addressof())));
	return activationFactory->ActivateInstance(instance);
}

HRESULT ManifestBasedActivation::DetouredRoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory) noexcept
{
	RETURN_HR_IF(E_INVALIDARG, factory == nullptr);

	auto it = g_catalog->find(WindowsGetStringRawBuffer(activatableClassId, nullptr));
	if (it == g_catalog->cend())
	{
		// no manifest mapping for component, fall back to original RoActivateInstance
		return g_originalRoGetActivationFactory(activatableClassId, iid, factory);
	}

	// We assume that LoadLibraryEx caches.
	HMODULE moduleHandle = LoadLibraryExW(it->second.ModuleName.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (moduleHandle == nullptr)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// REVIEW: Do we need to organize calls to FreeLibrary(moduleHandle)?

	ActivationFactoryGetter activationFactoryGetter = (ActivationFactoryGetter)GetProcAddress(moduleHandle, "DllGetActivationFactory");
	if (activationFactoryGetter == nullptr)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	wil::com_ptr<IActivationFactory> activationFactory;
	RETURN_IF_FAILED(activationFactoryGetter(activatableClassId, activationFactory.addressof()));
	RETURN_HR_IF(E_FAIL, activationFactory);

	if (iid == __uuidof(IActivationFactory))
	{
		*factory = activationFactory.detach();
		return S_OK;
	}
	else
	{
		return activationFactory->QueryInterface(iid, factory);
	}
}

std::function<bool()> ManifestBasedActivation::IsManifestBasedActivationRequired = []()
{
	static std::optional<bool> g_isManifestBasedActivationRequired;

	if (!g_isManifestBasedActivationRequired.has_value())
	{
		g_isManifestBasedActivationRequired = IsWindows1019H1OrGreater();
	}

	return g_isManifestBasedActivationRequired.value();
};

std::unique_ptr<ManifestBasedActivation::CatalogType> ManifestBasedActivation::g_catalog;

bool ManifestBasedActivation::g_detoursActive = false;
decltype(RoActivateInstance)* ManifestBasedActivation::g_originalRoActivateInstance = RoActivateInstance;
decltype(RoGetActivationFactory)* ManifestBasedActivation::g_originalRoGetActivationFactory = RoGetActivationFactory;

int ManifestBasedActivation::g_refCount = 0;


