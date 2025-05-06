#include "agent.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct config_t {
    bool debug;
};
const volatile struct config_t g_config = {};
/*============================= INTERNAL STRUCTS ============================*/
typedef struct unwind_tables_key_t {
    int pid;
    int shard;
} unwind_tables_key_t;

/*============================== MACROS =====================================*/
#define BPF_MAP(_name, _type, _key_type, _value_type, _max_entries) \
    struct {                                                        \
        __uint(type, _type);                                        \
        __type(key, _key_type);                                     \
        __type(value, _value_type );                                \
        __uint(max_entries, _max_entries);                          \
    } _name SEC(".maps")

#define BPF_HASH(_name, _key_type, _value_type, _max_entries)   \
    BPF_MAP(_name, BPF_MAP_TYPE_HASH, _key_type, _value_type, _max_entries)

/*================================ MAPS =====================================*/
BPF_HASH(debug_pids, int, u8, 32);
BPF_HASH(unwind_tables, unwind_tables_key_t, stack_unwind_table_t, MAX_PID_MAP_SIZE);



static __always_inline bool is_debug_enabled_for_pid(int pid) {
    void *val = bpf_map_lookup_elem(&debug_pids, &pid);
    if (val) {
        return true;
    }
    return false;
}

static __always_inline bool has_unwind_information(pid_t pid) {
    unwind_tables_key_t key = {.pid = pid, .shard = 0};
    stack_unwind_table_t *shard1 = bpf_map_lookup_elem(&unwind_tables, &key);
    if (shard1) {
        return true;
    }
    return false;
}

SEC("perf_event")
int profile_cpu(struct bpf_perf_event_data *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    int user_pid = pid_tgid;
    int user_tgid = pid_tgid >> 32;
    
    if (user_pid == 0) {
        return 0;
    }

    if (g_config.debug) {
        bpf_printk("debug mode enabled, make sure you specified process name");
        if (!is_debug_enabled_for_pid(user_tgid)) {
            return 0;
        }
    }
    bool has_unwind_info = has_unwind_information(user_pid);
    if (!has_unwind_info) {
        add_stacks(ctx, pid_tgid, STACK_WALKING_METHOD_FP, NULL);
    } else {
        stack_unwind_table_t *unwind_table = find_unwind_table(user_pid, ctx->regs.ip);
        if (unwind_table == NULL) {
            bpf_printk("IP not covered. In kernel space / bug? IP %llx)", ctx->regs.ip);
            BUMP_UNWIND_PC_NOT_COVERED_ERROR();
            return 0;
        }
        u64 last_idx = unwind_table->table_len - 1;
        if (last_idx < 0 || last_idx >= MAX_UNWIND_TABLE_SIZE) {
            bpf_printk("\t[error] this should never happen");
            BUMP_UNWIND_SHOULD_NEVER_HAPPEN_ERROR();
            return 0;
        }
        show_row(unwind_table, 0);
        show_row(unwind_table, 1);
        show_row(unwind_table, 2);
        show_row(unwind_table, last_idx);
        bpf_printk("pid %d tgid %d", user_pid, user_tgid);
        walk_user_stacktrace(ctx);
    }
    return 0;
}