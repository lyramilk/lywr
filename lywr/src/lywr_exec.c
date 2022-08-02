#include "lywr.hh"
#include "lywr_exec.hh"
#include "lywr_data.hh"
#include "lywr_util.hh"

LYWR_NAMESPACE_START

#define LYWRRC_VERIFY(x) {lywrrc rc = x;if(rc != lywrrc_ok) {TRACE("出错了，错误码是:%d\n",rc);return rc;}}


#define nullptr (void*)0



#define LYWR_OP_CALLBACK(OP,NAME) lywr_##NAME##_callback,
#define LYWR_OP_CALLBACK_UNUSE(OP,...) lywr_op_reserve_callback,

#define LYWR_OP_CALLBACK_DISPATH(OP,...) lywr_op_dispath_callback,
/*
#define LYWR_FUNCTION_VERIFY(XOP) { lywrrc __rc = XOP;	\
				if(__rc != lywrrc_ok){	\
					TRACE("执行函数%d(第%d行)出错，错误码是%d\n",index,inst->line,(int)__rc);	\
					lywr_callframe_stack_prev(&session->frames,&session->frame);	\
					if(__rc == lywrrc_eof) return lywrrc_ok;	\
					return __rc;	\
				}	\
			}
*/

#define LYWR_FUNCTION_VERIFY(XOP) { lywrrc __rc = XOP;	\
				if(__rc != lywrrc_ok){	\
					TRACE("执行函数%d(第%d行)出错，错误码是%d\n",index,inst->line,(int)__rc);	\
					lywr_callframe_stack_prev(&session->frames,&session->frame);	\
					return __rc;	\
				}	\
			}


static lywr_dispatch_type dispatch[256] = {
	LYWR_OP_DEFINE(LYWR_OP_CALLBACK_DISPATH,LYWR_OP_CALLBACK_DISPATH)
};

static lywr_dispatch_type dispatch2[256] = {
	LYWR_OP_DEFINE(LYWR_OP_CALLBACK,LYWR_OP_CALLBACK_UNUSE)
};

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

