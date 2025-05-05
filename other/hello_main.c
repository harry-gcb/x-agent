int main() {
    struct bpf_link *link = NULL;
    struct bpf_program *prog = NULL;
    struct bpf_object *obj = NULL;

    obj = bpf_object__open_file("hello_kern.o", NULL); // 打开文件
    prog = bpf_object__find_program_by_name(obj, "bpf_prog");   // 找到program
    bpf_object__load(obj);  // 加载到内核
    link = bpf_program__attach(prog);   // 生效 enable
cleanup:
    bpf_link__destroy(link);
    bpf_object__close(obj);
    return 0;
}