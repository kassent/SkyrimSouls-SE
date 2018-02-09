#include "SigScan.h"


sig_plugin_info plugin_info{ 0, nullptr };

char const* no_result_exception::what() const
{
	return "failed to find memory signatures...";
}