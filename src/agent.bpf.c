#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "agent.h"

const char LICENSE[] SEC("license") = "Dual BSD/GPL";

const volatile bool config_verbose = false;
/*============================== MACROS =====================================*/
// Number of frames to walk per tail call iteration.
#define MAX_STACK_DEPTH_PER_PROGRAM 15
// Size of the unwind table.
#define MAX_UNWIND_TABLE_SIZE 250 * 1000
// Size of the `<(pid, shard_id), unwind_table>` mapping. Determines how many
// processes we can unwind.
#define MAX_PID_MAP_SIZE 100

// Stack walking methods.
enum stack_walking_method {
    STACK_WALKING_METHOD_FP = 0,
    STACK_WALKING_METHOD_DWARF = 1,
};

#define BPF_MAP(_name, _type, _key_type, _value_type, _max_entries) \
    struct {                                                        \
        __uint(type, _type);                                        \
        __type(key, _key_type);                                     \
        __type(value, _value_type );                                \
        __uint(max_entries, _max_entries);                          \
    } _name SEC(".maps")

#define BPF_HASH(_name, _key_type, _value_type, _max_entries)   \
    BPF_MAP(_name, BPF_MAP_TYPE_HASH, _key_type, _value_type, _max_entries)

#define DEFINE_COUNTER(__func__name)                            \
static void BUMP_##__func__name() {                             \
    u32 *c = bpf_map_lookup_elem(&percpu_stats, &__func__name); \
    if (c != NULL) {                                            \
        *c += 1;                                                \
    }                                                           \
}

/*============================= INTERNAL STRUCTS ============================*/
typedef struct unwind_tables_key_t {
    int pid;
    int shard;
} unwind_tables_key_t;

typedef struct stack_unwind_row_t {
    __u64 pc;
    __u16 __reserved_do_not_use;
    __u8 cfa_type;
    __u8 rbp_type;
    __s16 cfa_offset;
    __s16 rbp_offset;
} stack_unwind_row_t;

typedef struct stack_unwind_table_t {
    __u64 low_pc;
    __u64 high_pc;
    __u64 table_len;
    __u64 __explicit_padding;
    stack_unwind_row_t rows[MAX_UNWIND_TABLE_SIZE];
} stack_unwind_table_t;


// We reached main.
__u32 UNWIND_SUCCESS = 1;
// Partial stack was retrieved.
__u32 UNWIND_TRUNCATED = 2;
// An (unhandled) dwarf expression was found.
__u32 UNWIND_UNSUPPORTED_EXPRESSION = 3;
// Any other error, such as failed memory reads.
// TODO(javierhonduco): split this error into subtypes.
__u32 UNWIND_CATCHALL_ERROR = 4;
// Errors that should never happen.
__u32 UNWIND_SHOULD_NEVER_HAPPEN_ERROR = 5;
// PC not in table (Kernel PC?).
__u32 UNWIND_PC_NOT_COVERED_ERROR = 6;
// Keep track of total samples.
__u32 UNWIND_SAMPLES_COUNT = 7;


/*================================ MAPS =====================================*/
BPF_HASH(debug_pids, int, u8, 32);
BPF_HASH(unwind_tables, unwind_tables_key_t, stack_unwind_table_t, MAX_PID_MAP_SIZE);

BPF_MAP(percpu_stats, BPF_MAP_TYPE_PERCPU_ARRAY, u32, u32, 10);

/*=========================== HELPER FUNCTIONS ==============================*/
DEFINE_COUNTER(UNWIND_SUCCESS);
DEFINE_COUNTER(UNWIND_TRUNCATED);
DEFINE_COUNTER(UNWIND_UNSUPPORTED_EXPRESSION);
DEFINE_COUNTER(UNWIND_SHOULD_NEVER_HAPPEN_ERROR);
DEFINE_COUNTER(UNWIND_CATCHALL_ERROR);
DEFINE_COUNTER(UNWIND_PC_NOT_COVERED_ERROR);


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

static __always_inline void show_row(stack_unwind_table_t *unwind_table, index) {
#if 0
    u64 pc = unwind_table->rows[index].pc;
    u16 cfa_type = unwind_table->rows[index].cfa_type;
    s16 cfa_offset = unwind_table->rows[index].cfa_offset;
    s16 rbp_offset = unwind_table->rows[index].rbp_offset;

    bpf_printk("~ %d entry. Loc: %llx, CFA reg: %d Offset: %d, $rbp %d", index,
               pc, cfa_type, cfa_offset, rbp_offset);
#endif
}

static void bump_samples() {
    u32 *c = bpf_map_lookup_elem(&percpu_stats, &UNWIND_SAMPLES_COUNT);
    if (c != NULL) {
        *c += 1;
        if (*c % 50 == 0) {
            unwind_print_stats();
        }
    }
}

static __always_inline int walk_user_stacktrace(struct bpf_perf_event_data *ctx) {
    bmp_samples();
}

SEC("perf_event")
int profile_cpu(struct bpf_perf_event_data *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    int user_pid = pid_tgid;
    int user_tgid = pid_tgid >> 32;
    
    if (user_pid == 0) {
        return 0;
    }

    if (config_verbose) {
        bpf_printk("debug mode enabled, make sure you specified process name");
        if (!is_debug_enabled_for_pid(user_tgid)) {
            return 0;
        }
    }

    bool has_unwind_info = has_unwind_information(user_pid);
    if (!has_unwind_info) {
        #if 0
        add_stacks(ctx, pid_tgid, STACK_WALKING_METHOD_FP, NULL);
        #endif
    } else {
        #if 0
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
        #endif
    }

    return 0;
}

