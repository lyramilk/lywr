#include "lywr.hh"
#include "lywr_util.hh"
#include "lywr_data.hh"
/*
#include "lywr_wasm_types.hh"
#include "lywr_stream.hh"
#include "lywr_expr.hh"
*/
#define nullptr (void*)0

LYWR_NAMESPACE_START


int lywr_symbol_cmpr(const void* a,const void* b)
{
	lywr_symbol* sa = (lywr_symbol*)a;
	lywr_symbol* sb = (lywr_symbol*)b;

	return lywr_memcmp2((unsigned char*)sa->fieldname,sa->namelength,(const unsigned char*)sb->fieldname,sb->namelength);
}


lywrrc LYWR_EXPORT lywr_function_init_spec(lywr_ctx* ctx,lywr_function_spec* spec)
{
	{
		lywrrc rc = lywr_get_module_by_name(ctx,spec->mod,spec->modlen,&spec->mdl);
		if(rc != lywrrc_ok) {
			TRACE("出错了，错误码是:%d\n",rc);
			return rc;
		}
	}

	{
		lywrrc rc = lywr_get_symbol_by_name(ctx,spec->mdl,spec->field,spec->fieldlen,&spec->symbol);
		if(rc != lywrrc_ok) {
			TRACE("出错了，错误码是:%d\n",rc);
			return rc;
		}
	}



	if(spec->symbol->type == lywr_module_type_native){
		TRACE("暂不支持预读native函数的参数类型%.*s.%.*s\n",spec->modlen,spec->mod,spec->fieldlen,spec->field);
		return lywrrc_param;
	}

	if(spec->symbol->type == lywr_module_type_wasm){

		if(spec->mdl->merge_function[spec->symbol->entry->index].type == lywr_wasm_resource_link_type_local){
			wasm_varuint32 functionindex = spec->mdl->merge_function[spec->symbol->entry->index].index;
			wasm_varuint32 typeindex = spec->mdl->functions[functionindex];
			wasm_func_type* ftype = spec->mdl->types + typeindex;

			spec->argc = ftype->spec.argc;
			spec->argv = ftype->spec.argv;
			spec->rcount = ftype->spec.rcount;
			spec->rval = ftype->spec.rval;
			spec->spec = ftype->spec.shortcut;
		}

		/*
		if(spec->mdl->merge_function[spec->symbol->entry->index].type == lywr_wasm_resource_link_type_local){
			wasm_varuint32 functionindex = spec->mdl->merge_function[spec->symbol->entry->index].index;
			wasm_varuint32 typeindex = spec->mdl->functions[functionindex];
			wasm_func_type* ftype = spec->mdl->types + typeindex;

			spec->argc = ftype->param_count;
			spec->argv = lywr_pool_malloc(ctx->mloader,sizeof(lywr_data) * ftype->param_count);
			if(spec->argv == nullptr) return lywrrc_oom;
			spec->rcount = ftype->return_count;
			spec->rval = lywr_pool_malloc(ctx->mloader,sizeof(lywr_data) * ftype->return_count);
			if(spec->rval == nullptr) return lywrrc_oom;

			for(wasm_varuint32 i=0;i<ftype->param_count;++i){
				if(ftype->param_types[i] == wasm_value_type_i32){
					spec->argv[i].type = lywr_data_i32;
				}else if(ftype->param_types[i] == wasm_value_type_i64){
					spec->argv[i].type = lywr_data_i64;
				}else if(ftype->param_types[i] == wasm_value_type_f32){
					spec->argv[i].type = lywr_data_f32;
				}else if(ftype->param_types[i] == wasm_value_type_f64){
					spec->argv[i].type = lywr_data_f64;
				}
			}

			for(wasm_varuint32 i=0;i<ftype->return_count;++i){
				if(ftype->return_type[i] == wasm_value_type_i32){
					spec->rval[i].type = lywr_data_i32;
				}else if(ftype->return_type[i] == wasm_value_type_i64){
					spec->rval[i].type = lywr_data_i64;
				}else if(ftype->return_type[i] == wasm_value_type_f32){
					spec->rval[i].type = lywr_data_f32;
				}else if(ftype->return_type[i] == wasm_value_type_f64){
					spec->rval[i].type = lywr_data_f64;
				}
			}
		}*/
	}


	return lywrrc_ok;
}

