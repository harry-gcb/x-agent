// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// Copyright (c) 2021 Wenbo Zhang
//
// Based on offcputime(8) from BCC by Brendan Gregg.
// 19-Mar-2021   Wenbo Zhang   Created this.
#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "offcputime.h"
#include "offcputime.skel.h"
#include "trace_helpers.h"

static struct env {
    pid_t pids[MAX_PID_NR];
    pid_t tids[MAX_TID_NR];
    bool user_threads_only;
    bool kernel_threads_only;
    int stack_storage_size;
    int perf_max_stack_depth;
    __u64 min_block_time;
    __u64 max_block_time;
    long state;
    int duration;
    bool verbose;
} env = {
    .stack_storage_size = 1024,
    .perf_max_stack_depth = 127,
    .min_block_time = 1,
    .max_block_time = -1,
    .state = -1,
    .duration = 99999999,
};

const char *argp_program_version = "offcputime 0.1";
const char *argp_program_bug_address =
    "https://github.com/iovisor/bcc/tree/master/libbpf-tools";
const char argp_program_doc[] =
"Summarize off-CPU time by stack trace.\n"
"\n"
"USAGE: offcputime [--help] [-p PID | -u | -k] [-m MIN-BLOCK-TIME] "
"[-M MAX-BLOCK-TIME] [--state] [--perf-max-stack-depth] [--stack-storage-size] "
"[duration]\n"
"EXAMPLES:\n"
"    offcputime             # trace off-CPU stack time until Ctrl-C\n"
"    offcputime 5           # trace for 5 seconds only\n"
"    offcputime -m 1000     # trace only events that last more than 1000 usec\n"
"    offcputime -M 10000    # trace only events that last less than 10000 usec\n"
"    offcputime -p 185,175,165 # only trace threads for PID 185,175,165\n"
"    offcputime -t 188,120,134 # only trace threads 188,120,134\n"
"    offcputime -u          # only trace user threads (no kernel)\n"
"    offcputime -k          # only trace kernel threads (no user)\n";

#define OPT_PERF_MAX_STACK_DEPTH    1 /* --pef-max-stack-depth */
#define OPT_STACK_STORAGE_SIZE        2 /* --stack-storage-size */
#define OPT_STATE            3 /* --state */

