#ifndef _lyramilk_lywr_defs_h_
#define _lyramilk_lywr_defs_h_

#ifndef LYWR_NAMESPACE_START
	#define LYWR_NAMESPACE_START
#endif

#ifndef LYWR_NAMESPACE_END
	#define LYWR_NAMESPACE_END
#endif


#ifdef __GNUC__
	#define LYWR_NOEXPORT __attribute__ ((visibility ("hidden")))
	#define LYWR_EXPORT __attribute__ ((visibility ("default")))
#else
	#define LYWR_NOEXPORT 
	#define LYWR_EXPORT 
#endif

LYWR_NAMESPACE_START
#ifdef _DEBUG
		#include <time.h>
		#include <stdio.h>
		#define TRACE(...) lywr_trace_logprefix(__FILE__,__LINE__),printf(__VA_ARGS__)
		void LYWR_EXPORT lywr_trace_logprefix(const char* file,unsigned int lineo);
#else
		#define TRACE(...)
#endif
LYWR_NAMESPACE_END


#endif