lywrrc lywr_module_call_function_by_index(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,wasm_varuint32 index)
{
	if(mdl->merge_function[index].type == lywr_wasm_resource_link_type_local){
		TRACE("执行内部函数:%d,全局id=%d\n",mdl->merge_function[index].index,index);
		wasm_varuint32 functionindex = mdl->merge_function[index].index;

		wasm_function_body* fb = mdl->functionbodys + functionindex;



		lywr_callframe* fr;
		lywr_callframe_stack_next(&session->frames,&fr);
		if(mdl->datas){
			wasm_data_segment* ds = mdl->datas + functionindex;
			fr->memory = session->memory[ds->index];
			TRACE("memory=%p\n",(void*)fr->memory);
		}else if(session->memory){
			fr->memory = session->memory[0];
			TRACE("memory=%p\n",(void*)fr->memory);
		}else{
			fr->memory = nullptr;
			TRACE("memory=%p\n",(void*)fr->memory);
		}

		wasm_varuint32 typeindex = mdl->functions[functionindex];
		wasm_func_type* ftype = mdl->types + typeindex;
		TRACE("执行函数%d成功，参数%d，返回%d\n",index,ftype->param_count,ftype->return_count);


		// 把每种类型的local数量都加在一起
		wasm_varuint32 all_local_count = 0;
		all_local_count += ftype->param_count;
		for(wasm_varuint32 local_count_index = 0;local_count_index<fb->local_count;++local_count_index){
			all_local_count += fb->locals[local_count_index].count;
		}

		lywr_data_array_reset(&fr->local,all_local_count);
		lywr_instruction_stack_reset(&fr->runblock);

		session->frame = fr;


		for(wasm_varuint32 i=0;i<ftype->param_count;++i){
			TRACE("从堆栈中弹出参数并转化为local[%d]\n",ftype->param_count - i - 1);
			lywr_data_cell* data;
			lywrrc rc = lywr_data_array_at(&fr->local,ftype->param_count - i - 1,&data);
			if(data == nullptr) {
				continue;
			}

			lywr_datastack_pop_u64(&session->stack,&data->u64,0);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				//lywr_callframe_stack_prev(&session->frames,&session->frame);
				//return rc;
				continue;
			}

			TRACE("参数:%d\n",data->i32);
		}

		wasm_varuint32 local_index = ftype->param_count;
		for(wasm_varuint32 local_count_index = 0;local_count_index<fb->local_count;++local_count_index){
			for(wasm_varuint32 i = 0;i<fb->locals[local_count_index].count;++i,++local_index){
				if(fb->locals[local_count_index].type == wasm_value_type_i32){
					lywr_data_cell* data = nullptr;
					lywr_data_array_at(&fr->local,local_index,&data);
					if(data) data->u64 = 0;
				}else if(fb->locals[local_count_index].type == wasm_value_type_i64){
					lywr_data_cell* data = nullptr;
					lywr_data_array_at(&fr->local,local_index,&data);
					if(data) data->u64 = 0;
				}else if(fb->locals[local_count_index].type == wasm_value_type_f32){
					lywr_data_cell* data = nullptr;
					lywr_data_array_at(&fr->local,local_index,&data);
					if(data) data->f32 = 0;
				}else if(fb->locals[local_count_index].type == wasm_value_type_f64){
					lywr_data_cell* data = nullptr;
					lywr_data_array_at(&fr->local,local_index,&data);
					if(data) data->f64 = 0;
				}
			}
		}

		lywr_expression* expr;
		lywrrc rc = lywr_get_lazy_data_as_expr(ctx,mdl,&fb->code,&expr);
		if(rc != lywrrc_ok) {
			TRACE("出错了，错误码是:%d\n",rc);
			return rc;
		}

		const lywr_instruction* inst = expr->insts;
		

		while(inst != nullptr){
			const lywr_instruction* next = inst->next;
			TRACE("函数[%d](行号%d)执行指令%s\n",index,inst->line,lywr_opcode_name(inst->code));
			switch(inst->code){
				case lywr_op_unreachable:
					{
						LYWR_FUNCTION_VERIFY(lywrrc_trap);
						inst = next;
					}
					break;
				case lywr_op_nop:
					{
						//LYWR_FUNCTION_VERIFY(lywrrc_ok);
						inst = next;
					}
					break;
				case lywr_op_block:
					{
						if(inst->u.block->_then){
							TRACE("block\n");
							next = inst->u.block->_then;
						}else{
							TRACE("block出错\n");
							LYWR_FUNCTION_VERIFY(lywrrc_trap);
						}
						LYWR_FUNCTION_VERIFY(lywr_instruction_stack_push(&session->frame->runblock,&inst));
						inst = next;
					}
					break;
				case lywr_op_loop:
					{
						if(inst->u.block->_then){
							TRACE("loop\n");
							next = inst->u.block->_then;
						}else{
							TRACE("loop出错\n");
							LYWR_FUNCTION_VERIFY(lywrrc_trap);
						}
						LYWR_FUNCTION_VERIFY(lywr_instruction_stack_push(&session->frame->runblock,&inst));
						inst = next;
					}
					break;
				case lywr_op_if:
					{
						unsigned int value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&value,0));
						if(value != 0 && inst->u.block->_then){
							TRACE("if true\n");
							next = inst->u.block->_then;
						}else if(value == 0 && inst->u.block->_else){
							TRACE("if false\n");
							next = inst->u.block->_else;
						}else{
							TRACE("if pass\n");
							next = inst->next;
							inst = next;
							break;
						}

						LYWR_FUNCTION_VERIFY(lywr_instruction_stack_push(&session->frame->runblock,&inst));
						inst = next;
					}
					break;
				case lywr_op_else:
					{
						LYWR_FUNCTION_VERIFY(lywrrc_trap);
					}
					break;
				case lywr_op_unused_0x06:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x07:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x08:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x09:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x0a:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_end:
					{
						const lywr_instruction* parent;
						lywrrc rc = lywr_instruction_stack_pop(&session->frame->runblock,&parent,0);
						if(rc == lywrrc_oom){
							lywr_callframe_stack_prev(&session->frames,&session->frame);
							return lywrrc_ok;
						}

						LYWR_FUNCTION_VERIFY(rc);
						inst = parent->next;
					}
					break;
				case lywr_op_br:
					{
						TRACE("br退出,层级=%d\n",inst->u.relative_depth);
						const lywr_instruction* parent = nullptr;
						for(int i=0;i<=inst->u.relative_depth;++i){
							TRACE("退出一层，%d\n",i);
							LYWR_FUNCTION_VERIFY(lywr_instruction_stack_pop(&session->frame->runblock,&parent,0));

							if(parent->code == lywr_op_loop){
								TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
								next = parent->u.block->_then;
							}else{
								TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
								next = parent->next;
							}
						}
						if(parent && parent->code == lywr_op_loop){
							LYWR_FUNCTION_VERIFY(lywr_instruction_stack_push(&session->frame->runblock,&parent));
						}
						inst = next;
					}
					break;
				case lywr_op_br_if:
					{
						unsigned int value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&value,0));
						if(value != 0){
							TRACE("br_if退出,层级=%d\n",inst->u.relative_depth);
							const lywr_instruction* parent = nullptr;
							for(int i=0;i<=inst->u.relative_depth;++i){
								TRACE("退出一层，%d\n",i);
								LYWR_FUNCTION_VERIFY(lywr_instruction_stack_pop(&session->frame->runblock,&parent,0));

								if(parent->code == lywr_op_loop){
									TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
									next = parent->u.block->_then;
								}else{
									TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
									next = parent->next;
								}
							}
							if(parent && parent->code == lywr_op_loop){
								LYWR_FUNCTION_VERIFY(lywr_instruction_stack_push(&session->frame->runblock,&parent));
							}
						}else{
							TRACE("br_if跳过\n");
						}

						inst = next;
					}
					break;
				case lywr_op_br_table:
					{
						unsigned int deep = inst->u.br_table->default_target;

						int index;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&index,0));

						if(index > 0 && index < inst->u.br_table->target_count){
							deep = inst->u.br_table->target_table[index];
						}

						const lywr_instruction* parent = nullptr;
						for(int i=0;i<=deep;++i){
							TRACE("退出一层，%d\n",i);
							LYWR_FUNCTION_VERIFY(lywr_instruction_stack_pop(&session->frame->runblock,&parent,0));

							if(parent->code == lywr_op_loop){
								TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
								next = parent->u.block->_then;
							}else{
								TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
								next = parent->next;
							}
						}
						if(parent && parent->code == lywr_op_loop){
							lywr_instruction_stack_push(&session->frame->runblock,&parent);
						}

						inst = next;
					}
					break;
				case lywr_op_return:
					{
						TRACE("返回\n");
						lywr_callframe_stack_prev(&session->frames,&session->frame);
						return lywrrc_ok;
						//inst = next;
					}
					break;
				case lywr_op_call:
					{
						LYWR_FUNCTION_VERIFY(lywr_module_call_function_by_index(ctx,mdl,session,inst->u.function_index));
						inst = next;
					}
					break;
				case lywr_op_call_indirect:
					{
						unsigned int index;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&index,0));

						TRACE("间接调用函数%d\n",index);
						if(mdl->merge_table[0].type != lywr_wasm_resource_link_type_local){
							TRACE("出错了，call_indirect调用了一个导入函数\n");
							LYWR_FUNCTION_VERIFY(lywrrc_bad_format);
						}

						wasm_table_type* tab = &mdl->tables[mdl->merge_table[0].index];

						lywr_instruction tmpinst;
						tmpinst.code = lywr_op_call;
						tmpinst.u.function_index = tab->element_list[index];
						tmpinst.next = inst->next;

						LYWR_FUNCTION_VERIFY(lywr_module_call_function_by_index(ctx,mdl,session,tmpinst.u.function_index));
						inst = next;
					}
					break;
				case lywr_op_unused_0x12:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x13:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x14:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x15:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x16:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x17:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x18:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x19:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_drop:
					{
						unsigned int rv;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&rv,0));
						inst = next;
					}
					break;
				case lywr_op_select:
					{
						unsigned int c;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&c,0));

						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&b,0));
						unsigned long long a;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&a,0));

						if(c){
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,a));
						}else{
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,b));
						}

						inst = next;
					}
					break;
				case lywr_op_unused_0x1c:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x1d:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x1e:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x1f:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_get_local:
					{
						lywr_data_cell* data;
						LYWR_FUNCTION_VERIFY(lywr_data_array_at(&session->frame->local,inst->u.local_index,&data));
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,data->u64));
						TRACE("local.get [%u] = %d\n",inst->u.local_index,data->i32);
						inst = next;
					}
					break;
				case lywr_op_set_local:
					{
						lywr_data_cell* data;
						LYWR_FUNCTION_VERIFY(lywr_data_array_at(&session->frame->local,inst->u.local_index,&data));
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&data->u64,0));
						TRACE("local.set [%u] = %d\n",inst->u.local_index,data->i32);
						inst = next;
					}
					break;
				case lywr_op_tee_local:
					{
						lywr_data_cell* data;
						LYWR_FUNCTION_VERIFY(lywr_data_array_at(&session->frame->local,inst->u.local_index,&data));
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&data->u64,1));
						TRACE("local.tee [%u] = %d\n",inst->u.local_index,data->i32);
						inst = next;
					}
					break;
				case lywr_op_get_global:
					{
						unsigned long long i = inst->u.global_index;
						if(i >= mdl->globals_count){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						if(mdl->globals[i].expr.init_expr_type == lywr_op_i32_const){
							TRACE("global.get [%llu] = %d\n",i,mdl->globals[i].expr.u.u32);
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_u32(&session->stack,mdl->globals[i].expr.u.u32));
							inst = next;
						}else if(mdl->globals[i].expr.init_expr_type == lywr_op_i64_const){
							TRACE("global.get [%llu] = %lld\n",i,mdl->globals[i].expr.u.u64);
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,mdl->globals[i].expr.u.u64));
							inst = next;
						}else if(mdl->globals[i].expr.init_expr_type == lywr_op_f32_const){
							TRACE("global.get [%llu] = %f\n",i,mdl->globals[i].expr.u.f32);
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_f32(&session->stack,mdl->globals[i].expr.u.f32));
							inst = next;
						}else if(mdl->globals[i].expr.init_expr_type == lywr_op_f64_const){
							TRACE("global.get [%llu] = %f\n",i,mdl->globals[i].expr.u.f64);
							LYWR_FUNCTION_VERIFY(lywr_datastack_push_f64(&session->stack,mdl->globals[i].expr.u.f64));
							inst = next;
						}else if(mdl->globals[i].expr.init_expr_type == lywr_op_get_global){
							TRACE("global.get 错误，gloal中怎么又get global了？\n");
							LYWR_FUNCTION_VERIFY(lywrrc_bad_format);
							inst = next;
						}else{
							LYWR_FUNCTION_VERIFY(lywrrc_not_implement);
						}
					}
					break;
				case lywr_op_set_global:
					{
						unsigned long long i = inst->u.global_index;
						if(i >= mdl->globals_count){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						unsigned long long data;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&data,1));
						mdl->globals[i].expr.u.u64 = data;
						inst = next;
					}
					break;
				case lywr_op_unused_0x25:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x26:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0x27:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_load:
					{
						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						int value = *(int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);

						TRACE("i32_load memory[%u] = %d,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u32(&session->stack,(unsigned int)value));

						inst = next;
					}
					break;
				case lywr_op_i64_load:
					{
						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						long long value = *(long long*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);

						TRACE("i64_load memory[%u] = %lld,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,(unsigned long long)value));

						inst = next;
					}
					break;
				case lywr_op_f32_load:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_load:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_load8_s:
					{
						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						// TODO 这里在arm中可能会有问题需要调试后修改。
						unsigned int value = *(char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
						TRACE("i32_load8_s memory[%u] = %u,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u32(&session->stack,(unsigned int)value));
						inst = next;
					}
					break;
				case lywr_op_i32_load8_u:
					{
						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						unsigned int value = *(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
						TRACE("i32_load8_u memory[%u] = %u,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u32(&session->stack,(unsigned int)value));
						inst = next;
					}
					break;
				case lywr_op_i32_load16_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_load16_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_load8_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_load8_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_load16_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_load16_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_load32_s:
					{
						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						long long  value = *(int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
						TRACE("i64_load32_s memory[%u] = %lld,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,(unsigned long long )value));
						inst = next;

					}
					break;
				case lywr_op_i64_load32_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_store:
					{
						unsigned int value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&value,0));

						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						*(unsigned int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned int)value;

						TRACE("i32_store memory[%u] = %d,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						inst = next;
					}
					break;
				case lywr_op_i64_store:
					{
						unsigned long long value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&value,0));

						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						*(unsigned long long*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned long long)value;

						TRACE("i64_store memory[%u] = %llu,flags=%u\n",addr + inst->u.memory_immediate.offset,value,inst->u.memory_immediate.flags);
						inst = next;
					}
					break;
				case lywr_op_f32_store:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_store:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_store8:
					{
						unsigned int value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&value,0));

						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						*(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned char)value;

						TRACE("i32_store memory[%u] = %d,flags=%u\n",addr + inst->u.memory_immediate.offset,(unsigned char)value,inst->u.memory_immediate.flags);
						inst = next;
					}
					break;
				case lywr_op_i32_store16:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_store8:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_store16:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_store32:
					{
						unsigned long long value;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&value,0));

						unsigned int addr;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,&addr,0));

						if(addr > session->frame->memory->size){
							LYWR_FUNCTION_VERIFY(lywrrc_oom);
						}

						*(unsigned int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned int)value;

						TRACE("i64_store memory[%u] = %d,flags=%u\n",addr + inst->u.memory_immediate.offset,(unsigned int)value,inst->u.memory_immediate.flags);
						inst = next;
					}
					break;
				case lywr_op_memory_size:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_memory_grow:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_const:
					{
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u32(&session->stack,inst->u.u32));
						TRACE("i32_const %d\n",inst->u.u32);
						inst = next;
					}
					break;
				case lywr_op_i64_const:
					{
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,inst->u.u64));
						TRACE("i64_const %lld\n",inst->u.u64);
						inst = next;
					}
					break;
				case lywr_op_f32_const:
					{
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_f32(&session->stack,inst->u.f32));
						TRACE("f32_const %f\n",inst->u.f32);
						inst = next;
					}
					break;
				case lywr_op_f64_const:
					{
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_f64(&session->stack,inst->u.f64));
						TRACE("f64_const %f\n",inst->u.f64);
						inst = next;
					}
					break;
				case lywr_op_i32_eqz:
					{
						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 = 0 == cell->u32;

						inst = next;
					}
					break;
				case lywr_op_i32_eq:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 == b;

						inst = next;
					}
					break;
				case lywr_op_i32_ne:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 != b;

						inst = next;
					}
					break;
				case lywr_op_i32_lt_s:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 < b;

						inst = next;
					}
					break;
				case lywr_op_i32_lt_u:
					{
						unsigned int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 = cell->u32 < b;

						inst = next;
					}
					break;
				case lywr_op_i32_gt_s:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 > b;

						inst = next;
					}
					break;
				case lywr_op_i32_gt_u:
					{
						unsigned int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 = cell->u32 > b;

						inst = next;
					}
					break;
				case lywr_op_i32_le_s:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 <= b;

						inst = next;
					}
					break;
				case lywr_op_i32_le_u:
					{
						unsigned int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 = cell->u32 <= b;

						inst = next;
					}
					break;
				case lywr_op_i32_ge_s:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = cell->i32 >= b;

						inst = next;
					}
					break;
				case lywr_op_i32_ge_u:
					{
						unsigned int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 = cell->u32 >= b;

						inst = next;
					}
					break;
				case lywr_op_i64_eqz:
					{
						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 = 0 == cell->u64;

						inst = next;
					}
					break;
				case lywr_op_i64_eq:
					{
						long long  b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));
						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i64 = cell->i64 == b;

						inst = next;
					}
					break;
				case lywr_op_i64_ne:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i64 = cell->i64 != b;

						inst = next;
					}
					break;
				case lywr_op_i64_lt_s:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i64 = cell->i64 < b;

						inst = next;
					}
					break;
				case lywr_op_i64_lt_u:
					{
						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 = cell->u64 < b;

						inst = next;
					}
					break;
				case lywr_op_i64_gt_s:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i64 = cell->i64 > b;

						inst = next;
					}
					break;
				case lywr_op_i64_gt_u:
					{
						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 = cell->u64 > b;

						inst = next;
					}
					break;
				case lywr_op_i64_le_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_le_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_ge_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_ge_u:
					{
						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 = cell->u64 >= b;

						inst = next;
					}
					break;
				case lywr_op_f32_eq:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_ne:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_lt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_gt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_le:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_ge:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_eq:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_ne:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_lt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_gt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_le:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_ge:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_clz:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_ctz:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_popcnt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_add:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 += b;
						inst = next;
					}
					break;
				case lywr_op_i32_sub:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 -= b;
						inst = next;
					}
					break;
				case lywr_op_i32_mul:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 *= b;
						inst = next;
					}
					break;
				case lywr_op_i32_div_s:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 /= b;
						inst = next;
					}
					break;
				case lywr_op_i32_div_u:
					{
						unsigned int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 /= b;
						inst = next;
					}
					break;
				case lywr_op_i32_rem_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_rem_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_and:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 &= b;
						inst = next;
					}
					break;
				case lywr_op_i32_or:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 |= b;
						inst = next;
					}
					break;
				case lywr_op_i32_xor:
					{
						int b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u32 ^= b;
						inst = next;
					}
					break;
				case lywr_op_i32_shl:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_shr_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_shr_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_rotl:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_rotr:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_clz:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_ctz:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_popcnt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_add:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 += b;

						inst = next;
					}
					break;
				case lywr_op_i64_sub:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 -= b;

						inst = next;
					}
					break;
				case lywr_op_i64_mul:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 *= b;

						inst = next;
					}
					break;
				case lywr_op_i64_div_s:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i64 /= b;

						inst = next;
					}
					break;
				case lywr_op_i64_div_u:
					{
						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 /= b;

						inst = next;
					}
					break;
				case lywr_op_i64_rem_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_rem_u:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_and:
					{
						long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 &= b;

						inst = next;
					}
					break;
				case lywr_op_i64_or:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_xor:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_shl:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_shr_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_shr_u:
					{
						unsigned long long b;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0));

						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->u64 = cell->u64 >> (b%0x40);

						inst = next;
					}
					break;
				case lywr_op_i64_rotl:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_rotr:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_abs:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_neg:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_ceil:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_floor:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_trunc:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_nearest:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_sqrt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_add:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_sub:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_mul:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_div:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_min:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_max:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_copysign:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_abs:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_neg:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_ceil:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_floor:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_trunc:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_nearest:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_sqrt:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_add:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_sub:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_mul:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_div:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_min:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_max:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_copysign:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_wrap_i64:
					{
						lywr_data_cell* cell;
						LYWR_FUNCTION_VERIFY(lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell));
						cell->i32 = (int)cell->i64;

						inst = next;
					}
					break;
				case lywr_op_i32_trunc_s_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_trunc_u_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_trunc_s_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_trunc_u_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_extend_s_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_extend_u_i32:
					{
						unsigned int a;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0));
						unsigned long long c = (unsigned long long)a;
						LYWR_FUNCTION_VERIFY(lywr_datastack_push_u64(&session->stack,(unsigned long long)c));
						inst = next;
					}
					break;
				case lywr_op_i64_trunc_s_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_trunc_u_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_trunc_s_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_trunc_u_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_convert_s_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_convert_u_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_convert_s_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_convert_u_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_demote_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_convert_s_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_convert_u_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_convert_s_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_convert_u_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_promote_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_reinterpret_f32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_reinterpret_f64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f32_reinterpret_i32:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_f64_reinterpret_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_extend8_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i32_extend16_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_extend8_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_extend16_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_i64_extend32_s:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_drop_64:
					{
						unsigned long long rv;
						LYWR_FUNCTION_VERIFY(lywr_datastack_pop_u64(&session->stack,&rv,0));
						inst = next;
					}
					break;
				case lywr_op_select_64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_get_local_fast:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_set_local_fast_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_set_local_fast:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_tee_local_fast:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_tee_local_fast_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_copy_stack_top:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_ext_op_copy_stack_top_i64:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_impdep:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xcf:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd0:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd1:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd2:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd3:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd4:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd5:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd6:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd7:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd8:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xd9:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xda:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xdb:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xdc:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xdd:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xde:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xdf:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe0:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe1:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe2:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe3:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe4:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe5:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe6:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe7:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe8:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xe9:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xea:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xeb:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xec:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xed:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xee:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xef:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf0:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf1:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf2:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf3:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf4:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf5:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf6:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf7:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf8:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xf9:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xfa:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_unused_0xfb:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_misc_prefix:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_simd_prefix:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_thread_prefix:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				case lywr_op_reserved_prefix:
					{
						LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
						inst = next;
					}
					break;
				default:{
					TRACE("未知的错误码:%x\n",inst->code);
					LYWR_FUNCTION_VERIFY(dispatch[inst->code](ctx,mdl,session,inst,&next));
					inst = next;
				}
			}
		}

		TRACE("执行函数完毕\n");
		lywr_callframe_stack_prev(&session->frames,&session->frame);

		return lywrrc_ok;
	}else if(mdl->merge_function[index].type == lywr_wasm_resource_link_type_import){
		TRACE("执行导入函数全局id=%d\n",index);
		wasm_import_entry* entry = mdl->imports + mdl->merge_function[index].index;
		if(entry->kind != 0){
			TRACE("调用导入函数[%d]失败:类型不符\n",mdl->merge_function[index].index);
			return lywrrc_bad_format;
		}

		wasm_varuint32 typeindex = entry->function;
		wasm_func_type* ftype = mdl->types + typeindex;
		TRACE("调用导入函数:%.*s.%.*s,参数%d，返回%d\n",entry->module_len,entry->module_str,entry->field_len,entry->field_str,ftype->param_count,ftype->return_count);

		lywr_module* mdl_finded;
		{
			lywrrc rc = lywr_get_module_by_name(ctx,(const char*)entry->module_str,entry->module_len,&mdl_finded);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		lywr_symbol* symbol_finded;
		{
			lywrrc rc = lywr_get_symbol_by_name(ctx,mdl_finded,(const char*)entry->field_str,entry->field_len,&symbol_finded);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		TRACE("symbol_finded=%p\n",(void*)symbol_finded);

		lywr_function_spec spec;

		spec.mod = (const char*)entry->module_str;
		spec.modlen = entry->module_len;
		spec.field = (const char*)entry->field_str;
		spec.fieldlen = entry->field_len;

		spec.mdl = mdl_finded;
		spec.symbol = symbol_finded;


		spec.argc = ftype->spec.argc;
		spec.argv = ftype->spec.argv;
		spec.rcount = ftype->spec.rcount;
		spec.rval = ftype->spec.rval;
		spec.spec = ftype->spec.shortcut;

		for(wasm_varuint32 i=0;i<spec.argc;++i){
			TRACE("弹出外部调用参数:%d\n",i);
			lywrrc rc = lywr_datastack_pop_u64(&session->stack,&spec.argv[i].u64,0);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		{
			lywrrc rc = lywr_function_exec(ctx,&spec);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		TRACE("返回数量=%d\n",ftype->return_count);
		for(wasm_varuint32 i=0;i<ftype->return_count;++i){
			TRACE("需要压入返回值：%d\n",ftype->return_type[i]);

			lywrrc rc = lywr_datastack_push_u64(&session->stack,spec.rval[i].u64);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		{
			lywrrc rc = lywr_function_destory_spec(ctx,&spec);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		TRACE("调用导入函数:%d 完毕\n",mdl->merge_function[index].index);
		return lywrrc_ok;
	}
	return lywrrc_not_found;
}




lywrrc lywr_op_dispath_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	//static int calltimes = 0;
	//TRACE("当前指令:%x,总miss次数:%d\n",inst->code,++calltimes);
	__asm("int $3");
	return dispatch2[inst->code](ctx,mdl,session,inst,next);
}

lywrrc lywr_op_reserve_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("未实现指令:%x,%s\n",inst->code,lywr_opcode_name(inst->code));
	return lywrrc_not_implement;
}

lywrrc lywr_op_unreachable_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("未实现指令:%x,%s\n",inst->code,lywr_opcode_name(inst->code));
	return lywrrc_trap;
}

lywrrc lywr_op_nop_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_block_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	if(inst->u.block->_then){
		TRACE("block\n");
		*next = inst->u.block->_then;
	}else{
		TRACE("block出错\n");
		return lywrrc_trap;
	}
	return lywr_instruction_stack_push(&session->frame->runblock,&inst);
}

lywrrc lywr_op_loop_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	if(inst->u.block->_then){
		TRACE("loop\n");
		*next = inst->u.block->_then;
	}else{
		TRACE("loop出错\n");
		return lywrrc_trap;
	}
	return lywr_instruction_stack_push(&session->frame->runblock,&inst);
}

