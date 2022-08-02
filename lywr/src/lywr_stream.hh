#ifndef _lyramilk_lywr_stream_hh_
#define _lyramilk_lywr_stream_hh_
#include "lywr_defs.h"
#include "lywr.h"
	
LYWR_NAMESPACE_START

long long lywr_pread(lywr_ctx* ctx,lywr_module* mdl,long long offset,void* buf,long long bufsize);
long long lywr_stream_read(lywr_ctx* ctx,lywr_module* mdl,void* buf,long long bufsize);
long long lywr_stream_peek(lywr_ctx* ctx,lywr_module* mdl,void* buf,long long bufsize);
long long lywr_stream_seek(lywr_ctx* ctx,lywr_module* mdl,long long offset);
long long lywr_stream_skip(lywr_ctx* ctx,lywr_module* mdl,long long offset);
long long lywr_stream_tell(lywr_ctx* ctx,lywr_module* mdl);
long long lywr_stream_get(lywr_ctx* ctx,lywr_module* mdl,void* c);

LYWR_NAMESPACE_END

#endif