static const struct argp_option opts[] = {
    { "pid", 'p', "PID", 0, "Trace these PIDs only, comma-separated list", 0 },
    { "tid", 't', "TID", 0, "Trace these TIDs only, comma-separated list", 0 },
    { "user-threads-only", 'u', NULL, 0,
      "User threads only (no kernel threads)", 0 },
    { "kernel-threads-only", 'k', NULL, 0,
      "Kernel threads only (no user threads)", 0 },
    { "perf-max-stack-depth", OPT_PERF_MAX_STACK_DEPTH,
      "PERF-MAX-STACK-DEPTH", 0, "the limit for both kernel and user stack traces (default 127)", 0 },
    { "stack-storage-size", OPT_STACK_STORAGE_SIZE, "STACK-STORAGE-SIZE", 0,
      "the number of unique stack traces that can be stored and displayed (default 1024)", 0 },
    { "min-block-time", 'm', "MIN-BLOCK-TIME", 0,
      "the amount of time in microseconds over which we store traces (default 1)", 0 },
    { "max-block-time", 'M', "MAX-BLOCK-TIME", 0,
      "the amount of time in microseconds under which we store traces (default U64_MAX)", 0 },
    { "state", OPT_STATE, "STATE", 0, "filter on this thread state bitmask (eg, 2 == TASK_UNINTERRUPTIBLE) see include/linux/sched.h", 0 },
    { "verbose", 'v', NULL, 0, "Verbose debug output", 0 },
    { NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help", 0 },
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    static int pos_args;
    int ret;

    switch (key) {
    case 'h':
        argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
        break;
    case 'v':
        env.verbose = true;
        break;
    case 'p':
        ret = split_convert(strdup(arg), ",", env.pids, sizeof(env.pids),
                    sizeof(pid_t), str_to_int);
        if (ret) {
            if (ret == -ENOBUFS)
                fprintf(stderr, "the number of pid is too big, please "
                    "increase MAX_PID_NR's value and recompile\n");
            else
                fprintf(stderr, "invalid PID: %s\n", arg);

            argp_usage(state);
        }
        break;
    case 't':
        ret = split_convert(strdup(arg), ",", env.tids, sizeof(env.tids),
                    sizeof(pid_t), str_to_int);
        if (ret) {
            if (ret == -ENOBUFS)
                fprintf(stderr, "the number of tid is too big, please "
                    "increase MAX_TID_NR's value and recompile\n");
            else
                fprintf(stderr, "invalid TID: %s\n", arg);

            argp_usage(state);
        }
        break;
    case 'u':
        env.user_threads_only = true;
        break;
    case 'k':
        env.kernel_threads_only = true;
        break;
    case OPT_PERF_MAX_STACK_DEPTH:
        errno = 0;
        env.perf_max_stack_depth = strtol(arg, NULL, 10);
        if (errno) {
            fprintf(stderr, "invalid perf max stack depth: %s\n", arg);
            argp_usage(state);
        }
        break;
    case OPT_STACK_STORAGE_SIZE:
        errno = 0;
        env.stack_storage_size = strtol(arg, NULL, 10);
        if (errno) {
            fprintf(stderr, "invalid stack storage size: %s\n", arg);
            argp_usage(state);
        }
        break;
    case 'm':
        errno = 0;
        env.min_block_time = strtoll(arg, NULL, 10);
        if (errno) {
            fprintf(stderr, "Invalid min block time (in us): %s\n", arg);
            argp_usage(state);
        }
        break;
    case 'M':
        errno = 0;
        env.max_block_time = strtoll(arg, NULL, 10);
        if (errno) {
            fprintf(stderr, "Invalid min block time (in us): %s\n", arg);
            argp_usage(state);
        }
        break;
    case OPT_STATE:
        errno = 0;
        env.state = strtol(arg, NULL, 10);
        if (errno || env.state < 0 || env.state > 2) {
            fprintf(stderr, "Invalid task state: %s\n", arg);
            argp_usage(state);
        }
        break;
    case ARGP_KEY_ARG:
        if (pos_args++) {
            fprintf(stderr,
                "Unrecognized positional argument: %s\n", arg);
            argp_usage(state);
        }
        errno = 0;
        env.duration = strtol(arg, NULL, 10);
        if (errno || env.duration <= 0) {
            fprintf(stderr, "Invalid duration (in s): %s\n", arg);
            argp_usage(state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG && !env.verbose)
        return 0;
    return vfprintf(stderr, format, args);
}

static void sig_handler(int sig)
{
}

static void print_map(struct ksyms *ksyms, struct syms_cache *syms_cache,
              struct offcputime_bpf *obj)
{
    struct key_t lookup_key = {}, next_key;
    const struct ksym *ksym;
    const struct syms *syms;
    const struct sym *sym;
    int err, i, ifd, sfd;
    unsigned long *ip;
    struct val_t val;
    struct sym_info sinfo;
    int idx;

    ip = calloc(env.perf_max_stack_depth, sizeof(*ip));
    if (!ip) {
        fprintf(stderr, "failed to alloc ip\n");
        return;
    }

    ifd = bpf_map__fd(obj->maps.info);
    sfd = bpf_map__fd(obj->maps.stackmap);
    while (!bpf_map_get_next_key(ifd, &lookup_key, &next_key)) {
        idx = 0;

        err = bpf_map_lookup_elem(ifd, &next_key, &val);
        if (err < 0) {
            fprintf(stderr, "failed to lookup info: %d\n", err);
            goto cleanup;
        }
        lookup_key = next_key;
        if (val.delta == 0)
            continue;
        if (bpf_map_lookup_elem(sfd, &next_key.kern_stack_id, ip) != 0) {
            fprintf(stderr, "    [Missed Kernel Stack]\n");
            goto print_ustack;
        }

        for (i = 0; i < env.perf_max_stack_depth && ip[i]; i++) {
            ksym = ksyms__map_addr(ksyms, ip[i]);
            if (!env.verbose) {
                printf("    %s\n", ksym ? ksym->name : "unknown");
            } else {
                if (ksym)
                    printf("    #%-2d 0x%lx %s+0x%lx\n", idx++, ip[i], ksym->name, ip[i] - ksym->addr);
                else
                    printf("    #%-2d 0x%lx [unknown]\n", idx++, ip[i]);
            }
        }

print_ustack:
        if (next_key.user_stack_id == -1)
            goto skip_ustack;

        if (bpf_map_lookup_elem(sfd, &next_key.user_stack_id, ip) != 0) {
            fprintf(stderr, "    [Missed User Stack]\n");
            goto skip_ustack;
        }

        syms = syms_cache__get_syms(syms_cache, next_key.tgid);
        if (!syms) {
            if (!env.verbose) {
                fprintf(stderr, "failed to get syms\n");
            } else {
                for (i = 0; i < env.perf_max_stack_depth && ip[i]; i++)
                    printf("    #%-2d 0x%016lx [unknown]\n", idx++, ip[i]);
            }
            goto skip_ustack;
        }
        for (i = 0; i < env.perf_max_stack_depth && ip[i]; i++) {
            if (!env.verbose) {
                sym = syms__map_addr(syms, ip[i]);
                if (sym)
                    printf("    %s\n", sym->name);
                else
                    printf("    [unknown]\n");
            } else {
                printf("    #%-2d 0x%016lx", idx++, ip[i]);
                err = syms__map_addr_dso(syms, ip[i], &sinfo);
                if (err == 0) {
                    if (sinfo.sym_name)
                        printf(" %s+0x%lx", sinfo.sym_name, sinfo.sym_offset);
                    printf(" (%s+0x%lx)", sinfo.dso_name, sinfo.dso_offset);
                }
                printf("\n");
            }
        }

skip_ustack:
        printf("    %-16s %s (%d)\n", "-", val.comm, next_key.pid);
        printf("        %lld\n\n", val.delta);
    }

cleanup:
    free(ip);
}

static bool print_header_threads()
{
    int i;
    bool printed = false;

    if (env.pids[0]) {
        printf(" PID [");
        for (i = 0; i < MAX_PID_NR && env.pids[i]; i++)
            printf("%d%s", env.pids[i], (i < MAX_PID_NR - 1 && env.pids[i + 1]) ? ", " : "]");
        printed = true;
    }

    if (env.tids[0]) {
        printf(" TID [");
        for (i = 0; i < MAX_TID_NR && env.tids[i]; i++)
            printf("%d%s", env.tids[i], (i < MAX_TID_NR - 1 && env.tids[i + 1]) ? ", " : "]");
        printed = true;
    }

    return printed;
}

static void print_headers()
{
    printf("Tracing off-CPU time (us) of");

    if (!print_header_threads())
        printf(" all threads");

    if (env.duration < 99999999)
        printf(" for %d secs.\n", env.duration);
    else
        printf("... Hit Ctrl-C to end.\n");
}

int main(int argc, char **argv)
{
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };
    struct syms_cache *syms_cache = NULL;
    struct ksyms *ksyms = NULL;
    struct offcputime_bpf *obj;
    int pids_fd, tids_fd;
    int err, i;
    __u8 val = 0;

    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err)
        return err;
    if (env.user_threads_only && env.kernel_threads_only) {
        fprintf(stderr, "user_threads_only and kernel_threads_only cannot be used together.\n");
        return 1;
    }
    if (env.min_block_time >= env.max_block_time) {
        fprintf(stderr, "min_block_time should be smaller than max_block_time\n");
        return 1;
    }

    libbpf_set_print(libbpf_print_fn);

    obj = offcputime_bpf__open();
    if (!obj) {
        fprintf(stderr, "failed to open BPF object\n");
        return 1;
    }

    /* initialize global data (filtering options) */
    obj->rodata->user_threads_only = env.user_threads_only;
    obj->rodata->kernel_threads_only = env.kernel_threads_only;
    obj->rodata->state = env.state;
    obj->rodata->min_block_ns = env.min_block_time;
    obj->rodata->max_block_ns = env.max_block_time;

    /* User space PID and TID correspond to TGID and PID in the kernel, respectively */
    if (env.pids[0])
        obj->rodata->filter_by_tgid = true;
    if (env.tids[0])
        obj->rodata->filter_by_pid = true;

    bpf_map__set_value_size(obj->maps.stackmap,
                env.perf_max_stack_depth * sizeof(unsigned long));
    bpf_map__set_max_entries(obj->maps.stackmap, env.stack_storage_size);

    if (!probe_tp_btf("sched_switch"))
        bpf_program__set_autoload(obj->progs.sched_switch, false);
    else
        bpf_program__set_autoload(obj->progs.sched_switch_raw, false);

    err = offcputime_bpf__load(obj);
    if (err) {
        fprintf(stderr, "failed to load BPF programs\n");
        goto cleanup;
    }

    if (env.pids[0]) {
        /* User pids_fd points to the tgids map in the BPF program */
        pids_fd = bpf_map__fd(obj->maps.tgids);
        for (i = 0; i < MAX_PID_NR && env.pids[i]; i++) {
            if (bpf_map_update_elem(pids_fd, &(env.pids[i]), &val, BPF_ANY) != 0) {
                fprintf(stderr, "failed to init pids map: %s\n", strerror(errno));
                goto cleanup;
            }
        }
    }
    if (env.tids[0]) {
        /* User tids_fd points to the pids map in the BPF program */
        tids_fd = bpf_map__fd(obj->maps.pids);
        for (i = 0; i < MAX_TID_NR && env.tids[i]; i++) {
            if (bpf_map_update_elem(tids_fd, &(env.tids[i]), &val, BPF_ANY) != 0) {
                fprintf(stderr, "failed to init tids map: %s\n", strerror(errno));
                goto cleanup;
            }
        }
    }

    ksyms = ksyms__load();
    if (!ksyms) {
        fprintf(stderr, "failed to load kallsyms\n");
        goto cleanup;
    }
    syms_cache = syms_cache__new(0);
    if (!syms_cache) {
        fprintf(stderr, "failed to create syms_cache\n");
        goto cleanup;
    }
    err = offcputime_bpf__attach(obj);
    if (err) {
        fprintf(stderr, "failed to attach BPF programs\n");
        goto cleanup;
    }

    signal(SIGINT, sig_handler);

    print_headers();

    /*
     * We'll get sleep interrupted when someone presses Ctrl-C (which will
     * be "handled" with noop by sig_handler).
     */
    sleep(env.duration);

    print_map(ksyms, syms_cache, obj);

cleanup:
    offcputime_bpf__destroy(obj);
    syms_cache__free(syms_cache);
    ksyms__free(ksyms);
    return err != 0;
}