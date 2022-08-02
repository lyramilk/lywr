#include <stdio.h>
#include "lywr.h"
#include <sys/file.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdlib.h>




#include <time.h>
#include <sys/sysinfo.h>
long long get_timespec(){
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		long long s = ts.tv_sec;
		long long n = ts.tv_nsec;
		//return (s * 1000000000 + n) / 1000;
		return s * 1000000000 + n;
}




#define LYWRRC_VERIFY(x) {lywrrc rc = x;if(rc != lywrrc_ok && rc != lywrrc_not_found) {TRACE("出错了，错误码是:%d\n",rc);return -1;}}

struct testbc_filerange
{
	char* pbase;
	char* peof;
};

int lywr_reader_pread_impl(struct lywr_ctx* ctx,char* buf,int bufsize,unsigned long long offset,struct testbc_filerange* f)
{
	long long max = (long long)(f->peof - (f->pbase + offset));

	if(max < 0){
		return -1;
	}

	if(bufsize < max){
		memcpy(buf,f->pbase + offset,bufsize);
		return bufsize;
	}
	memcpy(buf,f->pbase + offset,max);
	return max;
}

struct myblock
{
	struct myblock* next;
};


struct mymempool
{
	struct myblock* next;
};


int malloc_times =0;
int free_times =0;

void* lywr_malloc_impl(long long buffsize,void* uptr)
{
	/*
	int i = rand();
	int q = i % 1000;
	if(q < 20){
		return NULL;
	}*/
	++malloc_times;
	return malloc(buffsize);
}

void lywr_free_impl(void* buff,void* uptr)
{
	++free_times;
	free(buff);
}

lywrrc lywr_reader_open_impl(const char* filename,struct lywr_module_spec* f)
{
	struct testbc_filerange* fx = malloc(sizeof(struct testbc_filerange));
	if(fx == NULL) return lywrrc_oom;
	f->userdata = fx;

	struct stat st;
	if(stat(filename,&st) == -1){
		TRACE("获取文件信息失败\n");
		free(fx);
		return lywrrc_io_error;
	}
	fx->pbase = malloc(st.st_size);
	if(fx->pbase == NULL){
		TRACE("内存溢出\n");
		free(fx);
		return lywrrc_oom;
	}

	f->modulesize = st.st_size;

	int fd = open(filename,O_RDONLY| O_CLOEXEC,0644);
	if(fd == -1){
		TRACE("打开文件失败\n");
		free(fx->pbase);
		free(fx);
		return lywrrc_io_error;
	}
	if(read(fd,fx->pbase,st.st_size) != st.st_size){
		TRACE("读取数据失败\n");
		close(fd);
		free(fx->pbase);
		free(fx);
		return lywrrc_io_error;
	}
	close(fd);
	fx->peof = fx->pbase + st.st_size;

	f->pread = (lywr_reader_pread_cbk)lywr_reader_pread_impl;
	f->lazyload = lywr_module_spec_load_lazy;

	return lywrrc_ok;
}

lywrrc lywr_reader_close_impl(struct lywr_module_spec* f)
{
	struct testbc_filerange* fx = (struct testbc_filerange*)f->userdata;

	free(fx->pbase);
	free(fx);
	return lywrrc_ok;
}




lywrrc call(struct lywr_ctx* ctx,const char* mod,int modlen,const char* func,int funlen)
{
	lywr_function_spec spec;
	spec.mod = mod;
	spec.modlen = modlen;
	spec.field = func;
	spec.fieldlen = funlen;

	LYWRRC_VERIFY(lywr_function_init_spec(ctx,&spec);
	LYWRRC_VERIFY(lywr_function_exec(ctx,&spec));
	return lywr_function_destory_spec(ctx,&spec));
}




lywrrc call_fib(struct lywr_ctx* ctx,const char* mod,int modlen,const char* func,int funlen,int n)
{
	lywr_function_spec spec;
	spec.mod = mod;
	spec.modlen = modlen;
	spec.field = func;
	spec.fieldlen = funlen;

	LYWRRC_VERIFY(lywr_function_init_spec(ctx,&spec);

	TRACE("参数:%d\n",spec.argc);
	if(spec.argc > 0){
		spec.argv[0].u64 = n;
	}
	if(spec.argc > 1){
		//spec.argv[1].u64 = 0;
	}

	printf("即将调用fib(%llu)\n",spec.argv[0].u64);
	LYWRRC_VERIFY(lywr_function_exec(ctx,&spec));


	if(spec.rval[0].type == lywr_data_i64){
		printf("fib(%llu)=%llu,转换成16进制是%llx\n",spec.argv[0].i64,spec.rval[0].i64,spec.rval[0].i64);
	}

	return lywr_function_destory_spec(ctx,&spec));
}




lywrrc wasi_snapshot_preview1_clock_time_get(lywr_ctx* ctx,lywr_function_spec* spec)
{
	if(spec->rcount > 0){
		spec->rval[0].u32 = 0;
	}
	for(int i=0;i<spec->argc;++i){
		printf("参数%d %x\n",i,spec->argv[i].type);
	}


	printf("调用 clock_time_get 参数%d 返回值%d\n",spec->argc,spec->rcount);
	return lywrrc_ok;
}


