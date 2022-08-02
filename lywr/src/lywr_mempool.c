#include "lywr.hh"
#include "lywr_stream.hh"
#include "lywr_util.hh"

#define nullptr (void*)0

LYWR_NAMESPACE_START

typedef struct lywr_poolx_node{
	struct lywr_poolx_node* prev;
}lywr_poolx_node;

typedef struct lywr_poolx{
	lywr_malloc_cbk al;
	lywr_free_cbk fr;
	void* userptr;
	lywr_poolx_node* root;
}lywr_poolx;

typedef struct lywr_pool{
	lywr_malloc_cbk al;
	lywr_free_cbk fr;
	void* userptr;
}lywr_pool;



lywr_pool* lywr_pool_create(lywr_malloc_cbk al,lywr_free_cbk fr,void* userptr)
{
	lywr_pool* p = (lywr_pool*)al(sizeof(lywr_pool),userptr);
	p->al = al;
	p->fr = fr;
	p->userptr = userptr;
	return p;
}

void lywr_pool_destory(lywr_pool* pool)
{
	pool->fr(pool,pool->userptr);
}

void* lywr_pool_malloc(lywr_pool* pool,long long buffsize)
{
#ifdef _DEBUG
	void* ptr = pool->al(buffsize,pool->userptr);
	if(ptr == nullptr) return ptr;
	return lywr_memset(ptr,0xcc,buffsize);
#else
	return pool->al(buffsize,pool->userptr);
#endif
}

void lywr_pool_free(lywr_pool* pool,void* buff)
{
	pool->fr(buff,pool->userptr);
}







lywr_poolx* lywr_poolx_create(lywr_malloc_cbk al,lywr_free_cbk fr,void* userptr)
{
	lywr_poolx* p = (lywr_poolx*)al(sizeof(lywr_poolx),userptr);
	if(p == nullptr) return nullptr;
	p->al = al;
	p->fr = fr;
	p->userptr = userptr;
	p->root = nullptr;
	return p;
}

void lywr_poolx_destory(lywr_poolx* pool)
{
	lywr_poolx_node* node = pool->root;

	while(node){
		lywr_poolx_node* nodefree = node;
		node = node->prev;
		pool->fr(nodefree,pool->userptr);
	}
	pool->fr(pool,pool->userptr);
}

void* lywr_poolx_malloc(lywr_poolx* pool,long long buffsize)
{
	lywr_poolx_node* node = pool->al(buffsize + sizeof(lywr_poolx_node),pool->userptr);
	if(node == nullptr) return nullptr;
	node->prev = pool->root;
	pool->root = node;
	return sizeof(lywr_poolx_node) + (char*)node;
}


LYWR_NAMESPACE_END
