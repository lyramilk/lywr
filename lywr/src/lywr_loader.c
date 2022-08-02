#include "lywr.hh"
#include "lywr_wasm_types.hh"
#include "lywr_stream.hh"
#include "lywr_util.hh"
#include "lywr_data.hh"
#include "lywr_expr.hh"

#define nullptr (void*)0

LYWR_NAMESPACE_START

#define LYWRRC_VERIFY(x) {lywrrc rc = x;if(rc != lywrrc_ok) {TRACE("出错了，错误码是:%d\n",rc);return rc;}}



static int lywr_module_cmpr(const void* a,const void* b)
{
	lywr_module* sa = (lywr_module*)a;
	lywr_module* sb = (lywr_module*)b;

	return lywr_memcmp2((unsigned char*)sa->name,sa->namelength,(const unsigned char*)sb->name,sb->namelength);
}

static int lywr_symbol_cmpr(const void* a,const void* b)
{
	lywr_symbol* sa = (lywr_symbol*)a;
	lywr_symbol* sb = (lywr_symbol*)b;

	return lywr_memcmp2((unsigned char*)sa->fieldname,sa->namelength,(const unsigned char*)sb->fieldname,sb->namelength);
}


lywrrc lywr_init(lywr_ctx* ctx,lywr_malloc_cbk al,lywr_free_cbk fr,void* userdata)
{
	ctx->uptr = userdata;
	ctx->pmalloc = al;
	ctx->pfree = fr;
	ctx->mloader = lywr_pool_create(ctx->pmalloc,ctx->pfree,ctx->uptr);

	ctx->symbol_module_table.compator = lywr_module_cmpr;
	lywr_rbtree2_init(&ctx->symbol_module_table,ctx->mloader);

	return lywrrc_ok;
}

void lywr_free_instruction(lywr_pool* pool,lywr_instruction* cr)
{
	while(cr){
		lywr_instruction* nx = cr->next;

		if(cr->code == lywr_op_block || cr->code == lywr_op_loop || cr->code == lywr_op_if){
			if(cr->u.block->_then){
				lywr_free_instruction(pool,cr->u.block->_then);
			}
			if(cr->u.block->_else){
				lywr_free_instruction(pool,cr->u.block->_else);
			}

			lywr_pool_free(pool,cr->u.block);
		}


		if(cr->code == lywr_op_br_table){
			lywr_pool_free(pool,cr->u.br_table);
		}

		lywr_pool_free(pool,cr);
		cr = nx;
	}
}

lywrrc lywr_destory(lywr_ctx* ctx)
{
	struct lywr_rbtree2_iter* iter = nullptr;
	if(lywr_rbtree2_ok == lywr_rbtree2_scan_init(&ctx->symbol_module_table,&iter)){
		lywr_module* oldmodule;
		while(lywr_rbtree2_ok == lywr_rbtree2_scan_next(iter,(const void**)&oldmodule)){
			if(oldmodule->type == lywr_module_type_wasm){
				// 释放函数的spec
				wasm_varuint32 c;
				for(c = 0;c < oldmodule->types_count;++c){
					wasm_func_type* ftype = &oldmodule->types[c];
					lywr_pool_free(ctx->mloader,ftype->spec.argv);
					lywr_pool_free(ctx->mloader,ftype->spec.rval);
				}

				// 释放指令对象
				wasm_varuint32 i;
				for(i = 0; i < oldmodule->functionbodys_count ;++i){
					wasm_function_body* fb = oldmodule->functionbodys + i;
					if(fb->code.phase == lywr_lazy_data_phase_compiled){
						lywr_expression* expr = &fb->code.expr;
						lywr_instruction* cr = expr->insts;
						lywr_free_instruction(ctx->mloader,cr);
					}else if(fb->code.phase == lywr_lazy_data_phase_optimize){
						//TODO;
					}

				}
			}

			lywr_rbtree2_destory(&oldmodule->symbol_table);
			lywr_poolx_destory(oldmodule->mloader);
		}
	}

	lywr_rbtree2_scan_destory(&ctx->symbol_module_table,iter);









	lywr_rbtree2_destory(&ctx->symbol_module_table);
	lywr_pool_destory(ctx->mloader);
	return lywrrc_ok;
}