lywrrc wasi_snapshot_preview1_proc_exit(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 proc_exit 参数%d 返回值%d\n",spec->argc,spec->rcount);
	exit(0);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 0;
	}
	return lywrrc_ok;
}


lywrrc wasi_snapshot_preview1_fd_write(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 fd_write 参数%d 返回值%d\n",spec->argc,spec->rcount);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 1;
	}
	return lywrrc_ok;
}






/*

lywrrc wasi_snapshot_preview1_fd_write(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 fd_write 参数%d 返回值%d\n",spec->argc,spec->rcount);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 1;
	}
	return lywrrc_ok;
}


lywrrc wasi_snapshot_preview1_fd_write(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 fd_write 参数%d 返回值%d\n",spec->argc,spec->rcount);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 1;
	}
	return lywrrc_ok;
}


lywrrc wasi_snapshot_preview1_fd_write(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 fd_write 参数%d 返回值%d\n",spec->argc,spec->rcount);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 1;
	}
	return lywrrc_ok;
}


lywrrc wasi_snapshot_preview1_fd_write(lywr_ctx* ctx,lywr_function_spec* spec)
{
	printf("调用 fd_write 参数%d 返回值%d\n",spec->argc,spec->rcount);
	if(spec->rcount > 0){
		spec->rval[0].u32 = 1;
	}
	return lywrrc_ok;
}
*/


double msec(long long f)
{
	f /= 1000;
	return (double)f / 1000;
}


#include <stdlib.h>
#include <stdio.h>
int main(int argc,const char* argv[]){
	struct lywr_ctx ctx;
	struct lywr_module_spec f;

	srand(time(0));

	long long t1 = get_timespec();

	//LYWRRC_VERIFY(lywr_reader_open_impl("../testsuite/nodebuginfo.wasm",&f));
	//LYWRRC_VERIFY(lywr_reader_open_impl("../testsuite/O2.wasm",&f));
	LYWRRC_VERIFY(lywr_reader_open_impl("../testsuite/fib.wasm",&f));
	//LYWRRC_VERIFY(lywr_reader_open_impl("../testsuite/plus.wasm",&f));
	//LYWRRC_VERIFY(lywr_reader_open_impl("../testsuite/withdebuginfo.wasm",&f));
	LYWRRC_VERIFY(lywr_init(&ctx,lywr_malloc_impl,lywr_free_impl,NULL));

	{
		lywr_module* mdl;
		LYWRRC_VERIFY(lywr_native_module_init(&ctx,"wasi_snapshot_preview1",strlen("wasi_snapshot_preview1"),&mdl));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"clock_time_get",strlen("clock_time_get"),wasi_snapshot_preview1_clock_time_get));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"proc_exit",strlen("proc_exit"),wasi_snapshot_preview1_proc_exit));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"fd_write",strlen("fd_write"),wasi_snapshot_preview1_fd_write));
	}

	{
		lywr_module* mdl;
		LYWRRC_VERIFY(lywr_native_module_init(&ctx,"env",strlen("env"),&mdl));
		/*
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"__table_base",strlen("__table_base"),wasi_snapshot_preview1_clock_time_get));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"__memory_base",strlen("__memory_base"),wasi_snapshot_preview1_proc_exit));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"__stack_pointer",strlen("__stack_pointer"),wasi_snapshot_preview1_fd_write));
		LYWRRC_VERIFY(lywr_native_module_function(&ctx,mdl,"setTempRet0",strlen("setTempRet0"),wasi_snapshot_preview1_fd_write));
		*/
	}
	long long t2 = get_timespec();
	LYWRRC_VERIFY(lywr_load_wasm_module(&ctx,&f,"*",1));

	long long t3 = get_timespec();

	if(argc > 1){
		int n = atoi(argv[1]);
		LYWRRC_VERIFY(call_fib(&ctx,"*",1,"orig$fib",strlen("orig$fib"),n));
		// LYWRRC_VERIFY(call_fib(&ctx,"*",1,"fib2",strlen("fib2"),n));
	}
	
	/*
	if(argc > 1){
		int n = atoi(argv[1]);
		LYWRRC_VERIFY(call_fib(&ctx,"*",1,"plus",strlen("plus"),n));
	}*/

	/*{
		LYWRRC_VERIFY(call(&ctx,"*",1,"_start",strlen("_start")));
	}*/

	long long t4 = get_timespec();

	LYWRRC_VERIFY(lywr_destory(&ctx));
	LYWRRC_VERIFY(lywr_reader_close_impl(&f));
	long long t5 = get_timespec();

	printf("总耗时    :% 5.3f毫秒\n",msec(t5 - t1));
	printf("初始化耗时:% 5.3f毫秒\n",msec(t2 - t1));
	printf("加载wasm  :% 5.3f毫秒\n",msec(t3 - t2));
	printf("执行函数  :% 5.3f毫秒\n",msec(t4 - t3));
	printf("释放内存  :% 5.3f毫秒\n",msec(t5 - t4));
	printf("内存分配次数:% 5d\n",malloc_times);
	printf("内存释放次数:% 5d\n",free_times);
	return 0;
}
