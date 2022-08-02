#ifndef _lyramilk_lywr_data_hh_
#define _lyramilk_lywr_data_hh_

#include "lywr_defs.h"
#include "lywr.hh"
#include "lywr_util.hh"

LYWR_NAMESPACE_START


#define nullptr (void*)0


/*
lywrrc lywr_datastack_init(lywr_datastack* st,lywr_pool* mempool);
lywrrc lywr_datastack_destory(lywr_datastack* st);

long long lywr_datastack_bottom(lywr_datastack* st);
lywrrc lywr_datastack_at(lywr_datastack* st,unsigned long long index,lywr_data_cell** cell);

lywrrc lywr_datastack_push_u32(lywr_datastack* st,unsigned int rv);
lywrrc lywr_datastack_push_u64(lywr_datastack* st,unsigned long long rv);
lywrrc lywr_datastack_push_f32(lywr_datastack* st,float rv);
lywrrc lywr_datastack_push_f64(lywr_datastack* st,double rv);

lywrrc lywr_datastack_pop_u32(lywr_datastack* st,unsigned int* rv,int keep);
lywrrc lywr_datastack_pop_u64(lywr_datastack* st,unsigned long long* rv,int keep);
lywrrc lywr_datastack_pop_f32(lywr_datastack* st,float* rv,int keep);
lywrrc lywr_datastack_pop_f64(lywr_datastack* st,double* rv,int keep);


lywrrc lywr_instruction_stack_init(lywr_instruction_stack* st,lywr_pool* mempool);
lywrrc lywr_instruction_stack_reset(lywr_instruction_stack* st);
lywrrc lywr_instruction_stack_destory(lywr_instruction_stack* st);
lywrrc lywr_instruction_stack_push(lywr_instruction_stack* st,const lywr_instruction** ptr);
lywrrc lywr_instruction_stack_pop(lywr_instruction_stack* st,const lywr_instruction** buf,int keep);


lywrrc lywr_callframe_stack_init(lywr_callframe_stack* st,lywr_pool* mempool);
lywrrc lywr_callframe_stack_destory(lywr_callframe_stack* st);
lywrrc lywr_callframe_stack_next(lywr_callframe_stack* st,lywr_callframe** bottom);	//取得下一帧
lywrrc lywr_callframe_stack_prev(lywr_callframe_stack* st,lywr_callframe** bottom);	//取得上一帧



lywrrc lywr_data_array_init(lywr_data_array* st,unsigned int capacity,lywr_pool* mempool);
lywrrc lywr_data_array_reset(lywr_data_array* st,unsigned int capacity);
lywrrc lywr_data_array_destory(lywr_data_array* st);

lywrrc lywr_data_array_at(lywr_data_array* st,unsigned int index,lywr_data_cell** data);


lywrrc lywr_linearmemory_init(lywr_linearmemory* st,lywr_pool* mempool,unsigned int initial,unsigned int maximum);
lywrrc lywr_linearmemory_destory(lywr_linearmemory* st);
lywrrc lywr_linearmemory_grew(lywr_linearmemory* st,unsigned int newpagecount);



*/






///	lywr_datastack