lywrrc static lywr_parse_type_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Type Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->types_count));
	TRACE("entries数量:%d\n",mdl->types_count);

	mdl->types = lywr_poolx_malloc(mdl->mloader,mdl->types_count * sizeof(*mdl->types));
	wasm_varuint32 c;
	for(c = 0;c < mdl->types_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_func_type(ctx,mdl,&mdl->types[c]));
		wasm_func_type* ftype = &mdl->types[c];

		ftype->spec.shortcutlen = ftype->param_count + 1 + ftype->return_count;
		ftype->spec.shortcut = lywr_poolx_malloc(mdl->mloader,ftype->spec.shortcutlen);
		ftype->spec.argc = ftype->param_count;
		ftype->spec.argv = lywr_pool_malloc(ctx->mloader,sizeof(lywr_data) * ftype->param_count);
		if(ftype->spec.argv == nullptr) return lywrrc_oom;
		ftype->spec.rcount = ftype->return_count;
		ftype->spec.rval = lywr_pool_malloc(ctx->mloader,sizeof(lywr_data) * ftype->return_count);
		if(ftype->spec.rval == nullptr) return lywrrc_oom;



		int t =0;
		int i;
		for(i=0;i<ftype->param_count;++i){
			if(ftype->param_types[i] == wasm_value_type_i32){
				ftype->spec.shortcut[t++] = 'i';
				ftype->spec.argv[i].type = lywr_data_i32;
			}else if(ftype->param_types[i] == wasm_value_type_i64){
				ftype->spec.shortcut[t++] = 'I';
				ftype->spec.argv[i].type = lywr_data_i64;
			}else if(ftype->param_types[i] == wasm_value_type_f32){
				ftype->spec.shortcut[t++] = 'f';
				ftype->spec.argv[i].type = lywr_data_f32;
			}else if(ftype->param_types[i] == wasm_value_type_f64){
				ftype->spec.shortcut[t++] = 'F';
				ftype->spec.argv[i].type = lywr_data_f64;
			}else{
				return lywrrc_bad_format;
			}
		}
		ftype->spec.shortcut[t++] = ':';
		for(i=0;i<ftype->return_count;++i){
			if(ftype->return_type[i] == wasm_value_type_i32){
				ftype->spec.shortcut[t++] = 'i';
				ftype->spec.rval[i].type = lywr_data_i32;
			}else if(ftype->return_type[i] == wasm_value_type_i64){
				ftype->spec.shortcut[t++] = 'I';
				ftype->spec.rval[i].type = lywr_data_i64;
			}else if(ftype->return_type[i] == wasm_value_type_f32){
				ftype->spec.shortcut[t++] = 'f';
				ftype->spec.rval[i].type = lywr_data_f32;
			}else if(ftype->return_type[i] == wasm_value_type_f64){
				ftype->spec.shortcut[t++] = 'F';
				ftype->spec.rval[i].type = lywr_data_f64;
			}else{
				return lywrrc_bad_format;
			}
		}


	}

	return lywrrc_ok;
}

lywrrc static lywr_parse_import_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Import Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->imports_count));
	TRACE("entries数量:%d\n",mdl->imports_count);

	mdl->imports = lywr_poolx_malloc(mdl->mloader,mdl->imports_count * sizeof(*mdl->imports));

	wasm_varuint32 c;
	for(c = 0;c < mdl->imports_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_import_entry(ctx,mdl,&mdl->imports[c]));
	}

	return lywrrc_ok;
}

