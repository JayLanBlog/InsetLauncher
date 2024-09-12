
#include "gl_common.h"
#include "gl_driver.h"
Threading::CriticalSection glLock;






template <>
bool CheckConstParam(bool t)
{
	return t;
}