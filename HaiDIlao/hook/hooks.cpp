#include "hooks.h"
#include <iostream>
#include "common/common.h"
namespace Test {
	void test() {
		std::cout << "test" << std::endl;
	}
}

static std::vector<LibraryHook*>& LibList()
{
	static std::vector<LibraryHook*> libs;
	return libs;
}

LibraryHook::LibraryHook()
{
	LibList().push_back(this);
}

void LibraryHooks::RegisterHooks()
{
	BeginHookRegistration();

	for (LibraryHook* lib : LibList())
		lib->RegisterHooks();

	EndHookRegistration();
}
