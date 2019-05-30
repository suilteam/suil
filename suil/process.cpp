//
// Created by dc on 10/10/18.
//
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "suil/utils.h"
#include "suil/process.h"


namespace suil {

    static std::map<pid_t, Process*> PID_Process{};
    static bool PROC_Exiting{false};
    size_t ProcessBuffer::MAXIMUM_BUFFER_SIZE{0};

    extern void Worker_sa_handler(int sig, siginfo_t *info, void *context);

    void Process_sa_handler(int sig, siginfo_t *info, void *context) {
        strace("signal=%d", sig);
        if (PROC_Exiting)
            return;

        switch (sig) {
            case SIGCHLD:
                suil::Process::on_SIGCHLD(sig, info, context);
                break;
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
                PROC_Exiting = true;
                suil::Process::on_TERMINATION(sig, info, context);
                exit(EXIT_SUCCESS);
                /* UNREACHABLE */
                break;
            default:
                break;
        }

    }

    static void updatenv(const Map<String>& env)
    {
        if (env.empty())
            return;

        for (auto ev : env) {
            // set environment variables
            if (!ev.first.empty())
                setenv(ev.first(), ev.second(), true);
        }
    }

    bool openpipes(int in[2], int out[2], int err[2]) {
        if (pipe(in)) {
            // failed to open IO pipe
            serror("failed to pipe(input): %s", errno_s);
            return false;
        }

        if (pipe(out)) {
            // failed to open IO pipe
            serror("failed to pipe(output): %s", errno_s);
            utils::closepipe(in);
            return false;
        }

        if (pipe(err)) {
            // failed to open IO pipe
            serror("failed to pipe(error): %s", errno_s);
            utils::closepipe(in);
            utils::closepipe(out);
            return false;
        }

        return true;
    }

    inline void closepipes(int in[2], int out[2], int err[2]) {
        utils::closepipe(in);
        utils::closepipe(out);
        utils::closepipe(err);
    }

    void Process::on_SIGCHLD(int sig, siginfo_t *info, void *context)
    {
        pid_t pid = info? info->si_pid : -1;
        uid_t uid = info? info->si_uid : -1;
        strace("Process::on_SIGCHLD pid=%ld, uid=%ld", pid, uid);

        if (pid == -1) {
            // invalid SIGCHLD received
            serror("received an invalid SIGCHLD with null context {pid=%ld,uid=%ld}", pid, uid);
            return;
        }

        auto it = PID_Process.find(pid);
        if (it != PID_Process.end()) {
            if ((it->second != nullptr) && it->second->waitingExit)
                if (write(it->second->notifChan[1], &sig, sizeof(sig)) == -1)
                    lerror(it->second, "error writing to process notif channel: %s", errno_s);
        } else {
            // pid not registered
            strace("process with pid=%ld not registered or not waiting for ", pid);
            Worker_sa_handler(sig, info, context);
        }
    }

    void Process::on_TERMINATION(int sig, siginfo_t *info, void *context)
    {
        swarn("received {signal=%d} to terminate all child process", sig);
        for (auto& proc : PID_Process) {
            // terminate all child process
            if (proc.second) {
                proc.second->terminate();
            }
        }
        // erase all entries
        PID_Process.clear();
    }

    Process::Ptr Process::start(const Map<String>& env, const char * cmd, int argc, char *argv[])
    {
        int out[2], err[2], in[2];
        if (!openpipes(in, out, err))
            return nullptr;

        auto proc = Process::mkshared();
        if (pipe(proc->notifChan) == -1) {
            // forking process failed
            serror("error opening notification pipe: %s", errno_s);
            closepipes(in, out, err);
            return nullptr;
        }
        // the read end of the pipe should be non-blocking
        utils::setnonblocking(proc->notifChan[0]);

        if ((proc->pid = mfork()) == -1) {
            // forking process failed
            serror("forking failed: %s", errno_s);
            closepipes(in, out, err);
            return nullptr;
        }

        if (proc->pid == 0) {
            // child process IO is mapped back to process using pipes
            dup2(in[1],  STDIN_FILENO);
            dup2(out[1], STDOUT_FILENO);
            dup2(err[1], STDERR_FILENO);
            close(in[0]); close(out[0]); close(err[0]);
            // update environment variables
            updatenv(env);

            int ret = ::execvp(cmd, argv);

            // spawning process with execvp failed
            serror("launching '%s' failed: %s", errno_s);
            _exit(ret);
        }
        else {
            // close the unused ends of the pipe
            close(in[1]); close(out[1]); close(err[1]);
            proc->stdIn  = in[0];
            utils::setnonblocking(proc->stdIn);
            proc->stdOut = out[0];
            utils::setnonblocking(proc->stdOut);
            proc->stdErr = err[0];
            utils::setnonblocking(proc->stdErr);
            PID_Process[proc->pid] = proc.get();
            strace("launched process pid=%ld", proc->pid);
            // start reading asynchronously
            proc->startReadAsync();
        }

        return proc;
    }

