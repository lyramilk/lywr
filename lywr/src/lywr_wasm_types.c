#include "lywr.hh"
#include "lywr_wasm_types.hh"
#include "lywr_stream.hh"

#define nullptr (void*)0

LYWR_NAMESPACE_START

#define LYWRRC_VERIFY(x) {lywrrc rc = x;if(rc != lywrrc_ok) {TRACE("出错了，错误码是:%d\n",rc);return rc;}}

lywrrc static lywr_leb128_read64u(lywr_ctx* ctx,lywr_module* mdl,unsigned long long *value)
{
	unsigned long long r = 0;
	unsigned int idx = 0;
	while(1){
		unsigned char c;
		int rc = lywr_stream_get(ctx,mdl,(char*)&c);
		if(rc == -1){
			return lywrrc_io_error;
		}else if(rc == 0){
			*value = r;
			return lywrrc_eof;
		}
		unsigned long long k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}
	*value = r;
	return lywrrc_ok;
}

lywrrc static lywr_leb128_read32u(lywr_ctx* ctx,lywr_module* mdl,unsigned int *value)
{
	unsigned int r = 0;
	unsigned int idx = 0;
	while(1){
		unsigned char c;
		int rc = lywr_stream_get(ctx,mdl,(char*)&c);
		if(rc == -1){
			return lywrrc_io_error;
		}else if(rc == 0){
			*value = r;
			return lywrrc_eof;
		}
		unsigned int k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}
	*value = r;
	return lywrrc_ok;
}

lywrrc static lywr_leb128_read32i(lywr_ctx* ctx,lywr_module* mdl,int *value)
{
	int r = 0;
	unsigned int idx = 0;
	unsigned char c;
	while(1){
		int rc = lywr_stream_get(ctx,mdl,(char*)&c);
		if(rc == -1){
			return lywrrc_io_error;
		}else if(rc == 0){
			*value = r;
			return lywrrc_eof;
		}
		unsigned int k = c&0x7f;
		r |= (k << idx);
		if((c & 0x80) == 0) break;
		idx += 7;
	}

//TRACE("或之前:%x,%x,idx=%d\n",r,- (1 << (idx + 7)),idx);
	if(idx != 28 && (c & 0x40)){

#ifdef LYWR_LITTLE_ENDIAN
		r |= - (1 << (idx + 7));
#endif
#ifdef LYWR_BIG_ENDIAN
		r |= - (1 << (idx + 7));
#endif
	}
//TRACE("或之后:%x\n",r);
//TRACE("-32:%x\n",-32);
//TRACE("-32:%llx\n",(long long)(int)-32);
//TRACE("-32:%llx\n",(long long)(char)-32);

	*value = (int)r;
	return lywrrc_ok;
}

lywrrc lywr_parse_wasm_uint8(lywr_ctx* ctx,lywr_module* mdl,wasm_uint8* value)
{
	if(lywr_stream_read(ctx,mdl,value,1) != 1){
		return lywrrc_bad_format;
	}
	return lywrrc_ok;
}

lywrrc lywr_parse_wasm_uint32(lywr_ctx* ctx,lywr_module* mdl,wasm_uint32* value)
{
	if(lywr_stream_read(ctx,mdl,value,4) != 4){
		return lywrrc_bad_format;
	}
	return lywrrc_ok;
}


lywrrc lywr_parse_wasm_varuint1(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint1* value)
{
	unsigned int tmp;
	lywrrc rc = lywr_leb128_read32u(ctx,mdl,&tmp);
	if(rc != lywrrc_ok) return rc;
	*value = (wasm_varuint1)tmp;
	return rc;
}

lywrrc lywr_parse_wasm_varuint32(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint32* value)
{
	unsigned int tmp;
	lywrrc rc = lywr_leb128_read32u(ctx,mdl,&tmp);
	if(rc != lywrrc_ok) return rc;
	*value = (wasm_varuint32)tmp;
	return rc;
}

lywrrc lywr_parse_wasm_varuint64(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint64* value)
{
	unsigned long long tmp;
	lywrrc rc = lywr_leb128_read64u(ctx,mdl,&tmp);
	if(rc != lywrrc_ok) return rc;
	*value = (wasm_varuint64)tmp;
	return rc;
}

lywrrc lywr_parse_wasm_varint7(lywr_ctx* ctx,lywr_module* mdl,wasm_varint7* value)
{
	int tmp;
	lywrrc rc = lywr_leb128_read32i(ctx,mdl,&tmp);
	if(rc != lywrrc_ok) return rc;
	//TRACE("tmp=%x,%d\n",tmp,tmp);
	*value = (wasm_varint7)tmp;
	//TRACE("value=%x,%d\n",*value,*value);

	//TRACE("%d\n",(int)(char)-32);
	return rc;
}