lywrrc lywr_op_if_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int value;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&value,0);
	if(rc != lywrrc_ok){
		TRACE("if出错1\n");
		return rc;
	}
	if(value != 0 && inst->u.block->_then){
		TRACE("if true\n");
		*next = inst->u.block->_then;
	}else if(value == 0 && inst->u.block->_else){
		TRACE("if false\n");
		*next = inst->u.block->_else;
	}else{
		TRACE("if pass\n");
		*next = inst->next;
		return lywrrc_ok;
	}

	return lywr_instruction_stack_push(&session->frame->runblock,&inst);
}

lywrrc lywr_op_else_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	//else的功能在解析指令和if中已经实现了。
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_end_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	const lywr_instruction* parent;
	lywrrc rc = lywr_instruction_stack_pop(&session->frame->runblock,&parent,0);
	if(rc == lywrrc_oom) return lywrrc_eof;
	if(rc == lywrrc_ok){
		*next = parent->next;
		return lywrrc_ok;
	}
	return rc;
}

lywrrc lywr_op_br_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("br退出,层级=%d\n",inst->u.relative_depth);
	const lywr_instruction* parent = nullptr;
	for(int i=0;i<=inst->u.relative_depth;++i){
		TRACE("退出一层，%d\n",i);
		lywrrc rc = lywr_instruction_stack_pop(&session->frame->runblock,&parent,0);
		if(rc != lywrrc_ok) return rc;

		if(parent->code == lywr_op_loop){
			TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
			*next = parent->u.block->_then;
		}else{
			TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
			*next = parent->next;
		}
	}
	if(parent && parent->code == lywr_op_loop){
		return lywr_instruction_stack_push(&session->frame->runblock,&parent);
	}
	return lywrrc_ok;
}

