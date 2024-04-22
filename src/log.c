#include <stdio.h>
#include "log.h"

int g_log_settings = LOG_CRITICAL | LOG_ERROR | LOG_WARNING | LOG_INFO;
FILE *g_log_stream = stdout;

void Log_setmask(int mask)
{
	g_log_settings = mask;
}

int Log_getmask(void)
{
	return g_log_settings;
}
