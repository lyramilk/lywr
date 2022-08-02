#ifndef _lyramilk_lywr_rbtree2_h_
#define _lyramilk_lywr_rbtree2_h_

#include "lywr_defs.h"
#include "lywr_mempool.h"
LYWR_NAMESPACE_START

typedef struct lywr_rbtree2_iter lywr_rbtree2_iter;
typedef struct lywr_rbtree2_bucket lywr_rbtree2_bucket;

typedef struct lywr_rbtree2_node
{
	const void* data;
	struct lywr_rbtree2_node* left;
	struct lywr_rbtree2_node* right;
	struct lywr_rbtree2_node* parent;
	unsigned char color:1;	//	0黑	1红
	long cc;	//	子结点数
} lywr_rbtree2_node;

typedef int (*lywr_rbtree2_compator)(const void* a,const void* b);
typedef int (*lywr_rbtree2_lookup)(lywr_rbtree2_node* node,int depth,int idx,void* userdata);

typedef struct lywr_rbtree2_ctx
{
	struct lywr_rbtree2_node* root;
	struct lywr_rbtree2_node* min;
	struct lywr_rbtree2_node* max;
	lywr_rbtree2_compator compator;
	long size;

	struct lywr_rbtree2_bucket* mpool;
	struct lywr_rbtree2_node* reserve;
	long capacity;


	lywr_pool* memorypool;
	lywr_free_cbk pfree;
	void* uptr;
}lywr_rbtree2_ctx;

enum lywr_rbtree2_ec{
	lywr_rbtree2_ok,			//成功
	lywr_rbtree2_update,		//插入时发现己存在，所以更新
	lywr_rbtree2_fail,		//失败不解释
	lywr_rbtree2_oom,		//malloc失败
	lywr_rbtree2_notfound,	//没有找到
	lywr_rbtree2_end,		//迭代完成
};


/// root的属性将会被重置且不涉及任何指针的内存释放。
enum lywr_rbtree2_ec lywr_rbtree2_init(struct lywr_rbtree2_ctx* ctx,lywr_pool* memorypool);
enum lywr_rbtree2_ec lywr_rbtree2_destory(struct lywr_rbtree2_ctx* ctx);

///	old在发生lywr_rbtree2_update时被赋值为替换之前的旧值。
enum lywr_rbtree2_ec lywr_rbtree2_insert(struct lywr_rbtree2_ctx* ctx,const void* data,const void** old);
enum lywr_rbtree2_ec lywr_rbtree2_remove(struct lywr_rbtree2_ctx* ctx,const void* key,const void** old);
enum lywr_rbtree2_ec lywr_rbtree2_get(struct lywr_rbtree2_ctx* ctx,const void* key,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_at(struct lywr_rbtree2_ctx* ctx,long rank,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_rank(struct lywr_rbtree2_ctx* ctx,const void* key,long* rank);

enum lywr_rbtree2_ec lywr_rbtree2_scan_init(struct lywr_rbtree2_ctx* ctx,struct lywr_rbtree2_iter** iter);
enum lywr_rbtree2_ec lywr_rbtree2_scan_reset(struct lywr_rbtree2_ctx* ctx,struct lywr_rbtree2_iter* iter);
enum lywr_rbtree2_ec lywr_rbtree2_scan_seek(struct lywr_rbtree2_iter* iter,const void* key,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_scan_seek_rank(struct lywr_rbtree2_iter* iter,long rank,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_scan_next(struct lywr_rbtree2_iter* iter,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_scan_last(struct lywr_rbtree2_iter* iter,const void** data);
enum lywr_rbtree2_ec lywr_rbtree2_scan_destory(struct lywr_rbtree2_ctx* ctx,struct lywr_rbtree2_iter* iter);

enum lywr_rbtree2_ec lywr_rbtree2_verbose(struct lywr_rbtree2_ctx* ctx,lywr_rbtree2_lookup lookup_call_back,void* userdata);


LYWR_NAMESPACE_END

#endif