lywrrc lywr_op_br_if_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int value;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&value,0);
	if(rc != lywrrc_ok){
		TRACE("br_if出错1\n");
		return rc;
	}
	if(value != 0){
		TRACE("br_if退出,层级=%d\n",inst->u.relative_depth);
		const lywr_instruction* parent = nullptr;
		for(int i=0;i<=inst->u.relative_depth;++i){
			TRACE("退出一层，%d\n",i);
			lywrrc rc = lywr_instruction_stack_pop(&session->frame->runblock,&parent,0);
			if(rc != lywrrc_ok) return rc;

			if(parent->code == lywr_op_loop){
				TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
				*next = parent->u.block->_then;
			}else{
				TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
				*next = parent->next;
			}
		}
		if(parent && parent->code == lywr_op_loop){
			lywr_instruction_stack_push(&session->frame->runblock,&parent);
		}
	}else{
		TRACE("br_if跳过\n");
	}
	return lywrrc_ok;
}

lywrrc lywr_op_br_table_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int deep = inst->u.br_table->default_target;

	int index;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&index,0);
	if(rc != lywrrc_ok){
		TRACE("br_table出错\n");
		return rc;
	}

	if(index > 0 && index < inst->u.br_table->target_count){
		deep = inst->u.br_table->target_table[index];
	}

	const lywr_instruction* parent = nullptr;
	for(int i=0;i<=deep;++i){
		TRACE("退出一层，%d\n",i);
		lywrrc rc = lywr_instruction_stack_pop(&session->frame->runblock,&parent,0);
		if(rc != lywrrc_ok) return rc;

		if(parent->code == lywr_op_loop){
			TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
			*next = parent->u.block->_then;
		}else{
			TRACE("pop出来的指令是:%s\n",lywr_opcode_name(parent->code));
			*next = parent->next;
		}
	}
	if(parent && parent->code == lywr_op_loop){
		lywr_instruction_stack_push(&session->frame->runblock,&parent);
	}

	return lywrrc_ok;
}

