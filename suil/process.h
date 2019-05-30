//
// Created by dc on 10/10/18.
//

#ifndef SUIL_PROCESS_HPP
#define SUIL_PROCESS_HPP

#include <signal.h>

#include <suil/utils.h>
#include <suil/logging.h>

namespace suil {

    define_log_tag(PROCESS);

    using Reader = std::function<void(String&&)>;

    struct ProcessBuffer {
    private:
      friend struct Process;
      ProcessBuffer()
      {}

      inline void writeOutput(String&& s) {
        push(stdOutput, stdoutBytes, std::move(s));
      }

      inline void writeError(String&& s) {
        push(stdError, stderrBytes, std::move(s));
      }

      inline String readError() {
        return pop(stdError, stderrBytes);
      }

      inline String readOutput() {
        return pop(stdOutput, stdoutBytes);
      }

      inline bool hasStderr() const {
        return !stdError.empty();
      }

      inline bool hasStdout() const {
        return !stdOutput.empty();
      }

    private:
      void push(std::deque<String>& buf, size_t& sz, String&& s) {
        sz += s.size();
        while ((sz >= MAXIMUM_BUFFER_SIZE) && !buf.empty()) {
          sz -= buf.back().size();
          buf.pop_back();
        }
        buf.push_front(std::move(s));
      }

      String pop(std::deque<String>& buf, size_t& sz) {
          if (buf.empty()) {
            return "";
          }
          else {
            sz -= buf.back().size();
            String tmp = std::move(buf.back());
            buf.pop_back();
            return std::move(tmp);
          }
      }

    private:
      static size_t MAXIMUM_BUFFER_SIZE;
      size_t      stderrBytes;
      size_t      stdoutBytes;
      std::deque<String> stdError;
      std::deque<String> stdOutput;
    };

    struct Process : LOGGER(PROCESS) {

        using ReadCallback = std::function<bool(String&&)>;
        struct ReadCallbacks{
            ReadCallback onStdOutput{nullptr};
            ReadCallback onStdError{nullptr};
        };

        sptr(Process);

    public:
        template <typename... Args>
        static Ptr launch(const char *cmd, Args... args) {
            // execute with empty environment
            return Process::launch(Map<String>{}, cmd, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static Ptr launch(const  Map<String>& env, const char *cmd, Args... args) {
            // reserve buffer for process arguments
            size_t argc  = sizeof...(args);
            char *argv[argc+2];
            // pack parameters into a buffer
            argv[0] = (char *) cmd;
            int index{1};
            Process::pack(index, argv, std::forward<Args>(args)...);
            return Process::start(env, cmd, argc, argv);
        }

        template <typename ...Args>
        static Process::Ptr bash(const Map<String>& env, const char *cmd, Args... args) {
            OBuffer tmp{256};
            Process::strfmt(tmp, cmd, std::forward<Args>(args)..., "; sleep 0.1");
            String cmdStr(tmp);
            return Process::launch(env, "bash", "-c", cmdStr());
        }

        template <typename ...Args>
        static Process::Ptr bash(const Map<String>&& env, const char *cmd, Args... args) {
            OBuffer tmp{256};
            Process::strfmt(tmp, cmd, std::forward<Args>(args)..., "; sleep 0.1");
            String cmdStr(tmp);
            return Process::launch(env, "bash", "-c", cmdStr());
        }

        template <typename ...Args>
        inline static Process::Ptr bash(const char *cmd, Args... args) {
            OBuffer tmp{256};
            Process::strfmt(tmp, cmd, std::forward<Args>(args)...);
            String cmdStr(tmp);
            return Process::launch("bash", "-c", cmdStr());
        }

        bool isExited();

        void terminate();

        void waitExit(int timeout = -1);

        inline String getStdOutput() {
            return Ego.buffer.readOutput();
        }

        inline String getStdError() {
            return Ego.buffer.readError();
        }

        template <typename... Callbacks>
        inline void readAsync(Callbacks&&... callbacks) {
            // configure the callbacks
            ReadCallbacks cbs{nullptr, nullptr};
            utils::apply_config(cbs, std::forward<Callbacks>(callbacks)...);
            if (cbs.onStdError) {
                go(flushBuffers(Ego, std::move(cbs.onStdError), true));
            }
            if (cbs.onStdOutput) {
                go(flushBuffers(Ego, std::move(cbs.onStdOutput), false));
            }
        }

        ~Process() {
            terminate();
        }

    private:

        static void strfmt(OBuffer&) {}

        template <typename Arg>
        static void strfmt(OBuffer& out, Arg arg) { out << arg << " "; }

        template <typename Arg, typename... Args>
        static void strfmt(OBuffer& out, Arg arg, Args... args) {
            out << arg << " ";
            Process::strfmt(out, std::forward<Arg>(args)...);
        }

        static void pack(int& index, char* argv[]) { argv[index] = (char *) NULL; }

        template <typename Arg>
        static void pack(int& index, char* argv[], Arg arg) {
            argv[index++] = (char *) arg;
            argv[index++] = (char *) NULL;
        }

        template <typename Arg, typename... Args>
        static void pack(int& index, char* argv[], const Arg arg, Args... args) {
            argv[index++] = (char *)arg;
            pack(index, argv, std::forward<Args>(args)...);
        }

        void stopIO();

        void startReadAsync();

        static Ptr start(const Map<String>& env, const char* cmd, int argc, char* argv[]);

        friend void Process_sa_handler(int sig, siginfo_t *info, void *context);
        static void on_SIGCHLD(int sig, siginfo_t *info, void *context);
        static void on_TERMINATION(int sig, siginfo_t *info, void *context);

        static coroutine void processAsyncRead(Process& proc, int fd, bool err);

        static coroutine void flushBuffers(Process& p, Reader&& rd, bool err);


        pid_t           pid{0};
        int             stdOut{0};
        int             stdIn{0};
        int             stdErr{0};
        int             notifChan[2];
        bool            waitingExit{false};
        int             pendingReads{0};
        ProcessBuffer   buffer;
        Reader          asyncStdout;
        Reader          asyncStderr;
    };

}
#endif //SUIL_PROCESS_HPP
