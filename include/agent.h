#ifndef AGENT_H_
#define AGENT_H_

#include <vmlinux.h>

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

typedef struct stack_unwind_row_t {
    u64 pc;
    u16 __reserved_do_not_use;
    u8 cfa_type;
    u8 rbp_type;
    s16 cfa_offset;
    s16 rbp_offset;
} stack_unwind_row_t;

typedef struct stack_unwind_table_t {
    u64 low_pc;
    u64 high_pc;
    u64 table_len;
    u64 __explicit_padding;
    stack_unwind_row_t rows[MAX_UNWIND_TABLE_SIZE];
} stack_unwind_table_t;

#endif