lywrrc lywr_op_return_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywrrc_eof;
}

lywrrc lywr_op_call_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("[执行] call %u\n",inst->u.function_index);
	return lywr_module_call_function_by_index(ctx,mdl,session,inst->u.function_index);
}

lywrrc lywr_op_call_indirect_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int index;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&index,0);
	if(rc != lywrrc_ok) {
		TRACE("出错了，错误码是:%d\n",rc);
		return rc;
	}
	//wasm_varuint32 typeindex = inst->u.call_indirect_extend.type_index;

	TRACE("间接调用函数%d\n",index);
	if(mdl->merge_table[0].type != lywr_wasm_resource_link_type_local){
		TRACE("出错了，call_indirect调用了一个导入函数\n");
		return lywrrc_bad_format;
	}

	wasm_table_type* tab = &mdl->tables[mdl->merge_table[0].index];
	wasm_varuint32 functionindex = tab->element_list[index];
	TRACE("读到间接函数索引%d\n",functionindex);

	lywr_instruction tmpinst;
	tmpinst.code = lywr_op_call;
	tmpinst.u.function_index = functionindex;
	tmpinst.next = inst->next;

	inst = &tmpinst;

	return lywr_op_call_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_drop_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long rv;
	return lywr_datastack_pop_u64(&session->stack,&rv,0);
}

