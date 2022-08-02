#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
     srand( time(0));
	 int ix = rand();
	 if(ix % 2 == 0){
		printf("hello, world! %d\n",ix);
	 }else{
		printf("世界你好:%d\n",ix);
	 }
     return 0;
}