lywrrc LYWR_EXPORT lywr_function_destory_spec(lywr_ctx* ctx,lywr_function_spec* spec)
{
	return lywrrc_ok;
}


lywrrc LYWR_EXPORT lywr_function_exec(lywr_ctx* ctx,lywr_function_spec* spec)
{
	lywr_module* mdl = spec->mdl;
	lywr_symbol* symbol_finded = spec->symbol;

	if(symbol_finded == nullptr) return lywrrc_not_found;

	if(symbol_finded->type == lywr_module_type_native){
		TRACE("执行native函数%.*s.%.*s\n",spec->modlen,spec->mod,spec->fieldlen,spec->field);
		return symbol_finded->invoker(ctx,spec);
	}

	if(symbol_finded->type == lywr_module_type_wasm){
		TRACE("执行wasm函数%.*s.%.*s\n",spec->modlen,spec->mod,spec->fieldlen,spec->field);
		lywrrc rc;

		lywr_session session;
		session.mempool = lywr_pool_create(ctx->pmalloc,ctx->pfree,ctx->uptr);
		lywr_datastack_init(&session.stack,session.mempool);
		lywr_callframe_stack_init(&session.frames,session.mempool);

		do{
			if(mdl->merge_memory_count > 0){
				session.memory = lywr_pool_malloc(session.mempool,sizeof(*session.memory) * mdl->merge_memory_count);
				if(session.memory == nullptr) return lywrrc_oom;
				TRACE("分配了memory:%d\n",mdl->merge_memory_count);
			}else{
				session.memory = nullptr;
			}

			for(unsigned int i=0;i<mdl->merge_memory_count;++i){
				// 导入其它模块的内存
				TRACE("导入其它模块的内存 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
				if(mdl->merge_memory[i].type == lywr_wasm_resource_link_type_local){
					TRACE("导入其它模块的内存 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
					lywr_linearmemory* m = lywr_pool_malloc(session.mempool,sizeof(**session.memory));
					if(m == nullptr) return lywrrc_oom;
					lywr_linearmemory_init(m,session.mempool,mdl->memorys[mdl->merge_memory[i].index].limits.initial,mdl->memorys[mdl->merge_memory[i].index].limits.maximum);
					session.memory[i] = m;
				}else if(mdl->merge_memory[i].type == lywr_wasm_resource_link_type_import){
					TRACE("导入外部模块的内存 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
					lywr_linearmemory* m = lywr_pool_malloc(session.mempool,sizeof(**session.memory));
					if(m == nullptr) return lywrrc_oom;
					lywr_linearmemory_init(m,session.mempool,16,16);
					session.memory[i] = m;
				}
			}

			for(wasm_varuint32 i=0;i<mdl->datas_count;++i){
				wasm_data_segment* ds = mdl->datas + i;
				if(mdl->merge_memory[mdl->datas[i].index].type == lywr_wasm_resource_link_type_local){
					lywr_linearmemory* m = session.memory[mdl->merge_memory[mdl->datas[i].index].index];
					long long offset = 0;
					rc = lywr_get_i64_init_expr(ctx,mdl,&ds->offset,&offset);
					if(rc != lywrrc_ok) {
						TRACE("出错了，错误码是:%d\n",rc);
						break;
					}
					unsigned char* p = m->start + offset;

					wasm_byte* data;
					rc = lywr_get_lazy_data_as_bytes(ctx,mdl,&ds->data,&data);
					if(rc != lywrrc_ok) {
						TRACE("出错了，错误码是:%d\n",rc);
						break;
					}

					TRACE("向线性内存%lld写入了%lld个字节:%.*s\n",offset,ds->size,(int)ds->size,data);
					lywr_memcpy(p,data,ds->size);
				}else{
					//暂不支持通过data段向导入的内存块写数据。
					rc = lywrrc_bad_format;
					break;
				}
			}





			TRACE("symbol_finded->entry=%p\n",(void*)symbol_finded->entry);
			TRACE("在全局范围找到结点:%.*s，kind类型:%d\n",symbol_finded->namelength,symbol_finded->fieldname,symbol_finded->entry->kind);

			if(symbol_finded->entry->kind == 0){
				//Function
				TRACE("转换索引:%s %d\n",mdl->merge_function[symbol_finded->entry->index].type == lywr_wasm_resource_link_type_import ? "import" : "local",mdl->merge_function[symbol_finded->entry->index].index);
				wasm_varuint32 functionindex = mdl->merge_function[symbol_finded->entry->index].index;
				wasm_varuint32 typeindex = mdl->functions[functionindex];
				wasm_func_type* ftype = mdl->types + typeindex;

				if(mdl->merge_function[symbol_finded->entry->index].type == lywr_wasm_resource_link_type_local){
					for(wasm_varuint32 i=0;i<ftype->param_count;++i){
						rc = lywr_datastack_push_u64(&session.stack,spec->argv[i].u64);
						if(rc != lywrrc_ok) goto label_final;
					}

					rc = lywr_module_call_function_by_index(ctx,mdl,&session,symbol_finded->entry->index);

					if(rc != lywrrc_ok) {
						TRACE("出错了，错误码是:%d\n",rc);
						break;
					}
					TRACE("返回数量=%d\n",ftype->return_count);
					for(wasm_varuint32 i=0;i<ftype->return_count;++i){
						TRACE("需要压入返回值：%d\n",ftype->return_type[i]);

						if(ftype->return_type[i] == wasm_value_type_i32){
							if(spec->rval[i].type != lywr_data_i32){
								TRACE("参数不匹配\n");
								rc = lywrrc_param;
								goto label_final;
							}
							TRACE("模拟压入外部调用返回值i32\n");
							rc = lywr_datastack_pop_u32(&session.stack,&spec->rval[i].u32,0);
							if(rc != lywrrc_ok) {
								TRACE("出错了，错误码是:%d\n",rc);
								goto label_final;
							}
						}else if(ftype->return_type[i] == wasm_value_type_i64){
							if(spec->rval[i].type != lywr_data_i64){
								TRACE("参数不匹配:应该是i64结果是%d\n",spec->rval[i].type);
								rc = lywrrc_param;
								goto label_final;
							}
							TRACE("模拟压入外部调用返回值i64\n");
							rc = lywr_datastack_pop_u64(&session.stack,&spec->rval[i].u64,0);
							if(rc != lywrrc_ok) {
								TRACE("出错了，错误码是:%d\n",rc);
								goto label_final;
							}
						}else if(ftype->return_type[i] == wasm_value_type_f32){
							if(spec->rval[i].type != lywr_data_f32){
								TRACE("参数不匹配\n");
								rc = lywrrc_param;
								goto label_final;
							}
							TRACE("模拟压入外部调用返回值f32\n");
							lywrrc rc = lywr_datastack_pop_f32(&session.stack,&spec->rval[i].f32,0);
							if(rc != lywrrc_ok) {
								TRACE("出错了，错误码是:%d\n",rc);
								goto label_final;
							}
						}else if(ftype->return_type[i] == wasm_value_type_f64){
							if(spec->rval[i].type != lywr_data_f64){
								TRACE("参数不匹配\n");
								rc = lywrrc_param;
								goto label_final;
							}
							TRACE("模拟压入外部调用返回值f64\n");
							lywrrc rc = lywr_datastack_pop_f64(&session.stack,&spec->rval[i].f64,0);
							if(rc != lywrrc_ok) {
								TRACE("出错了，错误码是:%d\n",rc);
								goto label_final;
							}
						}
					}





				}
			}else if(symbol_finded->entry->kind == 1){
				//Table 
				rc = lywrrc_not_found;
				break;
			}else if(symbol_finded->entry->kind == 2){
				//Memory 
				rc = lywrrc_not_found;
				break;
			}else if(symbol_finded->entry->kind == 3){
				//Global 
				rc = lywrrc_not_found;
				break;
			}
		}while(0);
label_final:
		lywr_callframe_stack_destory(&session.frames);
		lywr_datastack_destory(&session.stack);
		lywr_pool_destory(session.mempool);
		return rc;
	}

	TRACE("试图调用未知函数%.*s.%.*s\n",spec->modlen,spec->mod,spec->fieldlen,spec->field);
	return lywrrc_not_found;
}


lywrrc LYWR_EXPORT lywr_native_module_init(lywr_ctx* ctx,const char* modulename,int modulenamelength,lywr_module** pmdl)
{
	lywr_poolx* mloader = lywr_poolx_create(ctx->pmalloc,ctx->pfree,ctx->uptr);
	if(mloader == nullptr) return lywrrc_oom;

	lywr_module* mdl = lywr_poolx_malloc(mloader,sizeof(lywr_module));
	if(mdl == nullptr) return lywrrc_oom;

	mdl->type = lywr_module_type_native;
	mdl->mloader = mloader;
	*pmdl = mdl;

	mdl->name = lywr_poolx_malloc(mloader,modulenamelength);
	if(mdl->name == nullptr) return lywrrc_oom;

	mdl->namelength = modulenamelength;
	mdl->symbol_table.compator = lywr_symbol_cmpr;
	lywr_rbtree2_init(&mdl->symbol_table,ctx->mloader);

	lywr_memcpy((unsigned char*)mdl->name,(unsigned char*)modulename,modulenamelength);

	{
		lywr_module* oldmodule;
		enum lywr_rbtree2_ec rc = lywr_rbtree2_insert(&ctx->symbol_module_table,mdl,(const void**)&oldmodule);
		if(rc == lywr_rbtree2_update){
			lywr_rbtree2_destory(&oldmodule->symbol_table);
			lywr_poolx_destory(oldmodule->mloader);
		}else if(rc != lywr_rbtree2_ok){
			return lywrrc_rbtree;
		}
	}

	return lywrrc_ok;
}
lywrrc LYWR_EXPORT lywr_native_module_function(lywr_ctx* ctx,lywr_module* mdl,const char* funcname,int funcnamelength,lywr_native_function_cbk cbk)
{
	lywr_symbol* newnode = lywr_poolx_malloc(mdl->mloader,sizeof(lywr_symbol));
	if(newnode == nullptr) return lywrrc_oom;


	newnode->type = lywr_module_type_native;
	newnode->namelength = funcnamelength;
	newnode->fieldname = lywr_poolx_malloc(mdl->mloader,funcnamelength);
	if(newnode->fieldname == nullptr) return lywrrc_oom;

	lywr_memcpy(newnode->fieldname,(const unsigned char*)funcname,funcnamelength);
	newnode->invoker = cbk;

	{
		lywr_symbol* oldmodule;
		enum lywr_rbtree2_ec rc = lywr_rbtree2_insert(&mdl->symbol_table,newnode,(const void**)&oldmodule);
		if(rc == lywr_rbtree2_update){
			//lywr_poolx_free(oldmodule);
		}else if(rc != lywr_rbtree2_ok){
			return lywrrc_rbtree;
		}
	}


	TRACE("向native模块%.*s注入函数%.*s\n",mdl->namelength,mdl->name,newnode->namelength,newnode->fieldname);
	return lywrrc_ok;
}




LYWR_NAMESPACE_END