lywrrc lywr_op_select_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int c;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&c,0);
	if(rc != lywrrc_ok) {
		TRACE("出错了，错误码是:%d\n",rc);
		return rc;
	}


	unsigned long long b;
	rc = lywr_datastack_pop_u64(&session->stack,&b,0);
	if(rc != lywrrc_ok) {
		TRACE("出错了，错误码是:%d\n",rc);
		return rc;
	}
	unsigned long long a;
	rc = lywr_datastack_pop_u64(&session->stack,&a,0);
	if(rc != lywrrc_ok) {
		TRACE("出错了，错误码是:%d\n",rc);
		return rc;
	}

	if(c){
		return lywr_datastack_push_u64(&session->stack,a);
	}else{
		return lywr_datastack_push_u64(&session->stack,b);
	}
}

lywrrc lywr_op_get_local_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	lywr_data_cell* data;
	lywrrc rc = lywr_data_array_at(&session->frame->local,inst->u.local_index,&data);
	TRACE("get_local[%d]=%d,%lld\n",inst->u.local_index,data->i32,data->i64);
	if(rc != lywrrc_ok) return rc;

	return lywr_datastack_push_u64(&session->stack,data->u64);
}

lywrrc lywr_op_set_local_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	lywr_data_cell* data;
	lywrrc rc = lywr_data_array_at(&session->frame->local,inst->u.local_index,&data);
	TRACE("set_local[%d]=%d,%lld\n",inst->u.local_index,data->i32,data->i64);
	if(rc != lywrrc_ok) return rc;

	return lywr_datastack_pop_u64(&session->stack,&data->u64,0);
}

