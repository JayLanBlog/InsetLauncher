#include "string_util.h"
#include <cwctype> 
#include <algorithm>

const WCHAR* charToWCHAR(const char* s) {
	int w_nlen = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
	WCHAR* ret;
	ret = (WCHAR*)malloc(sizeof(WCHAR) * w_nlen);
	memset(ret, 0, sizeof(ret));
	MultiByteToWideChar(CP_ACP, 0, s, -1, ret, w_nlen);
	return ret;
}
std::string str_to_lower(const std::string& str)
{
	std::string newstr(str);
	for (size_t i = 0; i < newstr.size(); i++)
		newstr[i] = (char)tolower(newstr[i]);
	return newstr;
}

std::wstring to_lower(const std::wstring& str) {
	std::wstring lower_str;
	lower_str.resize(str.size());
	std::transform(str.begin(), str.end(), lower_str.begin(),
		[](wchar_t c) { return std::tolower(c); });
	return lower_str;
}