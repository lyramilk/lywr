#include "lywr_opcode.h"

LYWR_NAMESPACE_START
#define LYWR_OP_GET_ENUM_NAME(OP,NAME) case OP: return #NAME + 3;
#define LYWR_OP_GET_ENUM_NAME_UNUSE2(OP,NAME,...) case OP: return #NAME + 3;
#define LYWR_OP_GET_ENUM_NAME_UNUSE(OP,...) LYWR_OP_GET_ENUM_NAME_UNUSE2(OP,##__VA_ARGS__,op_unused_##OP)
const char* lywr_opcode_name(int code){
	switch(code){
		LYWR_OP_DEFINE(LYWR_OP_GET_ENUM_NAME,LYWR_OP_GET_ENUM_NAME_UNUSE)
	}
	return "unknow";
}


LYWR_NAMESPACE_END
