#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>

#define MAX_PROCESSES 256

static Process  processes[MAX_PROCESSES];
static int      proc_count = 0;
static int      current_proc = 0;

void scheduler_init(void) {
    memset(processes, 0, sizeof(processes));
    proc_count   = 0;
    current_proc = 0;
}

int scheduler_spawn(const char *name, uint8_t priority, TaskFn task_fn) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_ZOMBIE || processes[i].id == 0) {
            processes[i].id       = i + 1;
            processes[i].state    = PROC_READY;
            processes[i].priority = priority;
            processes[i].lane_id  = lane_spawn(task_fn, NULL, priority);
            processes[i].task_fn  = task_fn;
            if (name) {
                int len = 0;
                while (name[len] && len < 31) {
                    processes[i].name[len] = name[len];
                    len++;
                }
                processes[i].name[len] = '\0';
            }
            if (i >= proc_count) proc_count = i + 1;
            return i;
        }
    }
    return -1;
}

void scheduler_tick(void) {
    if (proc_count == 0) return;

    /* Round-robin: find next READY process */
    int start = current_proc;
    do {
        current_proc = (current_proc + 1) % proc_count;
        if (processes[current_proc].state == PROC_READY) {
            processes[current_proc].state = PROC_RUNNING;
            if (processes[current_proc].task_fn)
                processes[current_proc].task_fn(NULL);
            processes[current_proc].state = PROC_READY;
            break;
        }
    } while (current_proc != start);
}

void scheduler_kill(int process_id) {
    if (process_id < 0 || process_id >= MAX_PROCESSES) return;
    processes[process_id].state = PROC_ZOMBIE;
    if (processes[process_id].lane_id >= 0)
        lane_kill(processes[process_id].lane_id);
}

void scheduler_list(void) {
    printf("%-4s %-32s %-8s %-4s\n", "ID", "NAME", "STATE", "PRI");
    for (int i = 0; i < proc_count; i++) {
        if (processes[i].id == 0) continue;
        const char *state_str[] = {"READY", "RUN", "BLOCKED", "ZOMBIE"};
        printf("%-4d %-32s %-8s %-4d\n",
               processes[i].id,
               processes[i].name,
               state_str[processes[i].state],
               processes[i].priority);
    }
}

int scheduler_active_count(void) {
    int cnt = 0;
    for (int i = 0; i < proc_count; i++) {
        if (processes[i].state != PROC_ZOMBIE && processes[i].id != 0)
            cnt++;
    }
    return cnt;
}
