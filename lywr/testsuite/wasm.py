import pywasm
pywasm.on_debug();


def clock_time_get(a,b,c,d):
	print("clock_time_get",a,b,c,d);
	return 0;

def proc_exit(obj,c):
	exit(c);

def fd_write(obj,fd,iov,iovcnt,nwrittenptr):
	print("fd_write",fd,iov,iovcnt,nwrittenptr);
	return 1;



vm = pywasm.load('O2.wasm',{
		"wasi_snapshot_preview1":{
			"clock_time_get":clock_time_get,
			"proc_exit":proc_exit,
			"fd_write":fd_write,
		}
	}
)
r=vm.exec('_start',[]);
print(r);
