#ifndef _lyramilk_lywr_h_
#define _lyramilk_lywr_h_
#include "lywr_defs.h"
#include "lywr_rbtree2.h"
#include "lywr_mempool.h"

LYWR_NAMESPACE_START

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#define I16(x) __builtin_bswap16(x)
	#define I32(x) __builtin_bswap32(x)
	#define I64(x) __builtin_bswap64(x)

	#define LYWR_BIG_ENDIAN 1
#else
	#define I16(x) x
	#define I32(x) x
	#define I64(x) x

	#define LYWR_LITTLE_ENDIAN 1
#endif




typedef struct lywr_ctx lywr_ctx;
typedef struct lywr_module lywr_module;
typedef struct lywr_symbol lywr_symbol;
typedef struct lywr_session lywr_session;
typedef struct lywr_module_spec lywr_module_spec;


typedef enum lywrrc{
	lywrrc_ok = 0,			//成功
	lywrrc_io_error = 1,	//io错误
	lywrrc_oom = 2,			//内存不足
	lywrrc_bad_format = 3,	//文件格式错误
	lywrrc_eof = 4,			//文件结束
	lywrrc_trap = 5,		//触发了unreachable
	lywrrc_not_found = 6,	//没有找到
	lywrrc_forbid = 7,		//禁止
	lywrrc_overflow = 8,	//数组越界
	lywrrc_rbtree = 9,		//红黑树的错误
	lywrrc_param = 10,		//参数错误
	lywrrc_not_implement = 11,		//未实现
#ifdef _DEBUG
	lywrrc_err = 9999999,	//开发过程中临时使用，最终生成中不使用这个不明确的错误。
#endif
}lywrrc;

typedef enum lywr_module_spec_lazy_enum{
	lywr_module_spec_load_normal,	//	直接加载，适用于小文件或者需要完全执行的文件。
	lywr_module_spec_load_lazy,		//	惰性加载，函数体或数据体只有用到的时候才会加载。
}lywr_module_spec_lazy_enum;

// 内部用单字节表示这个枚举，所以值不能大于255
#define LYWR_DATA_64BITS 0x10
#define LYWR_DATA_SIGNED 0x02
#define LYWR_DATA_FLOAT  0x01

typedef enum lywr_data_type{
	lywr_data_u32 = 0 | 0,
	lywr_data_u64 = 0 | LYWR_DATA_64BITS,
	lywr_data_i32 = LYWR_DATA_SIGNED | 0,
	lywr_data_i64 = LYWR_DATA_SIGNED | LYWR_DATA_64BITS,
	lywr_data_f32 = LYWR_DATA_FLOAT | 0,
	lywr_data_f64 = LYWR_DATA_FLOAT | LYWR_DATA_64BITS,
}lywr_data_type;

typedef struct lywr_data {
	lywr_data_type type;
	union {
		unsigned int u32;
		int i32;
		unsigned long long u64;
		long long i64;
		float f32;
		double f64;
	};
}lywr_data;


typedef struct lywr_function_spec{
	const char* mod;	//模块名(由调用者填写)
	int modlen;			//模块名长度(由调用者填写)
	const char* field;	//方法名(由调用者填写)
	int fieldlen;		//方法名长度(由调用者填写)

	lywr_module* mdl;		//不要动
	lywr_symbol* symbol;	//不要动

	char* spec;			// i=i32,I=i64,f=f64,F=f32,:后面是返回值，例如   ii:f   含义是接收两个i32参数，返回一个f32
	int speclen;

	int rcount;			//返回值数量(由lywr_function_init_spec填写)
	lywr_data* rval;	//返回值(由lywr_function_exec填写)

	int argc;			//参数数量(由lywr_function_init_spec填写)
	lywr_data* argv;	//参数值(由lywr_function_init_spec初始化后再由调用者根据每一项的type填写值部分)
}lywr_function_spec;

//	为模块读文件
typedef lywrrc (*lywr_reader_pread_cbk)(struct lywr_ctx* ctx,char* buf,int bufsize,unsigned long long ofset,void* userdata);
//	加载模块
typedef lywrrc (*lywr_load_module_cbk)(struct lywr_ctx* ctx,char* modulename,int modulenamelength,lywr_module_spec* module_spec);

//	外部函数
typedef lywrrc (*lywr_native_function_cbk)(lywr_ctx* ctx,lywr_function_spec* spec);

typedef struct lywr_module_spec
{
	void* userdata;
	lywr_module_spec_lazy_enum lazyload;
	lywr_reader_pread_cbk pread;
	unsigned long long modulesize;
}lywr_module_spec;


typedef struct lywr_ctx
{
	lywr_pool* mloader;

	lywr_rbtree2_ctx symbol_module_table;

	// 内存分配
	lywr_malloc_cbk pmalloc;
	lywr_free_cbk pfree;

		/// 加载模块
	lywr_load_module_cbk load;
	
	// 用户数据
	void* uptr;
} lywr_ctx;

lywrrc LYWR_EXPORT lywr_init(lywr_ctx* ctx,lywr_malloc_cbk al,lywr_free_cbk fr,void* userdata);
lywrrc LYWR_EXPORT lywr_destory(lywr_ctx* ctx);

// 加载一个wasm模块
lywrrc LYWR_EXPORT lywr_load_wasm_module(lywr_ctx* ctx,lywr_module_spec* spec,const char* modulename,int modulenamelength);

// 初始化一个本地模块，并通过mdl参数获得它
lywrrc LYWR_EXPORT lywr_native_module_init(lywr_ctx* ctx,const char* modulename,int modulenamelength,lywr_module** mdl);
// 向初始化好的本地函数注入本地模块
lywrrc LYWR_EXPORT lywr_native_module_function(lywr_ctx* ctx,lywr_module* mdl,const char* funcname,int funcnamelength,lywr_native_function_cbk cbk);
// 向初始化好的本地内存块注入本地模块
lywrrc LYWR_EXPORT lywr_native_module_memory(lywr_ctx* ctx,lywr_module* mdl,const char* funcname,int funcnamelength,char* ptr,long long size);

// 初始化一个函数的参数及返回值对象spec，spec中的argv的内存会分配好。要先填好mod(模块名)、modlen(模块名长度)、field(方法名)、fieldlen(方法名长度)。
lywrrc LYWR_EXPORT lywr_function_init_spec(lywr_ctx* ctx,lywr_function_spec* spec);
//	调用一个函数，可以向argv写入参数，并在调用完成后通过rval获得返回值
lywrrc LYWR_EXPORT lywr_function_exec(lywr_ctx* ctx,lywr_function_spec* spec);
// 释放掉 lywr_function_init_spec 创建的spec对象
lywrrc LYWR_EXPORT lywr_function_destory_spec(lywr_ctx* ctx,lywr_function_spec* spec);

LYWR_NAMESPACE_END


#endif
