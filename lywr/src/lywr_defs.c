#include "lywr_defs.h"
LYWR_NAMESPACE_START

#ifdef _DEBUG
void lywr_trace_logprefix(const char* file,unsigned int lineo)
{
	time_t ti = time(NULL);
	struct tm __t;
	struct tm *t = localtime_r(&ti,&__t);
	if (t == NULL){
		return;
	}
	printf("\x1b[33m%04d-%02d-%02d %02d:%02d:%02d %s:%d\x1b[0m==>",1900 + __t.tm_year,1+__t.tm_mon,__t.tm_mday,__t.tm_hour,__t.tm_min,__t.tm_sec,file,lineo);
}
#else
void lywr_trace_logprefix(const char* file,unsigned int lineo)
{
	
}
#endif



LYWR_NAMESPACE_END
