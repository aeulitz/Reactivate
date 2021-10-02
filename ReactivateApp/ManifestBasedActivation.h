#pragma once

#include <Windows.h>
#include <roapi.h>
#include <VersionHelpers.h>
#include <xmllite.h>

#include <functional>
#include <map>

// This is what the repo is meant to demonstrate. This class is intended to serve
// the following purposes:
// 1. It is a container for the code needed to redirect WinRT component activation
//    to control it via data in the executable's app manifest. In that capacity,
//    it serves the same purpose as undocked reg-free WinRT.
// 2. Unlike undocked reg-free WinRT which effects redirection of component activation
//    at app boot, this class is meant to enable redirection on demand.
// 3. Instances of this class are meant to be lightweight tokens to control the
//    lifetime of the redirection. This is meant to facilitate solutions where several
//    "entry points" require redirection.
class ManifestBasedActivation
{
private:
	ManifestBasedActivation() noexcept;

public:
	ManifestBasedActivation(const ManifestBasedActivation&) = delete;
	ManifestBasedActivation(ManifestBasedActivation&&) noexcept;
	ManifestBasedActivation& operator=(const ManifestBasedActivation&) = delete;
	ManifestBasedActivation& operator=(ManifestBasedActivation&&) = delete;

	~ManifestBasedActivation() noexcept;

	static std::pair<HRESULT, ManifestBasedActivation> Initialize() noexcept;

private:

#ifdef TESTBUILD
	friend class ManifestBasedActivationTests;
#endif

	// REVIEW: If by the time we get this working we don't need any info other than the module name,
	// get rid of this struct.
	struct ComponentInfo
	{
		std::wstring ModuleName;
	};

	using CatalogType = std::unordered_map<std::wstring /* activation ID */, ComponentInfo>;

	static std::function<bool()> IsManifestBasedActivationRequired;

	static HRESULT Uninitialize() noexcept;

	static HRESULT LoadCatalog() noexcept;

	static HRESULT ParseFileElements(IXmlReader& reader) noexcept;
	static HRESULT ParseFileElement(IXmlReader& reader) noexcept;
	static HRESULT ParseActivatableClassElement(IXmlReader& reader, const wchar_t** name) noexcept;

	static HRESULT InitializeDetours() noexcept;
	static HRESULT UninitializeDetours() noexcept;

	static HRESULT DetouredRoActivateInstance(HSTRING activatableClassId, ::IInspectable** instance) noexcept;
	static HRESULT DetouredRoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory) noexcept;

	bool m_active;
	static int g_refCount;
	static std::unique_ptr<CatalogType> g_catalog;
	static bool g_detoursActive;
	static decltype(RoActivateInstance)* g_originalRoActivateInstance;
	static decltype(RoGetActivationFactory)* g_originalRoGetActivationFactory;
};

// ensure ManifestBasedActivation instances can be easily passed around
static_assert(sizeof(ManifestBasedActivation) <= sizeof(bool));
