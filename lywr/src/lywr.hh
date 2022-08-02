#ifndef _lyramilk_lywr_hh_
#define _lyramilk_lywr_hh_
#include "lywr_defs.h"
#include "lywr.h"
#include "lywr_rbtree2.h"
#include "lywr_opcode.h"

LYWR_NAMESPACE_START


typedef enum lywrmoduletype{
	lywrmt_wasm = 0,		//wasm文件
	lywrmt_native = 1,		//本地接口
}lywrmoduletype;

//	这个结构在lywr中不使用，只是传回给lywr_malloc_cbk/lywr_free_cbk由调用者自己使用。
typedef struct lywr_pool lywr_pool;
typedef struct lywr_poolx lywr_poolx;
typedef struct lywr_module lywr_module;
typedef struct lywr_ctx lywr_ctx;


typedef struct lywr_instruction_br_table
{
	unsigned int target_count;
	unsigned int default_target;
	unsigned int target_table[1];
}lywr_instruction_br_table;


typedef struct lywr_instruction_block
{
	unsigned char block_type;
	struct lywr_instruction* _then;
	struct lywr_instruction* _else;
}lywr_instruction_block;


typedef struct lywr_instruction
{
	lywr_opcode code:8;
	union {
		unsigned int u1;
		int i32;
		long long i64;
		unsigned int u32;
		unsigned long long u64;
		float f32;
		double f64;
		struct lywr_instruction_block* block;
		struct lywr_instruction_br_table* br_table;
		unsigned int sig;
		unsigned int relative_depth;
		unsigned int function_index;
		unsigned int local_index;
		unsigned int global_index;
		struct{
			unsigned int type_index;
			unsigned char reserved;
		}call_indirect_extend;
		struct{
			unsigned int flags;
			unsigned int offset;
		}memory_immediate;
	} u;
	int line;
	struct lywr_instruction* next;
}lywr_instruction;

typedef struct lywr_expression
{
	lywr_instruction* insts;
}lywr_expression;

typedef unsigned char wasm_byte;

//	varuint1
//	varint7
//	varuint32
typedef unsigned char wasm_uint8;
typedef unsigned int wasm_uint32;
typedef unsigned char wasm_varuint1;
typedef char wasm_varint7;
typedef unsigned char wasm_varuint7;
typedef unsigned int wasm_varuint32;
typedef unsigned long long wasm_varuint64;

typedef enum wasm_value_type{
	wasm_value_type_i32 = 0x7f,
	wasm_value_type_i64 = 0x7e,
	wasm_value_type_f32 = 0x7d,
	wasm_value_type_f64 = 0x7c,
}wasm_value_type;

//typedef wasm_varint7 wasm_value_type;
typedef wasm_varint7 wasm_elem_type;

typedef char wasm_external_kind;

typedef struct lywr_function_type_spec{
	char* shortcut;		// i=i32,I=i64,f=f64,F=f32,:后面是返回值，例如   ii:f   含义是接收两个i32参数，返回一个f32
	int shortcutlen;

	int rcount;			//返回值数量(由lywr_function_init_spec填写)
	lywr_data* rval;	//返回值(由lywr_function_exec填写)

	int argc;			//参数数量(由lywr_function_init_spec填写)
	lywr_data* argv;	//参数值(由lywr_function_init_spec初始化后再由调用者根据每一项的type填写值部分)
}lywr_function_type_spec;



//	func_type
typedef struct wasm_func_type
{
	wasm_varuint32 param_count;
	wasm_value_type* param_types;

	wasm_varuint1 return_count;
	wasm_value_type* return_type;

	lywr_function_type_spec spec;
} wasm_func_type;


typedef struct wasm_resizable_limits
{
	wasm_varuint1 flags;
	wasm_varuint64 initial;
	wasm_varuint64 maximum;
} wasm_resizable_limits;


typedef struct wasm_elem_segment wasm_elem_segment;
//	table_type
typedef struct wasm_table_type
{
	wasm_elem_type element_type;
	wasm_resizable_limits limits;
	wasm_varuint32* element_list;
} wasm_table_type;


