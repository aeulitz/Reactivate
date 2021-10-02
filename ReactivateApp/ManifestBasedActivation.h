#pragma once

#include <Windows.h>
#include <roapi.h>
#include <VersionHelpers.h>
#include <xmllite.h>

#define WIN1019H1_BLDNUM 18362

// REVIEW: move into ManifestBasedActivation class?
VERSIONHELPERAPI IsWindowsVersionOrGreaterEx(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor, WORD wBuildNumber)
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

// REVIEW: move into ManifestBasedActivation class?
inline bool IsWindows1019H1OrGreater()
{
	return IsWindowsVersionOrGreaterEx(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0, WIN1019H1_BLDNUM);
}

class ManifestBasedActivation
{
public:
	static std::pair<HRESULT, ManifestBasedActivation> Initialize() noexcept;
	~ManifestBasedActivation() noexcept;

private:
	ManifestBasedActivation() noexcept;

	// REVIEW: If by the time we get this working we don't need any info other than the module name,
	// get rid of this struct.
	struct ComponentInfo
	{
		std::wstring ModuleName;
	};

	using CatalogType = std::unordered_map<std::wstring /* activation ID */, ComponentInfo>;

	static HRESULT LoadCatalog() noexcept;

	static HRESULT ParseFileElements(IXmlReader& reader) noexcept;
	static HRESULT ParseFileElement(IXmlReader& reader) noexcept;
	static HRESULT ParseActivatableClassElement(IXmlReader& reader, const wchar_t** name) noexcept;

	static HRESULT InitializeDetours() noexcept;
	static HRESULT UninitializeDetours() noexcept;

	static HRESULT DetouredRoActivateInstance(HSTRING activatableClassId, ::IInspectable** instance) noexcept;
	static HRESULT DetouredRoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory) noexcept;

	static int g_refCount;
	static std::optional<bool> g_osSupportsManifestBasedActivation;


	static bool g_detoursActive;
	static decltype(RoActivateInstance)* g_originalRoActivateInstance;
	static decltype(RoGetActivationFactory)* g_originalRoGetActivationFactory;

	static std::unique_ptr<CatalogType> g_catalog;
};

