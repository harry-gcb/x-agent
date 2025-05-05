#ifndef OFFCPUTIME_H_
#define OFFCPUTIME_H_

#define TASK_COMM_LEN 16
#define MAX_PID_NR    30
#define MAX_TID_NR    30

struct key_t {
    __u32 pid;
    __u32 tgid;
    int user_stack_id;
    int kern_stack_id;
};

struct val_t {
    __u64 delta;
    char comm[TASK_COMM_LEN];
};

#endif