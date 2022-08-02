#ifndef _lyramilk_lywr_opcode_hh_
#define _lyramilk_lywr_opcode_hh_

#include "lywr_defs.h"

LYWR_NAMESPACE_START

#define LYWR_OP_DEFINE(E,U) E(0x00,op_unreachable)  /* unreachable */ \
	E(0x01,op_nop)  /* nop */ \
	E(0x02,op_block)  /* block */ \
	E(0x03,op_loop)  /* loop */ \
	E(0x04,op_if)  /* if */ \
	E(0x05,op_else)  /* else */ \
	U(0x06)  \
	U(0x07)  \
	U(0x08)  \
	U(0x09)  \
	U(0x0a)  \
	E(0x0b,op_end)  /* end */ \
	E(0x0c,op_br)  /* br */ \
	E(0x0d,op_br_if)  /* br if */ \
	E(0x0e,op_br_table)  /* br table */ \
	E(0x0f,op_return)  /* return */ \
	E(0x10,op_call)  /* call */ \
	E(0x11,op_call_indirect)  /* call_indirect */ \
	U(0x12)  \
	U(0x13)  \
	U(0x14)  \
	U(0x15)  \
	U(0x16)  \
	U(0x17)  \
	U(0x18)  \
	U(0x19)  \
	E(0x1a,op_drop)  /* drop */ \
	E(0x1b,op_select)  /* select */ \
	U(0x1c)  \
	U(0x1d)  \
	U(0x1e)  \
	U(0x1f)  \
	E(0x20,op_get_local)  /* get_local */ \
	E(0x21,op_set_local)  /* set_local */ \
	E(0x22,op_tee_local)  /* tee_local */ \
	E(0x23,op_get_global)  /* get_global */ \
	E(0x24,op_set_global)  /* set_global */ \
	U(0x25)  \
	U(0x26)  \
	U(0x27)  \
	E(0x28,op_i32_load)  /* i32.load */ \
	E(0x29,op_i64_load)  /* i64.load */ \
	E(0x2a,op_f32_load)  /* f32.load */ \
	E(0x2b,op_f64_load)  /* f64.load */ \
	E(0x2c,op_i32_load8_s)  /* i32.load8_s */ \
	E(0x2d,op_i32_load8_u)  /* i32.load8_u */ \
	E(0x2e,op_i32_load16_s)  /* i32.load16_s */ \
	E(0x2f,op_i32_load16_u)  /* i32.load16_u */ \
	E(0x30,op_i64_load8_s)  /* i64.load8_s */ \
	E(0x31,op_i64_load8_u)  /* i64.load8_u */ \
	E(0x32,op_i64_load16_s)  /* i64.load16_s */ \
	E(0x33,op_i64_load16_u)  /* i64.load16_u */ \
	E(0x34,op_i64_load32_s)  /* i32.load32_s */ \
	E(0x35,op_i64_load32_u)  /* i32.load32_u */ \
	E(0x36,op_i32_store)  /* i32.store */ \
	E(0x37,op_i64_store)  /* i64.store */ \
	E(0x38,op_f32_store)  /* f32.store */ \
	E(0x39,op_f64_store)  /* f64.store */ \
	E(0x3a,op_i32_store8)  /* i32.store8 */ \
	E(0x3b,op_i32_store16)  /* i32.store16 */ \
	E(0x3c,op_i64_store8)  /* i64.store8 */ \
	E(0x3d,op_i64_store16)  /* i64.sotre16 */ \
	E(0x3e,op_i64_store32)  /* i64.store32 */ \
	E(0x3f,op_memory_size)  /* memory.size */ \
	E(0x40,op_memory_grow)  /* memory.grow */ \
	E(0x41,op_i32_const)  /* i32.const */ \
	E(0x42,op_i64_const)  /* i64.const */ \
	E(0x43,op_f32_const)  /* f32.const */ \
	E(0x44,op_f64_const)  /* f64.const */ \
	E(0x45,op_i32_eqz)  /* i32.eqz */ \
	E(0x46,op_i32_eq)  /* i32.eq */ \
	E(0x47,op_i32_ne)  /* i32.ne */ \
	E(0x48,op_i32_lt_s)  /* i32.lt_s */ \
	E(0x49,op_i32_lt_u)  /* i32.lt_u */ \
	E(0x4a,op_i32_gt_s)  /* i32.gt_s */ \
	E(0x4b,op_i32_gt_u)  /* i32.gt_u */ \
	E(0x4c,op_i32_le_s)  /* i32.le_s */ \
	E(0x4d,op_i32_le_u)  /* i32.le_u */ \
	E(0x4e,op_i32_ge_s)  /* i32.ge_s */ \
	E(0x4f,op_i32_ge_u)  /* i32.ge_u */ \
	E(0x50,op_i64_eqz)  /* i64.eqz */ \
	E(0x51,op_i64_eq)  /* i64.eq */ \
	E(0x52,op_i64_ne)  /* i64.ne */ \
	E(0x53,op_i64_lt_s)  /* i64.lt_s */ \
	E(0x54,op_i64_lt_u)  /* i64.lt_u */ \
	E(0x55,op_i64_gt_s)  /* i64.gt_s */ \
	E(0x56,op_i64_gt_u)  /* i64.gt_u */ \
	E(0x57,op_i64_le_s)  /* i64.le_s */ \
	E(0x58,op_i64_le_u)  /* i64.le_u */ \
	E(0x59,op_i64_ge_s)  /* i64.ge_s */ \
	E(0x5a,op_i64_ge_u)  /* i64.ge_u */ \
	E(0x5b,op_f32_eq)  /* f32.eq */ \
	E(0x5c,op_f32_ne)  /* f32.ne */ \
	E(0x5d,op_f32_lt)  /* f32.lt */ \
	E(0x5e,op_f32_gt)  /* f32.gt */ \
	E(0x5f,op_f32_le)  /* f32.le */ \
	E(0x60,op_f32_ge)  /* f32.ge */ \
	E(0x61,op_f64_eq)  /* f64.eq */ \
	E(0x62,op_f64_ne)  /* f64.ne */ \
	E(0x63,op_f64_lt)  /* f64.lt */ \
	E(0x64,op_f64_gt)  /* f64.gt */ \
	E(0x65,op_f64_le)  /* f64.le */ \
	E(0x66,op_f64_ge)  /* f64.ge */ \
	E(0x67,op_i32_clz)  /* i32.clz */ \
	E(0x68,op_i32_ctz)  /* i32.ctz */ \
	E(0x69,op_i32_popcnt)  /* i32.popcnt */ \
	E(0x6a,op_i32_add)  /* i32.add */ \
	E(0x6b,op_i32_sub)  /* i32.sub */ \
	E(0x6c,op_i32_mul)  /* i32.mul */ \
	E(0x6d,op_i32_div_s)  /* i32.div_s */ \
	E(0x6e,op_i32_div_u)  /* i32.div_u */ \
	E(0x6f,op_i32_rem_s)  /* i32.rem_s */ \
	E(0x70,op_i32_rem_u)  /* i32.rem_u */ \
	E(0x71,op_i32_and)  /* i32.and */ \
	E(0x72,op_i32_or)  /* i32.or */ \
	E(0x73,op_i32_xor)  /* i32.xor */ \
	E(0x74,op_i32_shl)  /* i32.shl */ \
	E(0x75,op_i32_shr_s)  /* i32.shr_s */ \
	E(0x76,op_i32_shr_u)  /* i32.shr_u */ \
	E(0x77,op_i32_rotl)  /* i32.rotl */ \
	E(0x78,op_i32_rotr)  /* i32.rotr */ \
	E(0x79,op_i64_clz)  /* i64.clz */ \
	E(0x7a,op_i64_ctz)  /* i64.ctz */ \
	E(0x7b,op_i64_popcnt)  /* i64.popcnt */ \
	E(0x7c,op_i64_add)  /* i64.add */ \
	E(0x7d,op_i64_sub)  /* i64.sub */ \
	E(0x7e,op_i64_mul)  /* i64.mul */ \
	E(0x7f,op_i64_div_s)  /* i64.div_s */ \
	E(0x80,op_i64_div_u)  /* i64.div_u */ \
	E(0x81,op_i64_rem_s)  /* i64.rem_s */ \
	E(0x82,op_i64_rem_u)  /* i64.rem_u */ \
	E(0x83,op_i64_and)  /* i64.and */ \
	E(0x84,op_i64_or)  /* i64.or */ \
	E(0x85,op_i64_xor)  /* i64.xor */ \
	E(0x86,op_i64_shl)  /* i64.shl */ \
	E(0x87,op_i64_shr_s)  /* i64.shr_s */ \
	E(0x88,op_i64_shr_u)  /* i64.shr_u */ \
	E(0x89,op_i64_rotl)  /* i64.rotl */ \
	E(0x8a,op_i64_rotr)  /* i64.rotr */ \
	E(0x8b,op_f32_abs)  /* f32.abs */ \
	E(0x8c,op_f32_neg)  /* f32.neg */ \
	E(0x8d,op_f32_ceil)  /* f32.ceil */ \
	E(0x8e,op_f32_floor)  /* f32.floor */ \
	E(0x8f,op_f32_trunc)  /* f32.trunc */ \
	E(0x90,op_f32_nearest)  /* f32.nearest */ \
	E(0x91,op_f32_sqrt)  /* f32.sqrt */ \
	E(0x92,op_f32_add)  /* f32.add */ \
	E(0x93,op_f32_sub)  /* f32.sub */ \
	E(0x94,op_f32_mul)  /* f32.mul */ \
	E(0x95,op_f32_div)  /* f32.div */ \
	E(0x96,op_f32_min)  /* f32.min */ \
	E(0x97,op_f32_max)  /* f32.max */ \
	E(0x98,op_f32_copysign)  /* f32.copysign */ \
	E(0x99,op_f64_abs)  /* f64.abs */ \
	E(0x9a,op_f64_neg)  /* f64.neg */ \
	E(0x9b,op_f64_ceil)  /* f64.ceil */ \
	E(0x9c,op_f64_floor)  /* f64.floor */ \
	E(0x9d,op_f64_trunc)  /* f64.trunc */ \
	E(0x9e,op_f64_nearest)  /* f64.nearest */ \
	E(0x9f,op_f64_sqrt)  /* f64.sqrt */ \
	E(0xa0,op_f64_add)  /* f64.add */ \
	E(0xa1,op_f64_sub)  /* f64.sub */ \
	E(0xa2,op_f64_mul)  /* f64.mul */ \
	E(0xa3,op_f64_div)  /* f64.div */ \
	E(0xa4,op_f64_min)  /* f64.min */ \
	E(0xa5,op_f64_max)  /* f64.max */ \
	E(0xa6,op_f64_copysign)  /* f64.copysign */ \
	E(0xa7,op_i32_wrap_i64)  /* i32.wrap/i64 */ \
	E(0xa8,op_i32_trunc_s_f32)  /* i32.trunc_s/f32 */ \
	E(0xa9,op_i32_trunc_u_f32)  /* i32.trunc_u/f32 */ \
	E(0xaa,op_i32_trunc_s_f64)  /* i32.trunc_s/f64 */ \
	E(0xab,op_i32_trunc_u_f64)  /* i32.trunc_u/f64 */ \
	E(0xac,op_i64_extend_s_i32)  /* i64.extend_s/i32 */ \
	E(0xad,op_i64_extend_u_i32)  /* i64.extend_u/i32 */ \
	E(0xae,op_i64_trunc_s_f32)  /* i64.trunc_s/f32 */ \
	E(0xaf,op_i64_trunc_u_f32)  /* i64.trunc_u/f32 */ \
	E(0xb0,op_i64_trunc_s_f64)  /* i64.trunc_s/f64 */ \
	E(0xb1,op_i64_trunc_u_f64)  /* i64.trunc_u/f64 */ \
	E(0xb2,op_f32_convert_s_i32)  /* f32.convert_s/i32 */ \
	E(0xb3,op_f32_convert_u_i32)  /* f32.convert_u/i32 */ \
	E(0xb4,op_f32_convert_s_i64)  /* f32.convert_s/i64 */ \
	E(0xb5,op_f32_convert_u_i64)  /* f32.convert_u/i64 */ \
	E(0xb6,op_f32_demote_f64)  /* f32.demote/f64 */ \
	E(0xb7,op_f64_convert_s_i32)  /* f64.convert_s/i32 */ \
	E(0xb8,op_f64_convert_u_i32)  /* f64.convert_u/i32 */ \
	E(0xb9,op_f64_convert_s_i64)  /* f64.convert_s/i64 */ \
	E(0xba,op_f64_convert_u_i64)  /* f64.convert_u/i64 */ \
	E(0xbb,op_f64_promote_f32)  /* f64.promote/f32 */ \
	E(0xbc,op_i32_reinterpret_f32)  /* i32.reinterpret/f32 */ \
	E(0xbd,op_i64_reinterpret_f64)  /* i64.reinterpret/f64 */ \
	E(0xbe,op_f32_reinterpret_i32)  /* f32.reinterpret/i32 */ \
	E(0xbf,op_f64_reinterpret_i64)  /* f64.reinterpret/i64 */ \
	E(0xc0,op_i32_extend8_s)   /* i32.extend8_s */ \
	E(0xc1,op_i32_extend16_s)   /* i32.extend16_s */ \
	E(0xc2,op_i64_extend8_s)   /* i64.extend8_s */ \
	E(0xc3,op_i64_extend16_s)   /* i64.extend16_s */ \
	E(0xc4,op_i64_extend32_s)   /* i64.extend32_s */ \
	E(0xc5,op_drop_64)  \
	E(0xc6,op_select_64)  \
	E(0xc7,op_ext_op_get_local_fast)  \
	E(0xc8,op_ext_op_set_local_fast_i64)  \
	E(0xc9,op_ext_op_set_local_fast)  \
	E(0xca,op_ext_op_tee_local_fast)  \
	E(0xcb,op_ext_op_tee_local_fast_i64)  \
	E(0xcc,op_ext_op_copy_stack_top)  \
	E(0xcd,op_ext_op_copy_stack_top_i64)  \
	E(0xce,op_impdep)  \
	U(0xcf)  \
	U(0xd0)  \
	U(0xd1)  \
	U(0xd2)  \
	U(0xd3)  \
	U(0xd4)  \
	U(0xd5)  \
	U(0xd6)  \
	U(0xd7)  \
	U(0xd8)  \
	U(0xd9)  \
	U(0xda)  \
	U(0xdb)  \
	U(0xdc)  \
	U(0xdd)  \
	U(0xde)  \
	U(0xdf)  \
	U(0xe0)  \
	U(0xe1)  \
	U(0xe2)  \
	U(0xe3)  \
	U(0xe4)  \
	U(0xe5)  \
	U(0xe6)  \
	U(0xe7)  \
	U(0xe8)  \
	U(0xe9)  \
	U(0xea)  \
	U(0xeb)  \
	U(0xec)  \
	U(0xed)  \
	U(0xee)  \
	U(0xef)  \
	U(0xf0)  \
	U(0xf1)  \
	U(0xf2)  \
	U(0xf3)  \
	U(0xf4)  \
	U(0xf5)  \
	U(0xf6)  \
	U(0xf7)  \
	U(0xf8)  \
	U(0xf9)  \
	U(0xfa)  \
	U(0xfb)  \
	E(0xfc,op_misc_prefix)  \
	E(0xfd,op_simd_prefix)  \
	E(0xfe,op_thread_prefix)  \
	E(0xff,op_reserved_prefix)

#define LYWR_OP_ENUM(OP,NAME) lywr_##NAME = OP,
#define LYWR_OP_ENUM_UNUSE2(OP,NAME,...) lywr_##NAME = OP,
#define LYWR_OP_ENUM_UNUSE(OP,...) LYWR_OP_ENUM_UNUSE2(OP,##__VA_ARGS__,op_unused_##OP)

typedef enum lywr_opcode {
	LYWR_OP_DEFINE(LYWR_OP_ENUM,LYWR_OP_ENUM_UNUSE)
} lywr_opcode;

const char* lywr_opcode_name(int code);

LYWR_NAMESPACE_END

#endif