lywrrc lywr_op_tee_local_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	lywr_data_cell* data;
	lywrrc rc = lywr_data_array_at(&session->frame->local,inst->u.local_index,&data);
	TRACE("tee_local[%d]=%d,%lld\n",inst->u.local_index,data->i32,data->i64);
	if(rc != lywrrc_ok) return rc;

	return lywr_datastack_pop_u64(&session->stack,&data->u64,1);
}

lywrrc lywr_op_get_global_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long i = inst->u.global_index;
	if(i >= mdl->globals_count) return lywrrc_oom;

	if(mdl->globals[i].expr.init_expr_type == lywr_op_i32_const){
		TRACE("global.get [%llu] = %d\n",i,mdl->globals[i].expr.u.u32);
		return lywr_datastack_push_u32(&session->stack,mdl->globals[i].expr.u.u32);
	}else if(mdl->globals[i].expr.init_expr_type == lywr_op_i64_const){
		TRACE("global.get [%llu] = %lld\n",i,mdl->globals[i].expr.u.u64);
		return lywr_datastack_push_u64(&session->stack,mdl->globals[i].expr.u.u64);
	}else if(mdl->globals[i].expr.init_expr_type == lywr_op_f32_const){
		TRACE("global.get [%llu] = %f\n",i,mdl->globals[i].expr.u.f32);
		return lywr_datastack_push_f32(&session->stack,mdl->globals[i].expr.u.f32);
	}else if(mdl->globals[i].expr.init_expr_type == lywr_op_f64_const){
		TRACE("global.get [%llu] = %f\n",i,mdl->globals[i].expr.u.f64);
		return lywr_datastack_push_f64(&session->stack,mdl->globals[i].expr.u.f64);
	}else if(mdl->globals[i].expr.init_expr_type == lywr_op_get_global){
		TRACE("global.get 错误，gloal中怎么又get global了？\n");
		//TODO();

		return lywrrc_bad_format;
	}
	return lywrrc_not_implement;
}

lywrrc lywr_op_set_global_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long i = inst->u.global_index;
	if(i >= mdl->globals_count) return lywrrc_oom;

	unsigned long long data;
	lywrrc rc = lywr_datastack_pop_u64(&session->stack,&data,1);
	if(rc != lywrrc_ok) return rc;

	mdl->globals[i].expr.u.u64 = data;
	return lywrrc_ok;
}

lywrrc lywr_op_i32_load_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	int value = *(int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);

	TRACE("i32_load memory[%u] = %d,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)value);
}

lywrrc lywr_op_i64_load_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	long long value = *(long long*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);

	TRACE("i64_load memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_f32_load_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_load_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_load8_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	int value = *(char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i32_load8_s memory[%u] = %d,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)value);
}

lywrrc lywr_op_i32_load8_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	unsigned int value = *(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i32_load8_u memory[%u] = %u,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)value);
}

lywrrc lywr_op_i32_load16_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	unsigned int value = *(short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i32_load16_s memory[%u] = %u,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)value);
}

lywrrc lywr_op_i32_load16_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	unsigned int value = *(unsigned short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i32_load16_u memory[%u] = %u,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)value);
}

