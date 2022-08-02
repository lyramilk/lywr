/*
javac fib.java -d . && time java -cp . test.fib 30
*/

package test;

class fib
{

	public static long fib2(long n)
	{
		return n < 2?n:fib2(n-1)+fib2(n-2);
	}

	public static void main(String[] args)
	{
		if(args.length > 0){
			System.out.println(fib2(Long.parseLong(args[0])));
		}else{
			System.out.println("需要一个整数参数");
		}
	}
}