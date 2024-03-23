#include "process_internal.h"

/* Delete.
 */
 
static void process_cleanup(struct process *process) {
  if (process->fd>=0) {
    close(process->fd);
    process->fd=-1;
  }
  if (process->pid) {
    kill(process->pid,SIGTERM);
    int wstatus=0;
    waitpid(process->pid,&wstatus,0);
    process->pid=0;
  }
}
 
void process_context_del(struct process_context *ctx) {
  if (!ctx) return;
  if (ctx->processv) {
    while (ctx->processc-->0) process_cleanup(ctx->processv+ctx->processc);
    free(ctx->processv);
  }
  if (ctx->pollfdv) free(ctx->pollfdv);
  free(ctx);
}

/* New.
 */

struct process_context *process_context_new(const struct process_delegate *delegate) {
  struct process_context *ctx=calloc(1,sizeof(struct process_context));
  if (!ctx) return 0;
  if (delegate) ctx->delegate=*delegate;
  return ctx;
}

/* Update, convenience.
 */

int process_update(struct process_context *ctx,int toms) {
  int pollfdc=0;
  for (;;) {
    if ((pollfdc=process_get_files(ctx->pollfdv,ctx->pollfda,ctx))<=ctx->pollfda) break;
    int na=(pollfdc+16)&~15;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ctx->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ctx->pollfdv=nv;
    ctx->pollfda=na;
  }
  if (pollfdc<=0) {
    if (toms) usleep(toms*1000);
    return pollfdc;
  }
  int err=poll(ctx->pollfdv,pollfdc,toms);
  if (err>0) {
    struct pollfd *pollfd=ctx->pollfdv;
    int i=pollfdc;
    for (;i-->0;pollfd++) {
      if (pollfd->revents) {
        if (process_update_file(ctx,pollfd->fd)<0) return -1;
      }
    }
  }
  return 0;
}

/* List pollable files and perform any routine maintenance.
 */
 
int process_get_files(struct pollfd *dst,int dsta,struct process_context *ctx) {
  if (ctx->busy) return -1;
  ctx->busy=1;
  int dstc=0;
  int i=ctx->processc;
  struct process *process=ctx->processv+i-1;
  for (;i-->0;process--) {
    if ((process->fd<0)||!process->pid) {
      process_cleanup(process);
      ctx->processc--;
      memmove(process,process+1,sizeof(struct process)*(ctx->processc-i));
      continue;
    }
    if (dstc<dsta) {
      struct pollfd *pollfd=dst+dstc;
      memset(pollfd,0,sizeof(struct pollfd));
      pollfd->fd=process->fd;
      pollfd->events=POLLIN|POLLERR|POLLHUP;
    }
    dstc++;
  }
  ctx->busy=0;
  return dstc;
}

/* Update one polled file.
 */
 
int process_update_file(struct process_context *ctx,int fd) {
  if (ctx->busy) return -1;
  int i=ctx->processc;
  struct process *process=ctx->processv+i-1;
  ctx->busy=1;
  for (;i-->0;process--) {
    if (process->fd!=fd) continue;
    
    char buf[1024];
    int bufc=read(process->fd,buf,sizeof(buf));
    if (bufc>0) {
      if (ctx->delegate.cb_output) ctx->delegate.cb_output(ctx->delegate.userdata,process->pid,process->fd,buf,bufc);
    } else {
      int wstatus=0;
      waitpid(process->pid,&wstatus,0);
      if (WIFEXITED(wstatus)) process->status=WEXITSTATUS(wstatus);
      else process->status=-1;
      if (ctx->delegate.cb_term) ctx->delegate.cb_term(ctx->delegate.userdata,process->pid,process->fd,process->status);
      process->pid=0;
      process_cleanup(process);
      ctx->processc--;
      memmove(process,process+1,sizeof(struct process)*(ctx->processc-i));
    }
    ctx->busy=0;
    return 0;
  }
  ctx->busy=0;
  return -1;
}

/* Spawn process.
 */
 
int process_spawn_(struct process_context *ctx,...) {

  if (ctx->busy) return -1;
  if (ctx->processc>=ctx->processa) {
    int na=ctx->processa+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->processv,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->processv=nv;
    ctx->processa=na;
  }

  // Copy argv from variadics. No adjustments.
  va_list vargs;
  va_start(vargs,ctx);
  int arga=16,argc=0;
  char **argv=malloc(sizeof(void*)*arga);
  if (!argv) return -1;
  while (1) {
    const char *arg=va_arg(vargs,const char *);
    if (!arg||!arg[0]) break;
    if (argc>=arga-1) {
      int na=arga+16;
      if (na>INT_MAX/sizeof(void*)) {
        while (argc-->0) free(argv[argc]);
        free(argv);
        return -1;
      }
      void *nv=realloc(argv,sizeof(void*)*na);
      if (!nv) {
        while (argc-->0) free(argv[argc]);
        free(argv);
        return -1;
      }
      argv=nv;
      arga=na;
    }
    if (!(argv[argc]=strdup(arg))) {
      while (argc-->0) free(argv[argc]);
      free(argv);
      return -1;
    }
    argc++;
  }
  if (!argc) {
    free(argv);
    return -1;
  }
  argv[argc]=0;
  
  // Generate a pipe to use as child's stdin+stdout+stderr.
  int fdv[2]={-1,-1};
  if (socketpair(AF_UNIX,SOCK_STREAM,PF_UNIX,fdv)<0) {
    while (argc-->0) free(argv[argc]);
    free(argv);
    return -1;
  }
  
  // Fork the new process.
  int pid=fork();
  if (pid<0) {
    while (argc-->0) free(argv[argc]);
    free(argv);
    close(fdv[0]);
    close(fdv[1]);
    return -1;
  }
  
  // If we're the parent, record it and we're done.
  if (pid) {
    struct process *process=ctx->processv+ctx->processc++;
    memset(process,0,sizeof(struct process));
    process->pid=pid;
    process->fd=fdv[0];
    close(fdv[1]);
    return fdv[0];
  }
  
  // Redirect streams.
  close(fdv[0]);
  dup2(fdv[1],STDIN_FILENO);
  dup2(fdv[1],STDOUT_FILENO);
  dup2(fdv[1],STDERR_FILENO);
  
  // Execute.
  execvp(argv[0],argv);
  exit(1);
  return -1;
}