lywrrc static lywr_parse_function_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Function Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->functions_count));
	TRACE("entries数量:%d\n",mdl->functions_count);

	mdl->functions = lywr_poolx_malloc(mdl->mloader,mdl->functions_count * sizeof(*mdl->functions));

	wasm_varuint32 c;
	for(c = 0;c < mdl->functions_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->functions[c]));
		TRACE("定义function[%d]=%d\n",c,mdl->functions[c]);
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_table_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Table Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->tables_count));
	TRACE("entries数量:%d\n",mdl->tables_count);

	mdl->tables = lywr_poolx_malloc(mdl->mloader,mdl->tables_count * sizeof(*mdl->tables));
	wasm_varuint32 c;
	for(c = 0;c < mdl->tables_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_table_type(ctx,mdl,&mdl->tables[c]));
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_memory_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Memory Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->memorys_count));
	TRACE("entries数量:%d\n",mdl->memorys_count);

	mdl->memorys = lywr_poolx_malloc(mdl->mloader,mdl->memorys_count * sizeof(*mdl->memorys));
	wasm_varuint32 c;
	for(c = 0;c < mdl->memorys_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_memory_type(ctx,mdl,&mdl->memorys[c]));
		TRACE("读取到memeory=%llu~%llu\n",mdl->memorys[c].limits.initial,mdl->memorys[c].limits.maximum);
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_global_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Global Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->globals_count));
	TRACE("entries数量:%d\n",mdl->globals_count);

	mdl->globals = lywr_poolx_malloc(mdl->mloader,mdl->globals_count * sizeof(*mdl->globals));
	wasm_varuint32 c;
	for(c = 0;c < mdl->globals_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_global_variable(ctx,mdl,&mdl->globals[c]));
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_export_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Export Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->exports_count));
	TRACE("entries数量:%d\n",mdl->exports_count);

	mdl->exports = lywr_poolx_malloc(mdl->mloader,mdl->exports_count * sizeof(*mdl->exports));
	wasm_varuint32 c;
	for(c = 0;c < mdl->exports_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_export_entry(ctx,mdl,&mdl->exports[c]));
	}

	for(c = 0;c < mdl->exports_count;++c){
		lywr_symbol* newnode = lywr_poolx_malloc(mdl->mloader,sizeof(lywr_symbol));
		if(newnode == nullptr){
			return lywrrc_oom;
		}
		wasm_export_entry* ee = mdl->exports + c;

		newnode->type = lywr_module_type_wasm;
		newnode->namelength = ee->field_len;
		newnode->fieldname = lywr_poolx_malloc(mdl->mloader,ee->field_len);
		newnode->entry = ee;
		newnode->mdl = mdl;
		lywr_memcpy(newnode->fieldname,ee->field_str,ee->field_len);

		lywr_symbol* oldnode;
		enum lywr_rbtree2_ec rc = lywr_rbtree2_insert(&mdl->symbol_table,newnode,(const void**)&oldnode);
		if(rc == lywr_rbtree2_fail){
			return lywrrc_oom;
		}
		TRACE("向红黑树插入名字有[%d]个节字的节点：%.*s,其索引是type=%d,index=%d\n",newnode->namelength,newnode->namelength,(const unsigned char*)newnode->fieldname,ee->kind,ee->index);
	}

	return lywrrc_ok;
}

lywrrc static lywr_parse_start_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Start Section\n");
	wasm_varuint32 index; 
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&index));
	TRACE("入口点索引:%d\n",index);
	return lywrrc_ok;
}

lywrrc static lywr_parse_element_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Element Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->elements_count));
	TRACE("entries数量:%d\n",mdl->elements_count);

	mdl->elements = lywr_poolx_malloc(mdl->mloader,mdl->elements_count * sizeof(*mdl->elements));
	wasm_varuint32 c;
	for(c = 0;c < mdl->elements_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_elem_segment(ctx,mdl,&mdl->elements[c]));
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_code_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Code Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->functionbodys_count));
	TRACE("entries数量:%d\n",mdl->functionbodys_count);

	mdl->functionbodys = lywr_poolx_malloc(mdl->mloader,mdl->functionbodys_count * sizeof(*mdl->functionbodys));
	wasm_varuint32 c;
	for(c = 0;c < mdl->functionbodys_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_function_body(ctx,mdl,&mdl->functionbodys[c]));
		TRACE("加载函数[%d]=%lld字节\n",c,mdl->functionbodys[c].body_size);
	}
	return lywrrc_ok;
}