//	memory_type
typedef struct wasm_memory_type
{
	wasm_resizable_limits limits;
} wasm_memory_type;

//	global_type
typedef struct wasm_global_type
{
	wasm_value_type content_type;
	wasm_varuint1 mutability;
} wasm_global_type;


//	import_entry
typedef struct wasm_import_entry
{
	wasm_varuint32 module_len;
	wasm_byte* module_str;
	wasm_varuint32 field_len;
	wasm_byte* field_str;
	wasm_external_kind kind;
	union {
		//	import_entry | type=function	kind=0
		wasm_varuint32 function;
		//	import_entry | type=table		kind=1
		wasm_table_type table;
		//	import_entry | type=memory		kind=2
		wasm_memory_type memory;
		//	import_entry | type=global		kind=3
		wasm_global_type global;
	};
} wasm_import_entry;


typedef struct wasm_init_expr
{
	lywr_opcode init_expr_type:8;
	union {
		unsigned int u32;
		unsigned long long u64;
		float f32;
		double f64;
		wasm_uint32 global_index;
	} u;
}wasm_init_expr;


typedef struct wasm_export_entry
{
	wasm_varuint32 field_len;
	wasm_byte* field_str;
	wasm_external_kind kind;
	wasm_varuint32 index;
}wasm_export_entry;

typedef struct wasm_global_variable
{
	wasm_global_type type;
	wasm_init_expr expr;
} wasm_global_variable;

typedef struct wasm_elem_segment
{
	wasm_varuint32 index;
	wasm_init_expr offset;
	wasm_varuint32 num_elem;
	wasm_varuint32* elems;
} wasm_elem_segment;

typedef struct wasm_local_entry
{
	wasm_varuint32 count;
	wasm_value_type type;
} wasm_local_entry;



typedef enum lywr_lazy_data_phase{
	lywr_lazy_data_phase_unload = 0,	// 未加载
	lywr_lazy_data_phase_load = 1,		// 己加载
	lywr_lazy_data_phase_compiled = 2,	// 已编译为expr
	lywr_lazy_data_phase_optimize = 3,	// 优化完成
}lywr_lazy_data_phase;

typedef struct lywr_lazy_data{
	lywr_lazy_data_phase phase:8;
	union{
		struct
		{
			unsigned long long offset_of_wasm;
			unsigned long long size;
		};//	phase = 0;
		wasm_byte* data;	//	phase =1;
		lywr_expression expr;	//	phase =2;
	};
}lywr_lazy_data;


typedef struct wasm_function_body 
{
	wasm_varuint64 body_size;
	wasm_varuint32 local_count;
	wasm_local_entry* locals;
	lywr_lazy_data code;


} wasm_function_body;

typedef struct wasm_data_segment
{
	wasm_varuint32 index;
	wasm_init_expr offset;
	wasm_varuint64 size;
	lywr_lazy_data data;
} wasm_data_segment;


typedef enum lywr_module_type
{
	lywr_module_type_native = 1,
	lywr_module_type_wasm,
}lywr_module_type;

typedef struct lywr_symbol
{
	lywr_module_type type;
	wasm_byte* fieldname;
	wasm_varuint32 namelength;
	union{
		struct{
			lywr_module* mdl;
			wasm_export_entry* entry;
		};
		struct{
			lywr_native_function_cbk invoker;
		};
	};
}lywr_symbol;


typedef enum lywr_wasm_resource_link_type{
	lywr_wasm_resource_link_type_import,
	lywr_wasm_resource_link_type_local,
}lywr_wasm_resource_link_type;

typedef struct lywr_wasm_resource_link
{
	lywr_wasm_resource_link_type type;
	wasm_varuint32 index;
} lywr_wasm_resource_link;

