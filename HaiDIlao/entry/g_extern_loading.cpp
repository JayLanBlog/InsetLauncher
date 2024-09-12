#include "g_extern_loading.h"
#include "hook/hooks.h"
#include "sysos/sys_os_define.h"

void Launch_And_Inject() {
	
	std::cout << "Launch_And_Inject" << std::endl;
	std::string app = "E://C-work//OPPENDEMO//bin//src//tests//skima//skima.exe";
	std::string workdir = "E://C-work//OPPENDEMO//bin//src//tests//skima";
	//Process::LaunchAndInjectIntoProcess(app, workdir);
	//Process::LanuchDefaultProcess();
	Proc::LaunchDefaultAndInject();
	Test::test();
}