lywrrc lywr_parse_wasm_varuint7(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint7* value)
{
	unsigned int tmp;
	lywrrc rc = lywr_leb128_read32u(ctx,mdl,&tmp);
	if(rc != lywrrc_ok) return rc;
	*value = (wasm_varuint7)tmp;
	return rc;
}

lywrrc lywr_parse_wasm_bytes(lywr_ctx* ctx,lywr_module* mdl,wasm_byte* value,wasm_varuint32 length)
{
	if(lywr_stream_read(ctx,mdl,value,length) != length){
		return lywrrc_bad_format;
	}
	return lywrrc_ok;
}

//	value_type
lywrrc lywr_parse_wasm_value_type(lywr_ctx* ctx,lywr_module* mdl,wasm_value_type* value)
{
	wasm_varint7 v7;
	lywrrc rc = lywr_parse_wasm_varint7(ctx,mdl,&v7);
	if(rc == lywrrc_ok){
		switch((unsigned char)v7){
			case (unsigned char)-0x01: *value = wasm_value_type_i32;
			break;
			case (unsigned char)-0x02: *value = wasm_value_type_i64;
			break;
			case (unsigned char)-0x03: *value = wasm_value_type_f32;
			break;
			case (unsigned char)-0x04: *value = wasm_value_type_f64;
			break;
		}
	}
	return rc;
}

//	elem_type
lywrrc lywr_parse_wasm_elem_type(lywr_ctx* ctx,lywr_module* mdl,wasm_elem_type* value)
{
	return lywr_parse_wasm_varint7(ctx,mdl,value);
}

//	table_type
lywrrc lywr_parse_wasm_table_type(lywr_ctx* ctx,lywr_module* mdl,wasm_table_type* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_elem_type(ctx,mdl,&value->element_type));
	LYWRRC_VERIFY(lywr_parse_wasm_resizable_limits(ctx,mdl,&value->limits));
	// 这个element_list在读取Element段的时候初始化
	value->element_list = lywr_poolx_malloc(mdl->mloader,sizeof(*value->element_list) * value->limits.maximum);
	return lywrrc_ok;
}

//	memory_type
lywrrc lywr_parse_wasm_memory_type(lywr_ctx* ctx,lywr_module* mdl,wasm_memory_type* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_resizable_limits(ctx,mdl,&value->limits));
	return lywrrc_ok;
}

// resizable_limits(
lywrrc lywr_parse_wasm_resizable_limits(lywr_ctx* ctx,lywr_module* mdl,wasm_resizable_limits* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint1(ctx,mdl,&value->flags));
	LYWRRC_VERIFY(lywr_parse_wasm_varuint64(ctx,mdl,&value->initial));
	if(value->flags == 1){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint64(ctx,mdl,&value->maximum));
	}else{
		//如果没有最大值，那么最大值就是initial
		value->maximum = value->initial;
	}
	return lywrrc_ok;
}


//	global_variable
lywrrc lywr_parse_wasm_global_variable(lywr_ctx* ctx,lywr_module* mdl,wasm_global_variable* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_global_type(ctx,mdl,&value->type));
	LYWRRC_VERIFY(lywr_parse_wasm_init_expr(ctx,mdl,&value->expr));
	return lywrrc_ok;
}

//	global_type
lywrrc lywr_parse_wasm_global_type(lywr_ctx* ctx,lywr_module* mdl,wasm_global_type* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_value_type(ctx,mdl,&value->content_type));
	LYWRRC_VERIFY(lywr_parse_wasm_varuint1(ctx,mdl,&value->mutability));
	return lywrrc_ok;
}

