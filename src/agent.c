// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2022 Facebook */
#include <argp.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <linux/perf_event.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "agent.skel.h"
#include "agent.h"

static struct config_t {
    bool verbose;
    bool sw_event;
} config = {
    .verbose = false,
    .sw_event = false,
};

const char *argp_program_version = "agent 0.1";
const char *argp_program_bug_address = "https://github.com/harry-gcb/x-agent";
const char argp_program_doc[] =
"Summarize off-CPU time by stack trace.\n"
"\n"
"USAGE: agent [--help] [-p PID | -u | -k] [-m MIN-BLOCK-TIME] "
"[-M MAX-BLOCK-TIME] [--state] [--perf-max-stack-depth] [--stack-storage-size] "
"[duration]\n"
"EXAMPLES:\n"
"    agent             # trace off-CPU stack time until Ctrl-C\n"
"    agent 5           # trace for 5 seconds only\n"
"    agent -m 1000     # trace only events that last more than 1000 usec\n"
"    agent -M 10000    # trace only events that last less than 10000 usec\n"
"    agent -p 185,175,165 # only trace threads for PID 185,175,165\n"
"    agent -t 188,120,134 # only trace threads 188,120,134\n"
"    agent -u          # only trace user threads (no kernel)\n"
"    agent -k          # only trace kernel threads (no user)\n";

static const struct argp_option opts[] = {
    { "pid", 'p', "PID", 0, "Trace these PIDs only, comma-separated list", 0 },
    { "tid", 't', "TID", 0, "Trace these TIDs only, comma-separated list", 0 },
    { "verbose", 'v', NULL, 0, "Verbose debug output", 0 },
    { "sw-event", 's', NULL, 0, "use software event for triggering stack trace capture", 0},
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
        config.verbose = true;
        break;
    case 's':
        config.sw_event = true;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG && !config.verbose)
        return 0;
    return vfprintf(stderr, format, args);
}
/*
 * This function is from libbpf, but it is not a public API and can only be
 * used for demonstration. We can use this here because we statically link
 * against the libbpf built from submodule during build.
 */
extern int parse_cpu_mask_file(const char *fcpu, bool **mask, int *mask_sz);

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd,
                unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

int main(int argc, char *argv[])
{
    int i = 0;
    int err = 0;
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };

    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err) {
        return err;
    }

    struct agent_bpf *obj = NULL;;

    const char *online_cpus_file = "/sys/devices/system/cpu/online";
    int freq = 1, pid = -1, cpu;

    struct perf_event_attr attr;
    struct bpf_link **links = NULL;

    int num_cpus, num_online_cpus;
    int *pefds = NULL, pefd;

    bool *online_mask = NULL;

    err = parse_cpu_mask_file(online_cpus_file, &online_mask, &num_online_cpus);
    if (err) {
        fprintf(stderr, "Fail to get online CPU numbers: %d\n", err);
        goto cleanup;
    }

    num_cpus = libbpf_num_possible_cpus();
    if (num_cpus <= 0) {
        fprintf(stderr, "Fail to get the number of processors\n");
        err = -1;
        goto cleanup;
    }

    libbpf_set_print(libbpf_print_fn);

    obj = agent_bpf__open();
    if (!obj) {
        fprintf(stderr, "failed to open BPF object\n");
        return 1;
    }
    obj->rodata->config_verbose = config.verbose;
    /* initialize global data (filtering options) */
    // obj->rodata->user_threads_only = env.user_threads_only;
    // obj->rodata->kernel_threads_only = env.kernel_threads_only;
    // obj->rodata->state = env.state;
    // obj->rodata->min_block_ns = env.min_block_time;
    // obj->rodata->max_block_ns = env.max_block_time;


    err = agent_bpf__load(obj);
    if (err) {
        fprintf(stderr, "failed to load BPF programs\n");
        goto cleanup;
    }

    pefds = malloc(num_cpus * sizeof(int));
    for (i = 0; i < num_cpus; i++) {
        pefds[i] = -1;
    }

    links = calloc(num_cpus, sizeof(struct bpf_link *));

    memset(&attr, 0, sizeof(attr));
    attr.type = config.sw_event ? PERF_TYPE_SOFTWARE : PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = config.sw_event ? PERF_COUNT_SW_CPU_CLOCK : PERF_COUNT_HW_CPU_CYCLES;
    attr.sample_freq = freq;
    attr.freq = 1;

    for (cpu = 0; cpu < num_cpus; cpu++) {
        /* skip offline/not present CPUs */
        if (cpu >= num_online_cpus || !online_mask[cpu])
            continue;

        /* Set up performance monitoring on a CPU/Core */
        pefd = perf_event_open(&attr, pid, cpu, -1, PERF_FLAG_FD_CLOEXEC);
        if (pefd < 0) {
            if (!config.sw_event && errno == ENOENT) {
                fprintf(stderr,
                    "Fail to set up performance monitor on a CPU/Core.\n"
                    "Try running the profile example with the `--sw-event` option.\n");
            } else {
                fprintf(stderr, "Fail to set up performance monitor on a CPU/Core.\n");
            }
            err = -1;
            goto cleanup;
        }
        pefds[cpu] = pefd;

        /* Attach a BPF program on a CPU */
        links[cpu] = bpf_program__attach_perf_event(obj->progs.profile_cpu, pefd);
        if (!links[cpu]) {
            err = -1;
            goto cleanup;
        }
    }

    /* Wait and receive stack traces */
    while (1) {
        sleep(100);
    }

cleanup:
    if (links) {
        for (cpu = 0; cpu < num_cpus; cpu++)
            bpf_link__destroy(links[cpu]);
        free(links);
    }
    if (pefds) {
        for (i = 0; i < num_cpus; i++) {
            if (pefds[i] >= 0)
                close(pefds[i]);
        }
        free(pefds);
    }
    agent_bpf__destroy(obj);
    free(online_mask);
    return -err;
}
