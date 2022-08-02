unsigned long long fib(unsigned long long n)
{
	if(n < 3){
		if(n == 0) return 0;
		if(n == 1) return 1;
		if(n == 2) return 1;
	}
	unsigned long long a = 1;
	unsigned long long b = 1;
	unsigned long long c = 0;
	for (unsigned long long i = 3; i <= n; i++)
	{
		c = a + b;
		a = b;
		b = c;
	}

	return c;
}


unsigned long long fib2(unsigned long long n)
{
	return n < 2?n:fib2(n-1)+fib2(n-2);
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