lywrrc lywr_parse_wasm_init_expr(lywr_ctx* ctx,lywr_module* mdl,wasm_init_expr* value)
{
	unsigned char codetype;
	LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&codetype));
	value->init_expr_type = (lywr_opcode)codetype;

	if(value->init_expr_type == lywr_op_i32_const){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,(wasm_varuint32*)&value->u.u32));

		unsigned char code_instruction;
		LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&code_instruction));
		if(code_instruction == (unsigned char)lywr_op_end){
			return lywrrc_ok;
		}
		return lywrrc_bad_format;
	}else if(value->init_expr_type == lywr_op_i64_const){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint64(ctx,mdl,(wasm_varuint64*)&value->u.u64));

		unsigned char code_instruction;
		LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&code_instruction));
		if(code_instruction == (unsigned char)lywr_op_end){
			return lywrrc_ok;
		}
		return lywrrc_bad_format;
	}else if(value->init_expr_type == lywr_op_f32_const){
		LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,(wasm_byte*)&value->u.f32,4));

		unsigned char code_instruction;
		LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&code_instruction));
		if(code_instruction == (unsigned char)lywr_op_end){
			return lywrrc_ok;
		}
		return lywrrc_bad_format;
	}else if(value->init_expr_type == lywr_op_f64_const){
		LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,(wasm_byte*)&value->u.f64,8));

		unsigned char code_instruction;
		LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&code_instruction));
		if(code_instruction == (unsigned char)lywr_op_end){
			return lywrrc_ok;
		}
		return lywrrc_bad_format;
	}else if(value->init_expr_type == lywr_op_get_global){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,(wasm_varuint32*)&value->u.global_index));

		unsigned char code_instruction;
		LYWRRC_VERIFY(lywr_parse_wasm_uint8(ctx,mdl,&code_instruction));
		if(code_instruction == (unsigned char)lywr_op_end){
			return lywrrc_ok;
		}
		return lywrrc_bad_format;
	}
	return lywrrc_bad_format;
}

lywrrc lywr_parse_wasm_external_kind(lywr_ctx* ctx,lywr_module* mdl,wasm_external_kind* value)
{
	if(lywr_stream_read(ctx,mdl,value,1) != 1){
		return lywrrc_bad_format;
	}
	return lywrrc_ok;
}

//	func_type
lywrrc lywr_parse_wasm_func_type(lywr_ctx* ctx,lywr_module* mdl,wasm_func_type* value)
{
	wasm_varint7 form = 0;
	LYWRRC_VERIFY(lywr_parse_wasm_varint7(ctx,mdl,&form));
	//TRACE("form=%d,%x\n",form,form);
	if(form != -0x20){
		TRACE("文档在这里限定了form值是固定0x60，如果类型值不是0x60(func)就认为是有错误。实际是%x\n",form);
		return lywrrc_bad_format;
	}

	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->param_count));

	value->param_types = lywr_poolx_malloc(mdl->mloader,sizeof(*value->param_types) * value->param_count);
	if(value->param_types == nullptr) return lywrrc_oom;

	//TRACE("参数数量:%d\n",value->param_count);
	for(int param_index = 0;param_index < value->param_count;++param_index){
		LYWRRC_VERIFY(lywr_parse_wasm_value_type(ctx,mdl,&value->param_types[param_index]));
		//TRACE("参数索引:%d\n",param_index);
		//TRACE("参数类型:%x\n",value->param_types[param_index]);
	}

	LYWRRC_VERIFY(lywr_parse_wasm_varuint1(ctx,mdl,&value->return_count));
	//TRACE("返回值数量:%d\n",value->return_count);

	value->return_type = lywr_poolx_malloc(mdl->mloader,sizeof(*value->return_type) * value->return_count);
	if(value->return_type == nullptr) return lywrrc_oom;

	for(int return_index = 0;return_index < value->return_count;++return_index){
		LYWRRC_VERIFY(lywr_parse_wasm_value_type(ctx,mdl,&value->return_type[return_index]));
		//TRACE("返回索引:%d\n",return_index);
		//TRACE("返回类型:%x\n",value->return_type[return_index]);
	}

	return lywrrc_ok;
}

//	export_entry
lywrrc lywr_parse_wasm_export_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_export_entry* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->field_len));
	value->field_str = lywr_poolx_malloc(mdl->mloader,value->field_len);
	if(value->field_str == nullptr) return lywrrc_oom;
	LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,value->field_str,value->field_len));
	LYWRRC_VERIFY(lywr_parse_wasm_external_kind(ctx,mdl,&value->kind));
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->index));

	return lywrrc_ok;
}

