#include "lywr.hh"
#include "lywr_expr.hh"
#include "lywr_stream.hh"
#include "lywr_opcode.h"
#include "lywr_data.hh"
#include "lywr_util.hh"

LYWR_NAMESPACE_START

#define LYWRRC_VERIFY(x) {lywrrc rc = x;if(rc != lywrrc_ok) {TRACE0("出错了，错误码是:%d\n",rc);return rc;}}

#define TRACE0(...)

#define nullptr (void*)0

lywrrc static lywr_leb128_get64i(wasm_byte** ptr,long long *value)
{
	unsigned long long r = 0;
	unsigned int idx = 0;
	unsigned char c;
	while(1){
		c = *(*ptr)++;
		unsigned long long k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}

	if(idx != 64 && (c & 0x40)){
		r |= - (1 << (idx + 7));
	}

	*value = r;
	return lywrrc_ok;
}

lywrrc static lywr_leb128_get32u(wasm_byte** ptr,unsigned int *value)
{
	unsigned int r = 0;
	unsigned int idx = 0;
	while(1){
		unsigned char c = *(*ptr)++;
		unsigned int k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}
	*value = r;
	return lywrrc_ok;
}

lywrrc static lywr_leb128_get32i(wasm_byte** ptr,int *value)
{
	int r = 0;
	unsigned int idx = 0;
	unsigned char c;
	while(1){
		c = *(*ptr)++;
		unsigned int k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}
	if(idx != 28 && (c & 0x40)){
		r |= - (1 << (idx + 7));
	}

	*value = r;
	return lywrrc_ok;
}

lywrrc static lywr_get_nbytes(wasm_byte** ptr,void *value,int count)
{
	lywr_memcpy(value,*ptr,count);
	(*ptr) += count;
	return lywrrc_ok;
}

/*


typedef struct lywr_instruction
{
	lywr_opcode code:8;
	union {
		unsigned int u32;
		unsigned long long u64;
		float f32;
		double f64;
		struct lywr_instruction* _else;
		struct lywr_instruction* br_table;
		unsigned int sig;
	} u;
	struct lywr_instruction* child;
	struct lywr_instruction* next;
}lywr_instruction;

*/


