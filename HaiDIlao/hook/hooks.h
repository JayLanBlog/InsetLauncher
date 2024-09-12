#pragma once
#include "common/common.h"
#include "sysos/sys_os_define.h"

typedef std::function<void(void*)> FunctionCallback;

struct FuncitonEntry {
	FuncitonEntry() : orig(NULL), hook(NULL) {}
	FuncitonEntry(const char* f, void** o, void* d) : function(f), orig(o), hook(d) {}
	bool operator<(const FuncitonEntry& h) const { return function < h.function; }
	std::string function;
	void** orig;
	void* hook;
};


struct LibraryHook;


class LibraryHooks {
public:
	static void RegisterHooks();


    static void RegisterHookedLibrary(const char* libraryName, FunctionCallback loadedCallback);


	static void RegisterHookedLibraryFunction(const char* libraryName, const FuncitonEntry& hook);

	static bool Detect(const char* identifier);

private:
	static void BeginHookRegistration();
	static void EndHookRegistration();
};


struct LibraryHook
{
	LibraryHook();
	virtual void RegisterHooks() = 0;
	virtual void OptionsUpdated() {}
	virtual void RemoveHooks() {}
private:
	friend class LibraryHooks;

	static std::vector<LibraryHook*> m_Libraries;
};

namespace Test {

	void test();

};