#ifndef _lyramilk_lywr_util_hh_
#define _lyramilk_lywr_util_hh_
#include "lywr_defs.h"

LYWR_NAMESPACE_START

int lywr_memcmp2(const unsigned char* a,unsigned long long an,const unsigned char* b,unsigned long long bn);
int lywr_memcpy(unsigned char* a,const unsigned char* b,unsigned long long count);
unsigned char* lywr_memset(unsigned char* a,unsigned char b,unsigned long long count);

LYWR_NAMESPACE_END

#endif