typedef struct lywr_module
{
	lywr_module_type type;
	lywr_module_spec spec;
	unsigned long long offset;

	lywr_poolx* mloader;
	void* uptr;

	char* name;
	unsigned int namelength;

	wasm_varuint32 types_count;
	wasm_func_type* types;

	wasm_varuint32 imports_count;
	wasm_import_entry* imports;
	
	wasm_varuint32 functions_count;
	wasm_varuint32* functions;
	
	wasm_varuint32 tables_count;
	wasm_table_type* tables;
	
	wasm_varuint32 memorys_count;
	wasm_memory_type* memorys;
	
	wasm_varuint32 globals_count;
	wasm_global_variable* globals;
	
	wasm_varuint32 exports_count;
	wasm_export_entry* exports;
	
	wasm_varuint32 elements_count;
	wasm_elem_segment* elements;
	
	wasm_varuint32 functionbodys_count;
	wasm_function_body* functionbodys;
	
	wasm_varuint32 datas_count;
	wasm_data_segment* datas;
	

	wasm_varuint32 merge_function_count;
	lywr_wasm_resource_link* merge_function;

	wasm_varuint32 merge_table_count;
	lywr_wasm_resource_link* merge_table;

	wasm_varuint32 merge_memory_count;
	lywr_wasm_resource_link* merge_memory;

	wasm_varuint32 merge_global_count;
	lywr_wasm_resource_link* merge_global;
/*
	lywr_rbtree2_ctx* export_table;	//	map<fieldname,fieldid>
	lywr_rbtree2_ctx* import_table;	//	map<modulename.fieldname,fieldid>

	*/

	lywr_rbtree2_ctx symbol_table;
} lywr_module;



typedef union lywr_data_cell{
	unsigned int u32;
	int i32;
	unsigned long long u64;
	long long i64;
	float f32;
	double f64;
}lywr_data_cell;

typedef struct lywr_data_array {
	lywr_pool* mempool;
	lywr_data_cell* ptr;
	unsigned int size;
	unsigned int blucksize;
}lywr_data_array;

typedef struct lywr_datastack
{
	lywr_data_cell* start;
	long long size;
	long long cursor;
	lywr_pool* mempool;
}lywr_datastack;

typedef struct lywr_instruction_stack
{
	lywr_instruction const ** start;
	long long size;
	long long cursor;
	lywr_pool* mempool;
}lywr_instruction_stack;



typedef struct lywr_linearmemory
{
	unsigned char* start;
	unsigned long long size;
	unsigned int pagecount;
	unsigned int maximum;
	lywr_pool* mempool;
}lywr_linearmemory;

typedef struct lywr_callframe
{
	lywr_data_array local;
	lywr_linearmemory* memory;	//指针
	lywr_instruction_stack runblock;
} lywr_callframe;



typedef struct lywr_callframe_stack_node
{
	struct lywr_callframe_stack_node* parent;
	struct lywr_callframe_stack_node* child;
	lywr_callframe frame;
}lywr_callframe_stack_node;



typedef struct lywr_callframe_stack
{
	lywr_callframe_stack_node* bottom;
	lywr_pool* mempool;
}lywr_callframe_stack;

typedef struct lywr_session
{
	lywr_pool* mempool;
	lywr_datastack stack;
	lywr_linearmemory** memory;	//为了多模块之间的线性内存可共享，所以做成了两级指针。
	lywr_callframe_stack frames;
	lywr_callframe* frame;
} lywr_session;



lywrrc lywr_module_call_function_by_index(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,wasm_varuint32 index);


lywrrc lywr_get_module_by_name(lywr_ctx* ctx,const char* modulename,int modulenamelength,lywr_module** mdl);
lywrrc lywr_get_symbol_by_name(lywr_ctx* ctx,lywr_module* mdl,const char* funcname,int funcnamelength,lywr_symbol** symbol);

lywrrc lywr_get_lazy_data_as_bytes(lywr_ctx* ctx,lywr_module* mdl,lywr_lazy_data* data,wasm_byte** ret);
lywrrc lywr_get_lazy_data_as_expr(lywr_ctx* ctx,lywr_module* mdl,lywr_lazy_data* data,lywr_expression** ret);

lywrrc lywr_get_i64_init_expr(lywr_ctx* ctx,lywr_module* mdl,wasm_init_expr* init_expr,long long* rvalue);

LYWR_NAMESPACE_END


#endif
