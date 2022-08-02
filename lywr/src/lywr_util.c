#include "lywr_util.hh"

LYWR_NAMESPACE_START


int lywr_memcmp2(const unsigned char* a,unsigned long long an,const unsigned char* b,unsigned long long bn)
{
	int i;
	for(i=0;i<an && i<bn;++i){
		unsigned char c = a[i] - b[i];
		if(c != 0){
			if(c&0x80){
				return -1;
			}else{
				return 1;
			}
		}
	}

	if(an > bn){
		return 1;
	}else if(an < bn){
		return -1;
	}
	return 0;
}

int lywr_memcpy(unsigned char* a,const unsigned char* b,unsigned long long count)
{
	// 以64位模式复制前面能被8整除的长度部分。
	unsigned long long ic = count>>3;
	if(ic > 0){
		unsigned long long* ia = (unsigned long long*)a;
		unsigned long long* ib = (unsigned long long*)b;
		unsigned long long i;
		for(i =0;i < ic;++i){
			ia[i] = ib[i];
		}
	}

	// 长度的余数用普通的方式复制。
	unsigned long long i;
	for(i = ic>>3;i < count;++i){
		a[i] = b[i];
	}
	return 0;
}

unsigned char* lywr_memset(unsigned char* a,unsigned char b,unsigned long long count)
{
	if(count > 8){
		union{
			unsigned long long c;
			unsigned char bs[8];
		}bb;

		bb.bs[0] = b;
		bb.bs[1] = b;
		bb.bs[2] = b;
		bb.bs[3] = b;
		bb.bs[4] = b;
		bb.bs[5] = b;
		bb.bs[6] = b;
		bb.bs[7] = b;

		unsigned long long* qi = (unsigned long long*)a;
		unsigned long long count8 = count>>3;
		unsigned long long i;
		for(i=0;i<count8;++i){
			qi[i] = bb.c;
		}
		for(i=count8<<3;i<count;++i){
			a[i] = b;
		}
	}else{
		unsigned long long i;
		for(i=0;i<count;++i){
			a[i] = b;
		}
	}
	/*
		for(unsigned long long i=0;i<count;++i){
			a[i] = b;
		}
	*/

	return a;
}


LYWR_NAMESPACE_END
