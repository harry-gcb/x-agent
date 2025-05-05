clang -E hello_kern.c -o hello_kernel_precompile.c

/usr/bin/clang -g -O2 -target bpf -D__TARGET_ARCH_x86 -idirafter /usr/lib/llvm-14/lib/clang/14.0.0/include -idirafter /usr/local/include -idirafter /usr/include/x86_64-linux-gnu -idirafter /usr/include -isystem /home/harry/workspace/x-agent/build/cmake/libbpf -c hello_kern.c -o hello_kern.o