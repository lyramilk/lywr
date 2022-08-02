#ifndef _lyramilk_lywr_expr_hh_
#define _lyramilk_lywr_expr_hh_

#include "lywr.hh"

LYWR_NAMESPACE_START

#define LYWR_PARSE_EXPR_FLAG_NO_MEMORY 1
#define LYWR_PARSE_EXPR_FLAG_NO_GLOBAL 2

lywrrc lywr_parse_expr(wasm_byte* code,unsigned long long length,lywr_expression* expr,lywr_pool* pool,int flag);


LYWR_NAMESPACE_END

#endif