lywrrc static lywr_parse_data_section(lywr_ctx* ctx,lywr_module* mdl,long long section_start,long long payload_len)
{
	TRACE("解析 Data Section\n");
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&mdl->datas_count));
	TRACE("entries数量:%d\n",mdl->datas_count);

	mdl->datas = lywr_poolx_malloc(mdl->mloader,mdl->datas_count * sizeof(*mdl->datas));
	wasm_varuint32 c;
	for(c = 0;c < mdl->datas_count;++c){
		LYWRRC_VERIFY(lywr_parse_wasm_data_segment(ctx,mdl,&mdl->datas[c]));
	}
	return lywrrc_ok;
}

lywrrc lywr_get_i64_init_expr(lywr_ctx* ctx,lywr_module* mdl,wasm_init_expr* init_expr,long long* rvalue)
{
	if(init_expr->init_expr_type == lywr_op_i32_const){
		*rvalue = init_expr->u.u32;
	}else if(init_expr->init_expr_type == lywr_op_i64_const){
		*rvalue = init_expr->u.u64;
	}else if(init_expr->init_expr_type == lywr_op_f32_const){
		*rvalue = (long long)init_expr->u.f32;
	}else if(init_expr->init_expr_type == lywr_op_f64_const){
		*rvalue = (long long)init_expr->u.f64;
	}else if(init_expr->init_expr_type == lywr_op_get_global){
		unsigned long long global_index = init_expr->u.global_index;
		if(mdl->globals[global_index].expr.init_expr_type == lywr_op_i32_const){
			*rvalue = mdl->globals[global_index].expr.u.u32;
		}else if(mdl->globals[global_index].expr.init_expr_type == lywr_op_i64_const){
			*rvalue = mdl->globals[global_index].expr.u.u64;
		}else if(mdl->globals[global_index].expr.init_expr_type == lywr_op_f32_const){
			*rvalue = mdl->globals[global_index].expr.u.f32;
		}else if(mdl->globals[global_index].expr.init_expr_type == lywr_op_f64_const){
			*rvalue = mdl->globals[global_index].expr.u.f64;
		}else if(mdl->globals[global_index].expr.init_expr_type == lywr_op_get_global){
			TRACE("global.get 错误，gloal中怎么又get global了？\n");
			return lywrrc_bad_format;
		}
	}else{
		return lywrrc_bad_format;
	}
	return lywrrc_ok;
}