lywrrc lywr_parse_expr(wasm_byte* code,unsigned long long length,lywr_expression* expr,lywr_pool* pool,int flag)
{
	expr->insts = lywr_pool_malloc(pool,sizeof(lywr_instruction));
	if(expr->insts == nullptr) return lywrrc_oom;

	lywr_instruction_stack bst;
	LYWRRC_VERIFY(lywr_instruction_stack_init(&bst,pool));

	lywr_instruction* cr = expr->insts;
	wasm_byte* pc = code;
	wasm_byte* pend = code + length;
	unsigned int i =0;
	while(pc < pend){
		cr->code = *pc++;
		cr->line = ++i;
		switch(cr->code){
		case lywr_op_unreachable: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_nop: {
			}break;
		case lywr_op_block: {
				cr->u.block = lywr_pool_malloc(pool,sizeof(lywr_instruction_block));
				if(cr->u.block == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}

				cr->u.block->_then = lywr_pool_malloc(pool,sizeof(lywr_instruction));
				if(cr->u.block->_then == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}
				cr->u.block->_else = nullptr;
				cr->u.block->block_type = *pc++;

				lywrrc rc = lywr_instruction_stack_push(&bst,(const lywr_instruction **)&cr);
				if(rc != lywrrc_ok){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return rc;
				}

				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
				cr = cr->u.block->_then;
				continue;
			}break;
		case lywr_op_loop: {
				cr->u.block = lywr_pool_malloc(pool,sizeof(lywr_instruction_block));
				if(cr->u.block == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}
				cr->u.block->_then = lywr_pool_malloc(pool,sizeof(lywr_instruction));
				if(cr->u.block->_then == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}

				cr->u.block->_else = nullptr;
				cr->u.block->block_type = *pc++;

				lywrrc rc = lywr_instruction_stack_push(&bst,(const lywr_instruction **)&cr);
				if(rc != lywrrc_ok){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return rc;
				}

				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
				cr = cr->u.block->_then;
				continue;
			}break;
		case lywr_op_if: {
				cr->u.block = lywr_pool_malloc(pool,sizeof(lywr_instruction_block));
				if(cr->u.block == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}
				cr->u.block->_then = lywr_pool_malloc(pool,sizeof(lywr_instruction));
				if(cr->u.block->_then == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}

				cr->u.block->_else = nullptr;
				cr->u.block->block_type = *pc++;

				lywrrc rc = lywr_instruction_stack_push(&bst,(const lywr_instruction **)&cr);
				if(rc != lywrrc_ok){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return rc;
				}

				if(cr->u.block->block_type == 0x40){
					TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
				}
				if(cr->u.block->block_type == 0x7f){
					TRACE0("解析 %s i32\n",lywr_opcode_name(cr->code));
				}
				if(cr->u.block->block_type == 0x7e){
					TRACE0("解析 %s i64\n",lywr_opcode_name(cr->code));
				}
				if(cr->u.block->block_type == 0x7d){
					TRACE0("解析 %s f32\n",lywr_opcode_name(cr->code));
				}
				if(cr->u.block->block_type == 0x7c){
					TRACE0("解析 %s f64\n",lywr_opcode_name(cr->code));
				}

				cr = cr->u.block->_then;
				continue;
			}break;
		case lywr_op_else: {
				cr->code = lywr_op_end;
				lywrrc rc = lywr_instruction_stack_pop(&bst,(const lywr_instruction **)&cr,1);
				if(rc != lywrrc_ok){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return rc;
				}

				cr->u.block->_else = lywr_pool_malloc(pool,sizeof(lywr_instruction));
				if(cr->u.block->_else == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}

				TRACE0("解析 else\n");
				cr = cr->u.block->_else;
				continue;	//不自动切换cr
			}continue;
		case lywr_op_end: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
				cr->next = nullptr;
				lywr_instruction_stack_pop(&bst,(const lywr_instruction **)&cr,0);	//为lywrrc_oom也没有关系，因为进入函数的时候没有push进去一个块，所以函数end的时候会报oom，可以忽略。
			}break;
		case lywr_op_br: {
				lywr_leb128_get32u(&pc,&cr->u.relative_depth);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.relative_depth);
			}break;
		case lywr_op_br_if: {
				lywr_leb128_get32u(&pc,&cr->u.relative_depth);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.relative_depth);
			}break;
		case lywr_op_br_table: {
				unsigned int target_count;
				lywr_leb128_get32u(&pc,&target_count);
				cr->u.br_table = lywr_pool_malloc(pool,target_count * sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int));
				if(cr->u.br_table == nullptr){
					cr->next = nullptr;
					lywr_instruction_stack_destory(&bst);
					return lywrrc_oom;
				}
				cr->u.br_table->target_count = target_count;
				for(unsigned int i=0;i<target_count;++i){
					lywr_leb128_get32u(&pc,cr->u.br_table->target_table + i);
				}
				lywr_leb128_get32u(&pc,&cr->u.br_table->default_target);
			}break;
		case lywr_op_return: {
			}break;
		case lywr_op_call: {
				lywr_leb128_get32u(&pc,&cr->u.function_index);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.function_index);
			}break;
		case lywr_op_call_indirect: {
				lywr_leb128_get32u(&pc,&cr->u.call_indirect_extend.type_index);
				lywr_get_nbytes(&pc,&cr->u.call_indirect_extend.reserved,1);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.call_indirect_extend.type_index);
			}break;
		case lywr_op_drop: {
			}break;
		case lywr_op_select: {
			}break;
		case lywr_op_get_local:
		case lywr_op_set_local:
		case lywr_op_tee_local:{
				lywr_leb128_get32u(&pc,&cr->u.local_index);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.local_index);
			}break;
		case lywr_op_get_global:
		case lywr_op_set_global: {
				lywr_leb128_get32u(&pc,&cr->u.global_index);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.global_index);
				if(flag&LYWR_PARSE_EXPR_FLAG_NO_GLOBAL) cr->code = lywr_op_unreachable;
			}break;
		case lywr_op_i32_load:
		case lywr_op_i64_load:
		case lywr_op_f32_load:
		case lywr_op_f64_load:
		case lywr_op_i32_load8_s:
		case lywr_op_i32_load8_u:
		case lywr_op_i32_load16_s:
		case lywr_op_i32_load16_u:
		case lywr_op_i64_load8_s:
		case lywr_op_i64_load8_u:
		case lywr_op_i64_load16_s:
		case lywr_op_i64_load16_u:
		case lywr_op_i64_load32_s:
		case lywr_op_i64_load32_u:
		case lywr_op_i32_store:
		case lywr_op_i64_store:
		case lywr_op_f32_store:
		case lywr_op_f64_store:
		case lywr_op_i32_store8:
		case lywr_op_i32_store16:
		case lywr_op_i64_store8:
		case lywr_op_i64_store16:
		case lywr_op_i64_store32: {
				lywr_leb128_get32u(&pc,&cr->u.memory_immediate.flags);
				lywr_leb128_get32u(&pc,&cr->u.memory_immediate.offset);
				if(flag&LYWR_PARSE_EXPR_FLAG_NO_MEMORY) cr->code = lywr_op_unreachable;
				TRACE0("解析 %s %d %d\n",lywr_opcode_name(cr->code),cr->u.memory_immediate.flags,cr->u.memory_immediate.offset);
			}break;
		case lywr_op_memory_size:
		case lywr_op_memory_grow: {
				cr->u.u1 = *pc++;
				if(flag&LYWR_PARSE_EXPR_FLAG_NO_MEMORY) cr->code = lywr_op_unreachable;
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.u1);
			}break;
		case lywr_op_i32_const: {
				lywr_leb128_get32i(&pc,&cr->u.i32);
				TRACE0("解析 %s %d\n",lywr_opcode_name(cr->code),cr->u.i32);
			}break;
		case lywr_op_i64_const: {
				lywr_leb128_get64i(&pc,&cr->u.i64);
				TRACE0("解析 %s %llu\n",lywr_opcode_name(cr->code),cr->u.i64);
			}break;
		case lywr_op_f32_const: {
				lywr_get_nbytes(&pc,&cr->u.f32,4);
				TRACE0("解析 %s %f\n",lywr_opcode_name(cr->code),cr->u.f32);
			}break;
		case lywr_op_f64_const: {
				lywr_get_nbytes(&pc,&cr->u.f64,8);
				TRACE0("解析 %s %f\n",lywr_opcode_name(cr->code),cr->u.f64);
			}break;
		case lywr_op_i32_eqz: 
		case lywr_op_i32_eq: 
		case lywr_op_i32_ne: 
		case lywr_op_i32_lt_s: 
		case lywr_op_i32_lt_u: 
		case lywr_op_i32_gt_s: 
		case lywr_op_i32_gt_u: 
		case lywr_op_i32_le_s: 
		case lywr_op_i32_le_u: 
		case lywr_op_i32_ge_s: 
		case lywr_op_i32_ge_u: 
		case lywr_op_i64_eqz: 
		case lywr_op_i64_eq: 
		case lywr_op_i64_ne: 
		case lywr_op_i64_lt_s: 
		case lywr_op_i64_lt_u: 
		case lywr_op_i64_gt_s: 
		case lywr_op_i64_gt_u: 
		case lywr_op_i64_le_s: 
		case lywr_op_i64_le_u: 
		case lywr_op_i64_ge_s: 
		case lywr_op_i64_ge_u: 
		case lywr_op_f32_eq: 
		case lywr_op_f32_ne: 
		case lywr_op_f32_lt: 
		case lywr_op_f32_gt: 
		case lywr_op_f32_le: 
		case lywr_op_f32_ge: 
		case lywr_op_f64_eq: 
		case lywr_op_f64_ne: 
		case lywr_op_f64_lt: 
		case lywr_op_f64_gt: 
		case lywr_op_f64_le: 
		case lywr_op_f64_ge: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_i32_clz: 
		case lywr_op_i32_ctz: 
		case lywr_op_i32_popcnt: 
		case lywr_op_i32_add: 
		case lywr_op_i32_sub: 
		case lywr_op_i32_mul: 
		case lywr_op_i32_div_s: 
		case lywr_op_i32_div_u: 
		case lywr_op_i32_rem_s: 
		case lywr_op_i32_rem_u: 
		case lywr_op_i32_and: 
		case lywr_op_i32_or: 
		case lywr_op_i32_xor: 
		case lywr_op_i32_shl: 
		case lywr_op_i32_shr_s: 
		case lywr_op_i32_shr_u: 
		case lywr_op_i32_rotl: 
		case lywr_op_i32_rotr: 
		case lywr_op_i64_clz: 
		case lywr_op_i64_ctz: 
		case lywr_op_i64_popcnt: 
		case lywr_op_i64_add: 
		case lywr_op_i64_sub: 
		case lywr_op_i64_mul: 
		case lywr_op_i64_div_s: 
		case lywr_op_i64_div_u: 
		case lywr_op_i64_rem_s: 
		case lywr_op_i64_rem_u: 
		case lywr_op_i64_and: 
		case lywr_op_i64_or: 
		case lywr_op_i64_xor: 
		case lywr_op_i64_shl: 
		case lywr_op_i64_shr_s: 
		case lywr_op_i64_shr_u: 
		case lywr_op_i64_rotl: 
		case lywr_op_i64_rotr: 
		case lywr_op_f32_abs: 
		case lywr_op_f32_neg: 
		case lywr_op_f32_ceil: 
		case lywr_op_f32_floor: 
		case lywr_op_f32_trunc: 
		case lywr_op_f32_nearest: 
		case lywr_op_f32_sqrt: 
		case lywr_op_f32_add: 
		case lywr_op_f32_sub: 
		case lywr_op_f32_mul: 
		case lywr_op_f32_div: 
		case lywr_op_f32_min: 
		case lywr_op_f32_max: 
		case lywr_op_f32_copysign: 
		case lywr_op_f64_abs: 
		case lywr_op_f64_neg: 
		case lywr_op_f64_ceil: 
		case lywr_op_f64_floor: 
		case lywr_op_f64_trunc: 
		case lywr_op_f64_nearest: 
		case lywr_op_f64_sqrt: 
		case lywr_op_f64_add: 
		case lywr_op_f64_sub: 
		case lywr_op_f64_mul: 
		case lywr_op_f64_div: 
		case lywr_op_f64_min: 
		case lywr_op_f64_max: 
		case lywr_op_f64_copysign: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_i32_wrap_i64: 
		case lywr_op_i32_trunc_s_f32: 
		case lywr_op_i32_trunc_u_f32: 
		case lywr_op_i32_trunc_s_f64: 
		case lywr_op_i32_trunc_u_f64: 
		case lywr_op_i64_extend_s_i32: 
		case lywr_op_i64_extend_u_i32: 
		case lywr_op_i64_trunc_s_f32: 
		case lywr_op_i64_trunc_u_f32: 
		case lywr_op_i64_trunc_s_f64: 
		case lywr_op_i64_trunc_u_f64: 
		case lywr_op_f32_convert_s_i32: 
		case lywr_op_f32_convert_u_i32: 
		case lywr_op_f32_convert_s_i64: 
		case lywr_op_f32_convert_u_i64: 
		case lywr_op_f32_demote_f64: 
		case lywr_op_f64_convert_s_i32: 
		case lywr_op_f64_convert_u_i32: 
		case lywr_op_f64_convert_s_i64: 
		case lywr_op_f64_convert_u_i64: 
		case lywr_op_f64_promote_f32: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_i32_reinterpret_f32:
		case lywr_op_i64_reinterpret_f64: 
		case lywr_op_f32_reinterpret_i32: 
		case lywr_op_f64_reinterpret_i64: {
				TRACE0("解析 %s\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_i32_extend8_s: 
		case lywr_op_i32_extend16_s:
		case lywr_op_i64_extend8_s:
		case lywr_op_i64_extend16_s:
		case lywr_op_i64_extend32_s: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_drop_64: 
		case lywr_op_select_64: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_ext_op_get_local_fast:
		case lywr_op_ext_op_set_local_fast_i64:
		case lywr_op_ext_op_set_local_fast:
		case lywr_op_ext_op_tee_local_fast:
		case lywr_op_ext_op_tee_local_fast_i64: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_ext_op_copy_stack_top:
		case lywr_op_ext_op_copy_stack_top_i64: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_impdep: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_misc_prefix: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_simd_prefix: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_thread_prefix: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		case lywr_op_reserved_prefix: {
				TRACE0("解析 %s 不支持该指令\n",lywr_opcode_name(cr->code));
			}break;
		default:
			cr->next = nullptr;
			lywr_instruction_stack_destory(&bst);
			return lywrrc_bad_format;
		}
		cr->next = lywr_pool_malloc(pool,sizeof(lywr_instruction));
		if(cr->next == nullptr){
			lywr_instruction_stack_destory(&bst);
			return lywrrc_oom;
		}
		cr = cr->next;
	}
	if(cr)cr->next = nullptr;
	lywr_instruction_stack_destory(&bst);
	return lywrrc_bad_format;
}

LYWR_NAMESPACE_END
