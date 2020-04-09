//
// Created by Carter Mbotho on 2020-04-08.
//

#include "../common.h"
#include "../client.h"

namespace suil::sawsdk::Client {

    Syslog Logger::sSysLog{};

    Logger::Logger()
    {
        log::setup(opt(sink, [this](const char *msg, size_t size, log::Level l) {
            Ego.log(msg, size, l);
        }));
        log::setup(opt(format, [this](char *out, log::Level l, const char *tag, const char *fmt, va_list args){
            return Ego.format(out, l, tag, fmt, args);
        }));
    }

    Logger::~Logger() {
        log::setup(opt(sink, [](const char *msg, size_t size, log::Level l) {
            log::Handler()(msg, size, l);
        }));
        log::setup(opt(format, [](char *out, log::Level l, const char *tag, const char *fmt, va_list args){
            return log::Formatter()(out, l, tag, fmt, args);
        }));
    }

    void Logger::log(const char *msg, size_t size, suil::log::Level l)
    {
        switch (l) {
            case log::Level::INFO:
            case log::Level::NOTICE:
                printf("%*s", (int)size, msg);
                break;
            case log::Level::ERROR:
            case log::Level::CRITICAL:
            case log::Level::WARNING:
                fprintf(stderr, "error: %*s", (int)size, msg);
                break;
            case log::Level::DEBUG:
            case log::Level::TRACE:
                sSysLog.log(msg, size, l);
                break;
            default:
                break;
        }
    }

    size_t Logger::format(char *out, suil::log::Level l, const char *tag, const char *fmt, va_list args)
    {
        switch (l) {
            case log::DEBUG:
            case log::TRACE:
                return log::Formatter()(out, l, tag, fmt, args);
            default: {
                int wr = vsnprintf(out, SUIL_LOG_BUFFER_SIZE, fmt, args);
                out[wr++] = '\n';
                out[wr]   = '\0';
                return (size_t) wr;
            }
        }
    }
}