lywrrc lywr_load_wasm_module(lywr_ctx* ctx,lywr_module_spec* spec,const char* modulename,int modulenamelength)
{

	lywr_poolx* mloader = lywr_poolx_create(ctx->pmalloc,ctx->pfree,ctx->uptr);

	lywr_module* mdl = lywr_poolx_malloc(mloader,sizeof(lywr_module));
	mdl->type = lywr_module_type_wasm;
	mdl->mloader = mloader;
	mdl->offset = 0;
	mdl->namelength = modulenamelength;
	mdl->name = lywr_poolx_malloc(mloader,modulenamelength);

	mdl->symbol_table.compator = lywr_symbol_cmpr;
	lywr_rbtree2_init(&mdl->symbol_table,ctx->mloader);
	mdl->spec = *spec;

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

	wasm_uint32 magic = 0;

	if(lywr_parse_wasm_uint32(ctx,mdl,&magic) != lywrrc_ok) return lywrrc_bad_format;
	if(magic != 0x6d736100) return lywrrc_bad_format;
	TRACE("魔法数字:%08x\n",magic);
	wasm_uint32 version;
	if(lywr_parse_wasm_uint32(ctx,mdl,&version) != lywrrc_ok) return lywrrc_bad_format;
	TRACE("版 本 号:%d\n",version);



	mdl->types_count = 0;
	mdl->types = nullptr;

	mdl->imports_count = 0;
	mdl->imports = nullptr;
	
	mdl->functions_count = 0;
	mdl->functions = nullptr;
	
	mdl->tables_count = 0;
	mdl->tables = nullptr;
	
	mdl->memorys_count = 0;
	mdl->memorys = nullptr;
	
	mdl->globals_count = 0;
	mdl->globals = nullptr;
	
	mdl->exports_count = 0;
	mdl->exports = nullptr;
	
	mdl->elements_count = 0;
	mdl->elements = nullptr;
	
	mdl->functionbodys_count = 0;
	mdl->functionbodys = nullptr;
	
	mdl->datas_count = 0;
	mdl->datas = nullptr;
	

	mdl->merge_function_count = 0;
	mdl->merge_function = nullptr;

	mdl->merge_table_count = 0;
	mdl->merge_table = nullptr;

	mdl->merge_memory_count = 0;
	mdl->merge_memory = nullptr;

	mdl->merge_global_count = 0;
	mdl->merge_global = nullptr;


	while(1){
		wasm_varuint7 section_id = 0;
		{
			lywrrc rc = lywr_parse_wasm_varuint7(ctx,mdl,&section_id);
			if(rc != lywrrc_ok) {
				if(rc == lywrrc_eof) break;
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		wasm_varuint32 payload_len;
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&payload_len));

		long long section_start = mdl->offset;
		TRACE("在%llu读到段%d,长度%u\n",section_start,section_id,payload_len);
		if(section_id == 0){
			/*
			wasm_varuint32 name_len;
			LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&name_len));

			name

			TRACE("读到段id=%d\n",section_id);
			*/
			lywr_stream_seek(ctx,mdl,section_start + payload_len);
			//lywr_leb128_read
		}else if(section_id == 1){
			// Type sections
			LYWRRC_VERIFY(lywr_parse_type_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 2){
			// Import sections
			LYWRRC_VERIFY(lywr_parse_import_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 3){
			// Function sections
			LYWRRC_VERIFY(lywr_parse_function_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 4){
			// Table sections
			LYWRRC_VERIFY(lywr_parse_table_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 5){
			// Memory sections
			LYWRRC_VERIFY(lywr_parse_memory_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 6){
			// Global sections
			LYWRRC_VERIFY(lywr_parse_global_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 7){
			// Export sections
			LYWRRC_VERIFY(lywr_parse_export_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 8){
			// Start sections
			LYWRRC_VERIFY(lywr_parse_start_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 9){
			// Element sections
			LYWRRC_VERIFY(lywr_parse_element_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 10){
			// Code sections
			LYWRRC_VERIFY(lywr_parse_code_section(ctx,mdl,section_start,payload_len));
		}else if(section_id == 11){
			// Data sections
			LYWRRC_VERIFY(lywr_parse_data_section(ctx,mdl,section_start,payload_len));
		}else{
			TRACE("读到了错误的段id=%d\n",section_id);
			return lywrrc_bad_format;
		}
	}


	// 计算索引空间
	mdl->merge_function_count = mdl->functionbodys_count;
	mdl->merge_table_count = mdl->tables_count;
	mdl->merge_memory_count = mdl->memorys_count;
	mdl->merge_global_count = mdl->globals_count;

	wasm_varuint32 i;
	for(i = 0;i < mdl->imports_count;++i)
	{
		switch(mdl->imports[i].kind){
			case 0: mdl->merge_function_count++;
			break;
			case 1: mdl->merge_table_count++;
			break;
			case 2: mdl->merge_memory_count++;
			break;
			case 3: mdl->merge_global_count++;
			break;
		}
	}

	mdl->merge_function = lywr_poolx_malloc(mdl->mloader,mdl->merge_function_count * sizeof(*mdl->merge_function));
	mdl->merge_table = lywr_poolx_malloc(mdl->mloader,mdl->merge_table_count * sizeof(*mdl->merge_table));
	mdl->merge_memory = lywr_poolx_malloc(mdl->mloader,mdl->merge_memory_count * sizeof(*mdl->merge_memory));
	mdl->merge_global = lywr_poolx_malloc(mdl->mloader,mdl->merge_global_count * sizeof(*mdl->merge_global));




	wasm_varuint32 merge_function_index = 0;
	wasm_varuint32 merge_table_index = 0;
	wasm_varuint32 merge_memory_index = 0;
	wasm_varuint32 merge_global_index = 0;

	for(i = 0;i < mdl->imports_count;++i)
	{
		switch(mdl->imports[i].kind){
			case 0:{
				mdl->merge_function[merge_function_index].type = lywr_wasm_resource_link_type_import;
				mdl->merge_function[merge_function_index].index = i;
				TRACE("[function]index=%d->%d  import\n",merge_function_index,i);
				++merge_function_index;
			}
			break;
			case 1:{
				mdl->merge_table[merge_table_index].type = lywr_wasm_resource_link_type_import;
				mdl->merge_table[merge_table_index].index = i;
				TRACE("[table]index=%d->%d  import\n",merge_table_index,i);
				++merge_table_index;
			}
			break;
			case 2:{
				mdl->merge_memory[merge_memory_index].type = lywr_wasm_resource_link_type_import;
				mdl->merge_memory[merge_memory_index].index = i;
				TRACE("[memory]index=%d->%d  import\n",merge_memory_index,i);
				++merge_memory_index;
			}
			break;
			case 3:{
				mdl->merge_global[merge_global_index].type = lywr_wasm_resource_link_type_import;
				mdl->merge_global[merge_global_index].index = i;
				TRACE("[globa]index=%d->%d  import\n",merge_global_index,i);
				++merge_global_index;
			}
			break;
		}
	}

	for(i = 0;i < mdl->functionbodys_count;++i){
		mdl->merge_function[merge_function_index].type = lywr_wasm_resource_link_type_local;
		mdl->merge_function[merge_function_index].index = i;
		++merge_function_index;
	}

	for(i = 0;i < mdl->tables_count;++i){
		mdl->merge_table[merge_table_index].type = lywr_wasm_resource_link_type_local;
		mdl->merge_table[merge_table_index].index = i;
		++merge_table_index;
	}

	for(i = 0;i < mdl->memorys_count;++i){
		mdl->merge_memory[merge_memory_index].type = lywr_wasm_resource_link_type_local;
		mdl->merge_memory[merge_memory_index].index = i;
		++merge_memory_index;
	}

	for(i = 0;i < mdl->globals_count;++i){
		mdl->merge_global[merge_global_index].type = lywr_wasm_resource_link_type_local;
		mdl->merge_global[merge_global_index].index = i;
		++merge_global_index;
	}


	// 为table填写element_list
	for(i = 0;i < mdl->elements_count;++i){
		wasm_elem_segment* elem = mdl->elements + i;
		if(mdl->merge_table[elem->index].type != lywr_wasm_resource_link_type_local){
			return lywrrc_bad_format;
		}

		long long offset = 0;
		{
			lywrrc rc = lywr_get_i64_init_expr(ctx,mdl,&elem->offset,&offset);
			if(rc != lywrrc_ok) {
				TRACE("出错了，错误码是:%d\n",rc);
				return rc;
			}
		}

		wasm_varuint32 n;
		for(n = 0;n<elem->num_elem;++n){
			mdl->tables[mdl->merge_table[elem->index].index].element_list[offset + n] = elem->elems[n];
		}
	}
	return lywrrc_ok;
}

lywrrc lywr_get_lazy_data_as_bytes(lywr_ctx* ctx,lywr_module* mdl,lywr_lazy_data* data,wasm_byte** ret)
{
	if(data->phase == lywr_lazy_data_phase_load){
		*ret = data->data;
		return lywrrc_ok;
	}
	if(data->phase == lywr_lazy_data_phase_unload){
		wasm_byte* pbuf = lywr_poolx_malloc(mdl->mloader,data->size);

		if(lywr_pread(ctx,mdl,data->offset_of_wasm,pbuf,data->size) != data->size){
			return lywrrc_bad_format;
		}
		data->data = pbuf;
		data->phase = lywr_lazy_data_phase_load;
		TRACE("惰性加载:%llu个字节\n",data->size);
		*ret = data->data;
		return lywrrc_ok;
	}
	return lywrrc_bad_format;
}

lywrrc lywr_get_lazy_data_as_expr(lywr_ctx* ctx,lywr_module* mdl,lywr_lazy_data* data,lywr_expression** ret)
{

	if(data->phase == lywr_lazy_data_phase_compiled){
		// 第二次执行函数的时候之前会启动惰性优化
		TRACE("惰性优化函数\n");
		*ret = &data->expr;
		return lywrrc_ok;
	}
/*
	if(data->phase == lywr_lazy_data_phase_unload){
		data->phase = lywr_lazy_data_phase_load;
		wasm_byte* pbuf = lywr_poolx_malloc(mdl->mloader,data->size);

		if(lywr_pread(ctx,mdl,data->offset_of_wasm,pbuf,data->size) != data->size){
			return lywrrc_bad_format;
		}
		data->data = pbuf;
		TRACE("惰性加载:%llu个字节\n",data->size);
	}

	if(data->phase == lywr_lazy_data_phase_load){
		lywr_expression expr;
		lywr_parse_expr(data->data,data->size,&expr,ctx->mloader);
		data->expr = expr;
		*ret = &data->expr;
		data->phase = lywr_lazy_data_phase_compiled;

		TRACE("惰性解析函数\n");
		return lywrrc_ok;
	}
	*/

	if(data->phase == lywr_lazy_data_phase_unload){
		data->phase = lywr_lazy_data_phase_compiled;
		wasm_byte* pbuf = lywr_poolx_malloc(mdl->mloader,data->size);

		if(lywr_pread(ctx,mdl,data->offset_of_wasm,pbuf,data->size) != data->size){
			return lywrrc_bad_format;
		}
		//data->data = pbuf;
		TRACE("惰性加载:%llu个字节\n",data->size);
		lywr_expression expr;

		int flag = 0;

		if(mdl->merge_memory_count < 1){
			flag |= LYWR_PARSE_EXPR_FLAG_NO_MEMORY;
		}

		if(mdl->merge_global_count < 1){
			flag |= LYWR_PARSE_EXPR_FLAG_NO_GLOBAL;
		}

		lywr_parse_expr(pbuf,data->size,&expr,ctx->mloader,flag);
		data->expr = expr;
		*ret = &data->expr;

		TRACE("惰性解析函数\n");
		return lywrrc_ok;
	}


	return lywrrc_bad_format;
}



lywrrc lywr_get_module_by_name(lywr_ctx* ctx,const char* modulename,int modulenamelength,lywr_module** mdl)
{
	lywr_module* mdl_finded;
	{
		lywr_module keynode;
		keynode.namelength = modulenamelength;
		keynode.name = (char*)modulename;
		enum lywr_rbtree2_ec rbc = lywr_rbtree2_get(&ctx->symbol_module_table,&keynode,(const void**)&mdl_finded);
		if(rbc != lywr_rbtree2_ok){
			TRACE("没有找到模块:%.*s\n",modulenamelength,modulename);
			return lywrrc_not_found;
		}

		if(mdl_finded == nullptr){
			TRACE("没有找到模块:%.*s\n",modulenamelength,modulename);
			return lywrrc_not_found;
		}
	}
	TRACE("找到模块:%.*s=%p\n",modulenamelength,modulename,(const void*)mdl_finded);
	*mdl = mdl_finded;
	return lywrrc_ok;
}
lywrrc lywr_get_symbol_by_name(lywr_ctx* ctx,lywr_module* mdl,const char* funcname,int funcnamelength,lywr_symbol** symbol)
{
	lywr_symbol* symbol_finded;
	{
		lywr_symbol keynode;
		keynode.namelength = funcnamelength;
		keynode.fieldname = (unsigned char*)funcname;
		enum lywr_rbtree2_ec rbc = lywr_rbtree2_get(&mdl->symbol_table,&keynode,(const void**)&symbol_finded);
		if(rbc != lywr_rbtree2_ok){
			TRACE("没有找到符号:%.*s\n",funcnamelength,funcname);
			return lywrrc_not_found;
		}

		if(symbol_finded == nullptr){
			TRACE("没有找到符号:%.*s\n",funcnamelength,funcname);
			return lywrrc_not_found;
		}
	}
	TRACE("找到符号:%.*s=%p\n",funcnamelength,funcname,(const void*)symbol_finded);
	*symbol = symbol_finded;
	return lywrrc_ok;
}



LYWR_NAMESPACE_END