lywrrc lywr_op_i64_load8_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	unsigned long long value = *(char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load8_s memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i64_load8_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	unsigned long long value = *(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load8_u memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i64_load16_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	long long value = *(short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load16_s memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i64_load16_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	long long value = *(unsigned short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load16_u memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i64_load32_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	long long value = *(int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load32_s memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i64_load32_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int addr;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	long long value = *(unsigned int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset);
	TRACE("i64_load32_s memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)value);
}

lywrrc lywr_op_i32_store_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int value;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;

	*(unsigned int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned int)value;

	TRACE("i32_store memory[%u] = %d,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i64_store_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long value;
	lywrrc rc = lywr_datastack_pop_u64(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;



	if(addr > session->frame->memory->size) return lywrrc_oom;

	*(unsigned long long*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned long long)value;

	TRACE("i64_store memory[%u] = %lld,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_f32_store_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	float value;
	lywrrc rc = lywr_datastack_pop_f32(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(float*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (float)value;

	TRACE("f32_store memory[%u] = %f,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_f64_store_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	double value;
	lywrrc rc = lywr_datastack_pop_f64(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(double*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (double)value;

	TRACE("f64_store memory[%u] = %f,flags=%u\n",addr,value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i32_store8_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int value;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned char)value;

	TRACE("i32_store memory[%u] = %d,flags=%u\n",addr,(unsigned char)value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i32_store16_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int value;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(unsigned short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned short)value;

	TRACE("i32_store memory[%u] = %d,flags=%u\n",addr,(unsigned short)value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i64_store8_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long value;
	lywrrc rc = lywr_datastack_pop_u64(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(unsigned char*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned char)value;

	TRACE("i64_store memory[%u] = %d,flags=%u\n",addr,(unsigned char)value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i64_store16_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long value;
	lywrrc rc = lywr_datastack_pop_u64(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;


	*(unsigned short*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned short)value;

	TRACE("i64_store memory[%u] = %d,flags=%u\n",addr,(unsigned short)value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_i64_store32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long value;
	lywrrc rc = lywr_datastack_pop_u64(&session->stack,&value,0);
	if(rc != lywrrc_ok) return rc;


	unsigned int addr;
	rc = lywr_datastack_pop_u32(&session->stack,&addr,0);
	if(rc != lywrrc_ok) return rc;

	if(addr > session->frame->memory->size) return lywrrc_oom;




	*(unsigned int*)(session->frame->memory->start + addr + inst->u.memory_immediate.offset) = (unsigned int)value;

	TRACE("i64_store memory[%u] = %d,flags=%u\n",addr,(unsigned int)value,inst->u.memory_immediate.flags);
	return lywrrc_ok;
}

lywrrc lywr_op_memory_size_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_memory_grow_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_const_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("i32_const %d\n",inst->u.i32);
	return lywr_datastack_push_u32(&session->stack,inst->u.u32);
}

lywrrc lywr_op_i64_const_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("i64_const %lld\n",inst->u.i64);
	return lywr_datastack_push_u64(&session->stack,inst->u.u64);
}

lywrrc lywr_op_f32_const_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("f32_const %f\n",inst->u.f32);
	return lywr_datastack_push_f32(&session->stack,inst->u.f32);
}

lywrrc lywr_op_f64_const_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	TRACE("f64_const %f\n",inst->u.f64);
	return lywr_datastack_push_f64(&session->stack,inst->u.f64);
}

lywrrc lywr_op_i32_eqz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int data;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&data,0);
	if(rc != lywrrc_ok) return rc;

	unsigned int z = (data == 0)?1:0;

	return lywr_datastack_push_u32(&session->stack,z);
}

lywrrc lywr_op_i32_eq_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a == b;
	return lywr_datastack_push_u32(&session->stack,c);
}

lywrrc lywr_op_i32_ne_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a != b;
	return lywr_datastack_push_u32(&session->stack,c);
}

lywrrc lywr_op_i32_lt_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a < b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_lt_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a < b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_gt_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a > b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_gt_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a > b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_le_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a <= b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_le_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a <= b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_ge_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a >= b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_ge_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a >= b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i64_eqz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = 0 == b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_eq_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a == b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_ne_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a != b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_lt_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a < b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_lt_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a < b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_gt_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a > b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_gt_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a > b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_le_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a <= b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_le_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a <= b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_ge_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a >= b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_ge_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a >= b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_f32_eq_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_ne_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_lt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_gt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_le_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_ge_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_eq_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_ne_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_lt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_gt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_le_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_ge_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_clz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_ctz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_popcnt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_add_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a + b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_sub_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a - b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_mul_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a * b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_div_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a / b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_div_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a / b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_rem_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_rem_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_and_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a & b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_or_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a | b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_xor_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a ^ b;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_shl_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a << (b%0x20);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_shr_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int c = a >> (b%0x20);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_shr_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int b;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned int a;
	{
		lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned int c = a >> (b%0x20);
	return lywr_datastack_push_u32(&session->stack,(unsigned int)c);
}

lywrrc lywr_op_i32_rotl_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_rotr_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_clz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_ctz_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_popcnt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_add_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}


	lywr_data_cell* cell;
	lywrrc rc = lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell);
	if(rc != lywrrc_ok) return rc;

	cell->u64 += b;

	return lywrrc_ok;
}

lywrrc lywr_op_i64_sub_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}


	lywr_data_cell* cell;
	lywrrc rc = lywr_datastack_at(&session->stack,session->stack.cursor - 1,&cell);
	if(rc != lywrrc_ok) return rc;

	cell->u64 -= b;
	return lywrrc_ok;
}

lywrrc lywr_op_i64_mul_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a * b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_div_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a / b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_div_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a / b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_rem_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_rem_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_and_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a & b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_or_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a | b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_xor_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	long long c = a ^ b;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_shl_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a << (b%0x40);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_shr_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a >> (b%0x40);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_shr_u_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned long long b;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&b,0);
		if(rc != lywrrc_ok) return rc;
	}
	unsigned long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	unsigned long long c = a >> (b%0x40);
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);
}

lywrrc lywr_op_i64_rotl_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_rotr_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_abs_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_neg_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_ceil_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_floor_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_trunc_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_nearest_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_sqrt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_add_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_sub_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_mul_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_div_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_min_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_max_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_copysign_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_abs_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_neg_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_ceil_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_floor_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_trunc_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_nearest_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_sqrt_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_add_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_sub_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_mul_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_div_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_min_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_max_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_copysign_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_wrap_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	long long a;
	{
		lywrrc rc = lywr_datastack_pop_u64(&session->stack,(unsigned long long*)&a,0);
		if(rc != lywrrc_ok) return rc;
	}

	int b = (int)a;
	return lywr_datastack_push_u32(&session->stack,(unsigned int)b);
}

lywrrc lywr_op_i32_trunc_s_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_trunc_u_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_trunc_s_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_trunc_u_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_extend_s_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_extend_u_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	unsigned int a;
	lywrrc rc = lywr_datastack_pop_u32(&session->stack,(unsigned int*)&a,0);
	if(rc != lywrrc_ok) return rc;

	unsigned long long c = (unsigned long long)a;
	return lywr_datastack_push_u64(&session->stack,(unsigned long long)c);

}

lywrrc lywr_op_i64_trunc_s_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_trunc_u_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_trunc_s_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_trunc_u_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_convert_s_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_convert_u_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_convert_s_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_convert_u_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_demote_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_convert_s_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_convert_u_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_convert_s_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_convert_u_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_promote_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_reinterpret_f32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_reinterpret_f64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f32_reinterpret_i32_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_f64_reinterpret_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_extend8_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i32_extend16_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_extend8_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_extend16_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_i64_extend32_s_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_drop_64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_select_64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_get_local_fast_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_set_local_fast_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_set_local_fast_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_tee_local_fast_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_tee_local_fast_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_copy_stack_top_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_ext_op_copy_stack_top_i64_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_impdep_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_misc_prefix_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_simd_prefix_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_thread_prefix_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

lywrrc lywr_op_reserved_prefix_callback(lywr_ctx* ctx,lywr_module* mdl,lywr_session* session,const lywr_instruction* inst,const lywr_instruction** next)
{
	return lywr_op_reserve_callback(ctx,mdl,session,inst,next);
}

LYWR_NAMESPACE_END



