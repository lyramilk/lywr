#ifndef _lyramilk_lywr_loader_hh_
#define _lyramilk_lywr_loader_hh_

#include "lywr.hh"
#include "lywr_opcode.h"

LYWR_NAMESPACE_START



lywrrc lywr_parse_wasm_uint8(lywr_ctx* ctx,lywr_module* mdl,wasm_uint8* value);
lywrrc lywr_parse_wasm_uint32(lywr_ctx* ctx,lywr_module* mdl,wasm_uint32* value);
lywrrc lywr_parse_wasm_varuint1(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint1* value);
lywrrc lywr_parse_wasm_varuint32(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint32* value);
lywrrc lywr_parse_wasm_varuint64(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint64* value);

lywrrc lywr_parse_wasm_varint7(lywr_ctx* ctx,lywr_module* mdl,wasm_varint7* value);
lywrrc lywr_parse_wasm_varuint7(lywr_ctx* ctx,lywr_module* mdl,wasm_varuint7* value);
lywrrc lywr_parse_wasm_bytes(lywr_ctx* ctx,lywr_module* mdl,wasm_byte* value,wasm_varuint32 length);
//	value_type
lywrrc lywr_parse_wasm_value_type(lywr_ctx* ctx,lywr_module* mdl,wasm_value_type* value);

//	elem_type
lywrrc lywr_parse_wasm_elem_type(lywr_ctx* ctx,lywr_module* mdl,wasm_elem_type* value);

//	func_type
lywrrc lywr_parse_wasm_func_type(lywr_ctx* ctx,lywr_module* mdl,wasm_func_type* value);

//	table_type
lywrrc lywr_parse_wasm_table_type(lywr_ctx* ctx,lywr_module* mdl,wasm_table_type* value);

//	memory_type
lywrrc lywr_parse_wasm_memory_type(lywr_ctx* ctx,lywr_module* mdl,wasm_memory_type* value);

//	resizable_limits
lywrrc lywr_parse_wasm_resizable_limits(lywr_ctx* ctx,lywr_module* mdl,wasm_resizable_limits* value);

//	global_variable
lywrrc lywr_parse_wasm_global_variable(lywr_ctx* ctx,lywr_module* mdl,wasm_global_variable* value);

//	global_type
lywrrc lywr_parse_wasm_global_type(lywr_ctx* ctx,lywr_module* mdl,wasm_global_type* value);

//	init_expr
lywrrc lywr_parse_wasm_init_expr(lywr_ctx* ctx,lywr_module* mdl,wasm_init_expr* value);

//	export_entry
lywrrc lywr_parse_wasm_export_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_export_entry* value);

//	import_entry
lywrrc lywr_parse_wasm_import_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_import_entry* value);

//	elem_segment
lywrrc lywr_parse_wasm_elem_segment(lywr_ctx* ctx,lywr_module* mdl,wasm_elem_segment* value);

//	local_entry
lywrrc lywr_parse_wasm_local_entry(lywr_ctx* ctx,lywr_module* mdl,wasm_local_entry* value);

//	function_body
lywrrc lywr_parse_wasm_function_body(lywr_ctx* ctx,lywr_module* mdl,wasm_function_body* value);

//	data_segment
lywrrc lywr_parse_wasm_data_segment(lywr_ctx* ctx,lywr_module* mdl,wasm_data_segment* value);

LYWR_NAMESPACE_END

#endif

