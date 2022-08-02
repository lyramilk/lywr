unsigned long long plus(unsigned long long n)
{
	unsigned long long u = 0;
	for(unsigned long long i=0;i<n;++i){

		unsigned long long a = 0;
		for(unsigned long long ai=0;ai<i;++ai){
			a += ai;
		}

		unsigned long long b = 0;
		for(unsigned long long bi=0;bi<i - 1;++bi){
			b += bi;
		}
		u += a + b;
	}
	return u;
}
/*
#include <stdlib.h>
#include <stdio.h>
int main(int argc,const char* argv[]){
	if(argc > 1){
		int n = atoi(argv[1]);
		printf("fib2(%d)=%llu\n",n,fib2(n));
	}

	return 0;
}
*/