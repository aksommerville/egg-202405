/* process.h
 * Spawn and manage child processes.
 *
 * We pass around a file descriptor for the process's stdout+stderr+stdin, instead of a pid.
 * That's a convenience so you can mix it with other polly APIs.
 * But it requires the children to treat stdout+stderr+stdin sanely.
 * If they close one, we think the process is terminated.
 */
 
#ifndef PROCESS_H
#define PROCESS_H

struct pollfd;
struct process_context;

struct process_delegate {
  void *userdata;
  void (*cb_term)(void *userdata,int pid,int fd,int status);
  void (*cb_output)(void *userdata,int pid,int fd,const void *v,int c);
};

void process_context_del(struct process_context *ctx);

struct process_context *process_context_new(const struct process_delegate *delegate);

int process_update(struct process_context *ctx,int toms);
int process_get_files(struct pollfd *dst,int dsta,struct process_context *ctx);
int process_update_file(struct process_context *ctx,int fd);

/* Returns fd on success, or -1 if failed to launch.
 * Must contain at least one string arg.
 * Args terminate on the first null or empty string.
 */
int process_spawn_(struct process_context *ctx,...);
#define process_spawn(ctx,...) process_spawn_(ctx,##__VA_ARGS__,(void*)0)

#endif