lywrrc static lywr_datastack_init(lywr_datastack* st,lywr_pool* mempool)
{
	st->mempool = mempool;

	//st->size = 1024;
	st->size = 1;
	st->start = lywr_pool_malloc(mempool,st->size * sizeof(*st->start));
	st->cursor = 0;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_destory(lywr_datastack* st)
{
	lywr_pool_free(st->mempool,st->start);
	return lywrrc_ok;
}

lywrrc static lywr_datastack_realloc(lywr_datastack* st)
{
	unsigned long long newsize = st->size + 1;
	unsigned char* start2 = lywr_pool_malloc(st->mempool,newsize * sizeof(*st->start));
	lywr_memcpy(start2,(const unsigned char*)st->start,st->size * sizeof(*st->start));
	lywr_pool_free(st->mempool,st->start);
	st->start = (lywr_data_cell*)start2;

	st->size = newsize;
	return lywrrc_ok;
}

long long static lywr_datastack_bottom(lywr_datastack* st)
{
	return st->cursor;
}

lywrrc static lywr_datastack_at(lywr_datastack* st,unsigned long long index,lywr_data_cell** cell)
{
	if(index >= st->size){
		lywrrc rc = lywr_datastack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	*cell = st->start + index;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_push_u32(lywr_datastack* st,unsigned int rv)
{
	if(st->cursor >= st->size){
		lywrrc rc = lywr_datastack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	st->start[st->cursor].u32 = rv;
	++st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_push_u64(lywr_datastack* st,unsigned long long rv)
{
	if(st->cursor >= st->size){
		lywrrc rc = lywr_datastack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	st->start[st->cursor].u64 = rv;
	++st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_push_f32(lywr_datastack* st,float rv)
{
	if(st->cursor >= st->size){
		lywrrc rc = lywr_datastack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	st->start[st->cursor].f32 = rv;
	++st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_push_f64(lywr_datastack* st,double rv)
{
	if(st->cursor >= st->size){
		lywrrc rc = lywr_datastack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	st->start[st->cursor].f64 = rv;
	++st->cursor;
	return lywrrc_ok;
}


lywrrc static lywr_datastack_pop_u32(lywr_datastack* st,unsigned int* rv,int keep)
{
	if(st->cursor < 1) return lywrrc_oom;
	*rv = st->start[st->cursor - 1].u32;
	if(keep == 0) --st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_pop_u64(lywr_datastack* st,unsigned long long* rv,int keep)
{
	if(st->cursor < 1) return lywrrc_oom;
	*rv = st->start[st->cursor - 1].u64;
	if(keep == 0) --st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_pop_f32(lywr_datastack* st,float* rv,int keep)
{
	if(st->cursor < 1) return lywrrc_oom;
	*rv = st->start[st->cursor - 1].f32;
	if(keep == 0) --st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_datastack_pop_f64(lywr_datastack* st,double* rv,int keep)
{
	if(st->cursor < 1) return lywrrc_oom;
	*rv = st->start[st->cursor - 1].f64;
	if(keep == 0) --st->cursor;
	return lywrrc_ok;
}



///	lywr_instruction_stack

lywrrc static lywr_instruction_stack_init(lywr_instruction_stack* st,lywr_pool* mempool)
{
	st->mempool = mempool;
	st->size = 64;
	st->start = lywr_pool_malloc(mempool,st->size * sizeof(*st->start));
	st->cursor = 0;
	return lywrrc_ok;
}

lywrrc static lywr_instruction_stack_reset(lywr_instruction_stack* st)
{
	st->cursor = 0;
	return lywrrc_ok;
}


lywrrc static lywr_instruction_stack_destory(lywr_instruction_stack* st)
{
	lywr_pool_free(st->mempool,st->start);
	return lywrrc_ok;

}

lywrrc static lywr_instruction_stack_realloc(lywr_instruction_stack* st)
{
	unsigned long long newsize = st->size << 1;
	unsigned char* start2 = lywr_pool_malloc(st->mempool,newsize * sizeof(*st->start));
	lywr_memcpy(start2,(const unsigned char*)st->start,st->size * sizeof(*st->start));
	lywr_pool_free(st->mempool,st->start);
	st->start = (lywr_instruction const **)start2;

	st->size = newsize;
	return lywrrc_ok;
}

lywrrc static lywr_instruction_stack_push(lywr_instruction_stack* st,const lywr_instruction** ptr)
{
	if(st->cursor >= st->size){
		lywrrc rc = lywr_instruction_stack_realloc(st);
		if(rc != lywrrc_ok) return rc;
	}

	st->start[st->cursor] = *ptr;
	++st->cursor;
	return lywrrc_ok;
}

lywrrc static lywr_instruction_stack_pop(lywr_instruction_stack* st,const lywr_instruction** buf,int keep)
{
	if(st->cursor < 1) return lywrrc_oom;
	*buf = st->start[st->cursor - 1];
	if(keep == 0) --st->cursor;
	return lywrrc_ok;
}





lywrrc static lywr_data_array_init(lywr_data_array* st,unsigned int capacity,lywr_pool* mempool)
{
	st->blucksize = sizeof(lywr_data);
	st->mempool = mempool;
	TRACE("数组初始化大小:%d\n",capacity);

	st->size = capacity;
	if(st->size > 0){
		st->ptr = lywr_pool_malloc(st->mempool,st->size * st->blucksize);
	}
	return lywrrc_ok;
}

lywrrc static lywr_data_array_reset(lywr_data_array* st,unsigned int capacity)
{
	if(capacity <= st->size){
		return lywrrc_ok;
	}
	lywr_pool_free(st->mempool,st->ptr);


	st->size = capacity;
	if(st->size > 0){
		st->ptr = lywr_pool_malloc(st->mempool,st->size * st->blucksize);
	}
	return lywrrc_ok;
}


lywrrc static lywr_data_array_destory(lywr_data_array* st)
{
	if(st->size > 0){
		lywr_pool_free(st->mempool,st->ptr);
	}
	return lywrrc_ok;
}



lywrrc static lywr_data_array_set(lywr_data_array* st,unsigned int index,lywr_data_cell* data)
{
	if(st->size <= index){
		TRACE("数组大小:%d,请求的下标:%d，数组越界！！！\n",st->size,index);
		return lywrrc_overflow;
	}
	TRACE("数组大小:%d,请求的下标:%d，正常\n",st->size,index);
	st->ptr[index].u64 = data->u64;
	//lywr_memcpy((unsigned char*)(st->ptr) + index * st->blucksize,(unsigned char*)data,st->blucksize);
	return lywrrc_ok;
}

lywrrc static lywr_data_array_get(lywr_data_array* st,unsigned int index,lywr_data_cell* data)
{
	if(st->size <= index){
		TRACE("获取位置%d/%d\n",index,st->size);
		return lywrrc_not_found;
	}
	data->u64 = st->ptr[index].u64;
	//lywr_memcpy((unsigned char*)data,(unsigned char*)(st->ptr) + index * st->blucksize,st->blucksize);
	return lywrrc_ok;
}

lywrrc static lywr_data_array_at(lywr_data_array* st,unsigned int index,lywr_data_cell** data)
{
	if(st->size <= index){
		TRACE("获取位置%d/%d\n",index,st->size);
		return lywrrc_not_found;
	}
	*data = &st->ptr[index];
	//lywr_memcpy((unsigned char*)data,(unsigned char*)(st->ptr) + index * st->blucksize,st->blucksize);
	return lywrrc_ok;
}












lywrrc static lywr_callframe_stack_init(lywr_callframe_stack* st,lywr_pool* mempool)
{
	st->mempool = mempool;
	st->bottom = nullptr;
	return lywrrc_ok;
}

lywrrc static lywr_callframe_stack_destory(lywr_callframe_stack* st)
{
	lywr_callframe_stack_node* node = st->bottom;
	while(node->child){
		node = node->child;
	}



	while(node->parent){
		node = node->parent;
		lywr_instruction_stack_destory(&node->frame.runblock);
		lywr_data_array_destory(&node->frame.local);
		lywr_pool_free(st->mempool,node->child);
	}


	lywr_instruction_stack_destory(&node->frame.runblock);
	lywr_data_array_destory(&node->frame.local);
	lywr_pool_free(st->mempool,node);

	return lywrrc_ok;

}

lywrrc static lywr_callframe_stack_next(lywr_callframe_stack* st,lywr_callframe** bottom)
{
	if(st->bottom == nullptr){
		lywr_callframe_stack_node* node = lywr_pool_malloc(st->mempool,sizeof(*st->bottom));
		if(node == nullptr) return lywrrc_oom;

		node->parent = nullptr;
		node->child = nullptr;

		lywr_instruction_stack_init(&node->frame.runblock,st->mempool);
		lywr_data_array_init(&node->frame.local,36,st->mempool);

		st->bottom = node;
		*bottom = &st->bottom->frame;
		return lywrrc_ok;
	}

	if(st->bottom->child){
		st->bottom = st->bottom->child;
		*bottom = &st->bottom->frame;
		return lywrrc_ok;
	}else{
		lywr_callframe_stack_node* node = lywr_pool_malloc(st->mempool,sizeof(*st->bottom));
		if(node == nullptr) return lywrrc_oom;

		lywr_instruction_stack_init(&node->frame.runblock,st->mempool);
		lywr_data_array_init(&node->frame.local,36,st->mempool);

		node->parent = st->bottom;
		node->child = nullptr;
		st->bottom->child = node;
		st->bottom = node;
		*bottom = &st->bottom->frame;
		return lywrrc_ok;
	}
}

lywrrc static lywr_callframe_stack_prev(lywr_callframe_stack* st,lywr_callframe** bottom)
{
	if(st->bottom == nullptr){
		return lywrrc_eof;
	}
	if(st->bottom->parent == nullptr){
		return lywrrc_eof;
	}
	st->bottom = st->bottom->parent;
	*bottom = &st->bottom->frame;
	return lywrrc_ok;
}













#define LYWR_PAGE_COUNT	65536

lywrrc static lywr_linearmemory_init(lywr_linearmemory* st,lywr_pool* mempool,unsigned int initial,unsigned int maximum)
{
	st->mempool = mempool;
	st->pagecount = initial;
	st->maximum = maximum;
	st->size = LYWR_PAGE_COUNT * st->pagecount;
	TRACE("=============================================================分配了size=%lld\n",st->size);

	st->start = lywr_pool_malloc(mempool,st->size);
	lywr_memset(st->start,0,st->size);
	return lywrrc_ok;
}
lywrrc static lywr_linearmemory_destory(lywr_linearmemory* st)
{
	lywr_pool_free(st->mempool,st->start);
	return lywrrc_ok;
}

lywrrc static lywr_linearmemory_grew(lywr_linearmemory* st,unsigned int newpagecount)
{
	if(newpagecount > st->maximum){
		return lywrrc_oom;
	}

	long long newsize = (st->pagecount + newpagecount) * LYWR_PAGE_COUNT;

	unsigned char* start2 = lywr_pool_malloc(st->mempool,newsize);
	lywr_memcpy(start2,st->start,st->size);
	lywr_pool_free(st->mempool,st->start);
	st->start = start2;

	st->pagecount += newpagecount;
	st->size = newsize;
	return lywrrc_ok;
}














LYWR_NAMESPACE_END

#endif
