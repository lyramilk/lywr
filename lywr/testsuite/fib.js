function fib2(n)
{
	return n < 2?n:fib2(n-1)+fib2(n-2);
}

console.log(fib2(45));