//	import_entry
lywrrc lywr_parse_wasm_import_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_import_entry* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->module_len));
	value->module_str = lywr_poolx_malloc(mdl->mloader,value->module_len);
	if(value->module_str == nullptr) return lywrrc_oom;

	LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,value->module_str,value->module_len));

	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->field_len));
	value->field_str = lywr_poolx_malloc(mdl->mloader,value->field_len);
	if(value->field_str == nullptr) return lywrrc_oom;

	LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,value->field_str,value->field_len));

	LYWRRC_VERIFY(lywr_parse_wasm_external_kind(ctx,mdl,&value->kind));

	if(value->kind == 0){
		// Function
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->function));
		TRACE("导入Function: %.*s.%.*s %d\n",value->module_len,value->module_str,value->field_len,value->field_str,value->function);
	}else if(value->kind == 1){
		// Table
		LYWRRC_VERIFY(lywr_parse_wasm_table_type(ctx,mdl,&value->table));
		TRACE("导入Table: %.*s.%.*s\n",value->module_len,value->module_str,value->field_len,value->field_str);
	}else if(value->kind == 2){
		// Memory
		LYWRRC_VERIFY(lywr_parse_wasm_memory_type(ctx,mdl,&value->memory));
		TRACE("导入Memory: %.*s.%.*s\n",value->module_len,value->module_str,value->field_len,value->field_str);
	}else if(value->kind == 3){
		// Global
		LYWRRC_VERIFY(lywr_parse_wasm_global_type(ctx,mdl,&value->global));
		TRACE("导入Global: %.*s.%.*s\n",value->module_len,value->module_str,value->field_len,value->field_str);
	}

	return lywrrc_ok;
}

//	elem_segment
lywrrc lywr_parse_wasm_elem_segment(lywr_ctx* ctx,lywr_module* mdl,wasm_elem_segment* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->index));
	LYWRRC_VERIFY(lywr_parse_wasm_init_expr(ctx,mdl,&value->offset));
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->num_elem));

	value->elems = lywr_poolx_malloc(mdl->mloader,sizeof(*value->elems) * value->num_elem);
	if(value->elems == nullptr) return lywrrc_oom;

	for(wasm_varuint32 i = 0;i< value->num_elem;++i){
		LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->elems[i]));
	}
	return lywrrc_ok;
}

//	local_entry
lywrrc lywr_parse_wasm_local_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_local_entry* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->count));
	LYWRRC_VERIFY(lywr_parse_wasm_value_type(ctx,mdl,&value->type));
	return lywrrc_ok;
}

//	function_body
lywrrc lywr_parse_wasm_function_body(lywr_ctx* ctx,lywr_module* mdl,wasm_function_body* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint64(ctx,mdl,&value->body_size));
	long long curseg = mdl->offset;
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->local_count));

	value->locals = lywr_poolx_malloc(mdl->mloader,sizeof(*value->locals) * value->local_count);
	if(value->locals == nullptr) return lywrrc_oom;
	for(wasm_varuint32 i = 0;i< value->local_count;++i){
		LYWRRC_VERIFY(lywr_parse_wasm_local_entry(ctx,mdl,&value->locals[i]));
	}
	long long curseg2 = mdl->offset - curseg;

	wasm_varuint32 codesize = value->body_size - curseg2;

	if(mdl->spec.lazyload == lywr_module_spec_load_normal){
		value->code.phase = lywr_lazy_data_phase_load;
		value->code.data = lywr_poolx_malloc(mdl->mloader,codesize);
		if(value->code.data == nullptr) return lywrrc_oom;
		LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,value->code.data,codesize));
	}else if(mdl->spec.lazyload == lywr_module_spec_load_lazy){
		value->code.phase = lywr_lazy_data_phase_unload;
		value->code.size = codesize;
		value->code.offset_of_wasm = lywr_stream_tell(ctx,mdl);
		lywr_stream_skip(ctx,mdl,value->code.size);
	}

	return lywrrc_ok;
}

//	data_segment
lywrrc lywr_parse_wasm_data_segment(lywr_ctx* ctx,lywr_module* mdl,wasm_data_segment* value)
{
	LYWRRC_VERIFY(lywr_parse_wasm_varuint32(ctx,mdl,&value->index));
	LYWRRC_VERIFY(lywr_parse_wasm_init_expr(ctx,mdl,&value->offset));
	LYWRRC_VERIFY(lywr_parse_wasm_varuint64(ctx,mdl,&value->size));



	if(mdl->spec.lazyload == lywr_module_spec_load_normal){
		value->data.phase = lywr_lazy_data_phase_load;
		value->data.data = lywr_poolx_malloc(mdl->mloader,value->size);
		if(value->data.data == nullptr) return lywrrc_oom;
		LYWRRC_VERIFY(lywr_parse_wasm_bytes(ctx,mdl,value->data.data,value->size));
	}else if(mdl->spec.lazyload == lywr_module_spec_load_lazy){
		value->data.phase = lywr_lazy_data_phase_unload;
		value->data.size = value->size;
		value->data.offset_of_wasm = lywr_stream_tell(ctx,mdl);
		lywr_stream_skip(ctx,mdl,value->data.size);
	}
	return lywrrc_ok;
}

LYWR_NAMESPACE_END