    bool Process::isExited()
    {
        if (Ego.pid == -1)
            return true;
        int status{0};
        int ret = waitpid(Ego.pid, &status, WNOHANG);
        if (ret < 0) {
            if (errno != ECHILD)
                serror("process{pid=%ld} waitpid failed: %s", Ego.pid, errno_s);
            Ego.pid = -1;
            return true;
        }
        if ((ret == Ego.pid) && WIFEXITED(status)) {
            // process exited
            trace("process{pid=%ld} exited status=%d", Ego.pid, WEXITSTATUS(status));
            Ego.pid = -1;
            return true;
        }

        return false;
    }

    void Process::waitExit(int timeout)
    {
        if (Ego.isExited())
            return;

        Ego.waitingExit = true;
        do {
            int ev = fdwait(Ego.notifChan[0], FDW_IN | FDW_ERR, utils::after(timeout));
            if (ev & FDW_IN) {
                // notification received
                int sig{0};
                if (read(Ego.notifChan[1], &sig, sizeof(sig)) == sizeof(sig)) {
                    if (sig != SIGCHLD) {
                        // shouldn't have received this signal
                        iwarn("received signal=%d instead of SIGCHLD", sig);
                        continue;
                    }
                    break;
                }
            }
            trace("error while waiting for process to exit: %s", errno_s);
            break;

        } while (!Ego.isExited());

        Ego.waitingExit = false;
    }

    void Process::terminate()
    {
        trace("terminate process{pid=%ld} requested", Ego.pid);
        if (!Ego.isExited()) {
            pid_t  tmp{Ego.pid};
            Ego.stopIO();
            kill(tmp, SIGKILL);

        } else {
            // attempting to terminate an already exited process
            trace("attempting to terminate an already exited process");
            Ego.stopIO();
        }
        fdclear(Ego.notifChan[0]);
        utils::closepipe(Ego.notifChan);
    }

    void Process::stopIO()
    {
        if (Ego.stdOut >= 0) {
            close(Ego.stdOut);
            Ego.stdOut = -1;
            yield();
        }

        if (Ego.stdIn >= 0) {
            close(Ego.stdIn);
            Ego.stdIn = -1;
        }

        if (Ego.stdErr >= 0) {
            close(Ego.stdErr);
            Ego.stdErr = -1;
            yield();
        }
    }

    void Process::processAsyncRead(Process& proc, int fd, bool err)
    {
        proc.pendingReads++;
        char buffer[1024];
        do {
            int ev = fdwait(fd, FDW_IN | FDW_ERR, -1);
            ltrace(&proc, "read event{fd=%d} %d",fd, ev);
            if (ev == FDW_IN) {
                /* data available to read from fd */
                ssize_t nread = read(fd, buffer, sizeof(buffer)-1);
                if (nread == -1) {
                    if (errno == EWOULDBLOCK)
                        continue;
                    // reading failed
                    break;
                }
                if (nread == 0)
                    continue;

                // something has been read
                buffer[nread] = '\0';
                String tmp{buffer, (size_t)nread, false};
                if (err) {
                    if (proc.asyncStderr) {
                        proc.asyncStderr(tmp.dup());
                    }
                    else {
                        proc.buffer.writeError(tmp.dup());
                    }
                }
                else {
                    if (proc.asyncStdout) {
                        proc.asyncStdout(tmp.dup());
                    }
                    else {
                        proc.buffer.writeOutput(tmp.dup());
                    }
                }
            } else {
                // received an error
                ltrace(&proc, "error while waiting for {fd=%d} to be readable: %s",fd, errno_s);
                break;
            }
        } while (!proc.isExited());

        proc.pendingReads--;
    }

    void Process::flushBuffers(Process& p, Reader&& rd, bool err) {
        if (err) {
          while (p.buffer.hasStderr())
            rd(p.buffer.readError());

          p.asyncStderr = std::move(rd);
        }
        else {
          while (p.buffer.hasStdout())
            rd(p.buffer.readOutput());

          p.asyncStdout = std::move(rd);
        }
      }

    void Process::startReadAsync()
    {
        go(processAsyncRead(Ego, Ego.stdErr, true));
        go(processAsyncRead(Ego, Ego.stdOut, false));
    }
}
