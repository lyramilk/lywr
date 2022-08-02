#include "lywr_stream.hh"
#include "lywr.hh"

LYWR_NAMESPACE_START


long long lywr_pread(lywr_ctx* ctx,lywr_module* mdl,long long offset,void* buf,long long bufsize)
{
	return mdl->spec.pread(ctx,buf,bufsize,offset,mdl->spec.userdata);
}

long long lywr_stream_read(lywr_ctx* ctx,lywr_module* mdl,void* buf,long long bufsize)
{
	long long r = mdl->spec.pread(ctx,buf,bufsize,mdl->offset,mdl->spec.userdata);
	if(r > 0){
		mdl->offset += r;
	}
	return r;
}

long long lywr_stream_peek(lywr_ctx* ctx,lywr_module* mdl,void* buf,long long bufsize)
{
	return mdl->spec.pread(ctx,buf,bufsize,mdl->offset,mdl->spec.userdata);
}

long long lywr_stream_seek(lywr_ctx* ctx,lywr_module* mdl,long long offset)
{
	if(offset > mdl->spec.modulesize){
		mdl->offset = mdl->spec.modulesize;
		return mdl->offset;
	}
	mdl->offset = offset;
	return mdl->offset;
}
long long lywr_stream_skip(lywr_ctx* ctx,lywr_module* mdl,long long offset)
{
	return mdl->offset += offset;
}

long long lywr_stream_tell(lywr_ctx* ctx,lywr_module* mdl)
{
	return mdl->offset;
}

long long lywr_stream_get(lywr_ctx* ctx,lywr_module* mdl,void* c)
{
	return lywr_stream_read(ctx,mdl,c,1);
}

LYWR_NAMESPACE_END
