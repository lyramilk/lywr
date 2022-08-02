#ifndef _lyramilk_lywr_mempool_hh_
#define _lyramilk_lywr_mempool_hh_

LYWR_NAMESPACE_START

//	内存池
typedef struct lywr_pool lywr_pool;		//支持释放
typedef struct lywr_poolx lywr_poolx;	//不支持释放

//	通过回调函数向外界申请内存。
typedef void* (*lywr_malloc_cbk)(long long buffsize,void* userptr);
//	通过回调函数释放从外界申请的内存。
typedef void (*lywr_free_cbk)(void* buff,void* userptr);


// 使用分配和释放内存的回调函数创建内存池
lywr_poolx* lywr_poolx_create(lywr_malloc_cbk al,lywr_free_cbk fr,void* userptr);

//	销毁内存池，销毁的时候可以把所有池中分配的内存全部回收。无论是否己调用过free。
void lywr_poolx_destory(lywr_poolx* pool);


//	申请内存
void* lywr_poolx_malloc(lywr_poolx* pool,long long buffsize);






// 使用分配和释放内存的回调函数创建内存池
lywr_pool* lywr_pool_create(lywr_malloc_cbk al,lywr_free_cbk fr,void* userptr);

//	销毁内存池，销毁的时候可以把所有池中分配的内存全部回收。无论是否己调用过free。
void lywr_pool_destory(lywr_pool* pool);

//	申请内存
void* lywr_pool_malloc(lywr_pool* pool,long long buffsize);
//	释放内存
void lywr_pool_free(lywr_pool* pool,void* buff);


LYWR_NAMESPACE_END

#endif
