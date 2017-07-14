//
// Created by dc on 01/06/17.
//

#include <openssl/md5.h>

#include "sys.hpp"
#include "log.hpp"
#include "config.hpp"

namespace suil {

    namespace version {
        const uint16_t  MAJOR  = SUIL_MAJOR_VERSION;
        const uint16_t  MINOR  = SUIL_MINOR_VERSION;
        const uint16_t  PATCH  = SUIL_PATCH_VERSION;
        const uint16_t  BUILD  = SUIL_BUILD_NUMBER;
        const char*     TAG    = SUIL_BUILD_TAG;
        const char*     STRING = SUIL_VERSION_STRING;
        const char*     SWNAME = SUIL_SOFTWARE_NAME;
    };

    datetime::datetime(time_t t)
        : t_(t)
    {
        gmtime_r(&t_, &tm_);
    }

    datetime::datetime()
        : datetime(time(nullptr))
    {}

    datetime::datetime(const char *fmt, const char *str)
    {
        const char *tmp = HTTP_FMT;
        if (fmt) {
            tmp = fmt;
        }
        strptime(str, tmp, &tm_);
    }

    datetime::datetime(const char *http_time)
        : datetime(HTTP_FMT, http_time)
    {}

    buffer_t::buffer_t(uint32_t init_size)
        : data_{nullptr},
          size_(init_size),
          offset_(0)
    {
        if (size_)
            grow(init_size);
    }

    buffer_t::buffer_t(buffer_t && other)
        : data_(other.data_),
          size_(other.size_),
          offset_(other.offset_)
    {
        other.size_ = 0;
        other.offset_ = 0;
        other.data_ = nullptr;
    }

    buffer_t& buffer_t::operator=(buffer_t &&other) {
        size_ = other.size_;
        data_ = other.data_;
        offset_ = other.offset_;
        other.data_ = nullptr;
        other.offset_ = other.size_ = 0;

        return *this;
    }

    buffer_t::~buffer_t() {
        if (data_ != nullptr) {
            memory::free(data_);
            data_ = nullptr;
            size_ = 0;
            offset_ = 0;
        }
    }

    void buffer_t::appendf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendv(fmt, args);
        va_end(args);
    }

    void buffer_t::appendv(const char *fmt, va_list args) {
        char	sb[2048];
        int ret;
        ret = vsnprintf(sb, sizeof(sb), fmt, args);
        if (ret == -1) {
            throw std::runtime_error(
                    "buffer_t::appendv(): error: " + std::string(errno_s));
        }
        append(sb, (uint32_t) ret);
    }

    void buffer_t::appendnf(uint32_t hint, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        appendnv(hint, fmt, args);
        va_end(args);
    }

    void buffer_t::appendnv(uint32_t hint, const char *fmt, va_list args) {
        if ((size_-offset_) < hint)
            grow(hint);

        int ret;
        assert(data_ && hint);
        ret = vsnprintf((char *)(data_+offset_), (size_-offset_), fmt, args);
        if (ret == -1 || (ret + offset_) > size_) {
            throw std::runtime_error(
                    "buffer_t::appendnv(): error: " + std::string(errno_s));
        }
        offset_ += ret;
    }

    void buffer_t::append(const void *data, uint32_t len) {
        if ((size_-offset_) < len) {
            grow(len);
        }
        if (len)
            assert(data_);
        memcpy((data_+offset_), data, len);
        offset_ += len;
    }

    void buffer_t::append(time_t t, const char *fmt) {
        // reserve 64 bytes
        reserve(64);
        const char *pfmt = fmt? fmt : datetime::HTTP_FMT;
        ssize_t sz = strftime((char*)(data_+offset_), 64, pfmt, localtime(&t));
        if (sz < 0) {
            strace("formatting time failed: %s", errno_s);
            return;
        }
        offset_ += sz;
    }

    void buffer_t::grow(uint32_t add) {
        // check if the current buffer_t fits
        data_ = (uint8_t *)memory::realloc(data_,(offset_+add+1));
        if (data_ == nullptr)
            throw std::runtime_error(
                    "buffer_t::grow(): error: " + std::string(errno_s));
        // change the size of the memory
        size_ = (uint32_t) memory::fits(data_, (offset_+add+1));
    }

    void buffer_t::reset(size_t size, bool keep) {
        offset_ = 0;
        if (!keep || (size_ < size)) {
            size = (size < 8) ? 8 : size;
            grow((uint32_t) size);
        }
    }

    void buffer_t::seek(off_t off) {
        off_t to = offset_ + off;
        if (to < 0 || to > size_) {
            offset_ = 0;
        }
        else {
            offset_ = (uint32_t) to;
        }
    }

    void buffer_t::bseek(off_t off) {
        if (off < size_ && off >= 0) {
            offset_ = (uint32_t) off;
        }
    }

    void buffer_t::reserve(size_t size) {
        size_t  remaining = size_ - offset_;
        if (size > remaining) {
            grow((uint32_t) (size-remaining));
        }
    }

    char* buffer_t::release() {
        if (data_) {
            char *raw = (char *) (*this);
            data_ = nullptr;
            clear();
            return raw;
        }
        return (char *)"";
    }

    void buffer_t::clear() {
        if (data_) {
            memory::free(data_);
            data_ = nullptr;
        }
        offset_ = 0;
        size_ = 0;
    }

    buffer_t::operator char*() {
        if (data_ && data_[offset_] != '\0') {
            data_[offset_] = '\0';
        }
        return data();
    }

    buffer_t::operator strview_t() {
        if (data_ && offset_) {
            return boost::string_view((const char *) data_, offset_);
        }
        return strview_t();
    }

    file_t::file_t(mfile fd)
        : fd(fd)
    {}

    file_t::file_t(const char *pname, int flags, mode_t mode)
        : file_t(mfopen(pname, flags, mode))
    {}

    void file_t::close() {
        if (fd != nullptr) {
            flush(500);
            mfclose(fd);
            fd = nullptr;
        }
    }

    void file_t::flush(int64_t timeout) {
        assert(fd);
        mfflush(fd, utils::after(timeout));
    }

    off_t file_t::seek(off_t off) {
        assert(fd);
        return mfseek(fd, off);
    }

    bool file_t::eof() {
        assert(fd);
        return mfeof(fd) != 0;
    }

    off_t file_t::tell() {
        assert(fd);
        return mftell(fd);
    }

    bool file_t::read(void *buf, size_t &len, int64_t timeout) {
        bool status{true};
        assert(fd);
        size_t ret = mfread(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: file_t read error: %s", fd, errno_s);
            status = false;
        }
        len = ret;
        return status;
    }


    size_t file_t::write(const void *buf, size_t len, int64_t timeout) {
        assert(fd);
        size_t ret = mfwrite(fd, buf, len, utils::after(timeout));
        if (errno) {
            strace("%p: file_t write error: %s", fd, errno_s);
        }
        return ret;
    }

    file_t::~file_t() {
        close();
    }

    zcstring base64::encode(const uint8_t *data, size_t sz) {
        static char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        char *out = (char *) memory::alloc((sz+2)/3*4);
        char *it = out;
        while (sz >= 3) {
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xF0) >> 4)];
            // |-|-|-|-|X|X|X|X|
            h = (uint8_t) (*data++ & 0x0F) << 2;
            // |-|-|-|-|X|X|X|X|_|X|X|-|-|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xC0) >> 6)];
            // |-|-|X|X|X|X|X|X|
            *it++ = b64table[(*data++ & 0x3F)];
            sz -= 3;
        }

        if (sz == 1) {
            // pad with ==
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            *it++ = b64table[h];
            *it++ = '=';
            *it++ = '=';
        } else if (sz == 2) {
            // pad with =
            // |X|X|X|X|X|X|-|-|
            *it++ = b64table[((*data & 0xFC) >> 2)];
            // |-|-|-|-|-|-|X|X|
            uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
            // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
            *it++ = b64table[h | ((*data & 0xF0) >> 4)];
            // |-|-|-|-|X|X|X|X|
            h = (uint8_t) (*data++ & 0x0F) << 2;
            *it++ = b64table[h];
            *it++ = '=';
        }

        *it = '\0';
        // own the memory
        zcstring ret(out, it-out, true);
        return std::move(ret);
    }

    buffer_t base64::decode(const uint8_t *in, size_t size) {
        buffer_t b((uint32_t) (size/4)*3);
        static const unsigned char ASCII_LOOKUP[256] =
        {
            /* ASCII table */
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };
        size_t sz = size;
        const uint8_t *it = in;

        while (sz > 4) {
            if (ASCII_LOOKUP[it[0]] == 64 ||
                ASCII_LOOKUP[it[1]] == 64 ||
                ASCII_LOOKUP[it[2]] == 64 ||
                ASCII_LOOKUP[it[3]] == 64)
            {
                // invalid base64 character
                throw std::runtime_error("invalid base64 encoded string passed");
            }

            b.append((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
            b.append((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
            b.append((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
            sz -= 4;
            it += 4;
        }
        int i = 0;
        while ((it[i] != '=') && (ASCII_LOOKUP[it[i]] != 64) && (i++ < 4));
        if ((sz-i) && (it[i] != '=')) {
            // invalid base64 character
            throw std::runtime_error("invalid base64 encoded string passed");
        }
        sz -= 4-i;

        if (sz > 1) {
            b.append((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
        }
        if (sz > 2) {
            b.append((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
        }
        if (sz > 3) {
            b.append((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
        }

        return std::move(b);
    }

    char* utils::strndup(const char *str, size_t len) {
        char *nstr = nullptr;
        if ((nstr = (char *) memory::alloc(len+1)) != nullptr) {
            strncpy(nstr, str, len);
            nstr[len] = '\0';
        }
        return nstr;
    }

    int64_t utils::strtonum(const zcstring &str, int base, long long min, long long max) {
        long long l;
        char *ep;

        if (min > max) {
            throw std::range_error("min value specified is greater than max");
        }

        errno = 0;
        l = strtoll(str.str, &ep, base);
        if (errno != 0 || str == ep || *ep != '\0') {
            throw std::range_error("invalid converting t o number failed");
        }

        if (l < min) {
            throw std::range_error("converted value is less than min value");
        }

        if (l > max) {
            throw std::range_error("converted value is greator than max value");
        }

        return (l);
    }

    const std::vector<char *> utils::strsplit(zcstring &str, const char *delim) {
        int		count;
        char		*ap = NULL, *ptr = str.str, *eptr = ptr + str.len;
        std::vector<char*> out;

        count = 0;
        for (ap = strsep(&ptr, delim); ap != NULL; ap = strsep(&ptr, delim)) {
            out.push_back(ap);
            if (*ap != '\0' && ap != eptr) {
                ap++;
                count++;
            }
        }

        return std::move(out);
    }

    zcstring utils::strstrip(zcstring &str, char strip) {
        size_t	len = str.len;
        char		*s, *p;

        buffer_t b(str.len);
        void *tmp = b;
        p = (char *)tmp;

        for (s = str.str; s < (str.str + len); s++) {
            if (*s == strip)
                continue;
            *p++ = *s;
        }
        b.seek(p - (char *)tmp);

        // the zcstr will take ownership of the buffer
        return std::move(zcstring(b));
    }

    void* utils::memfind(void *src, size_t slen, const void *needle, size_t len) {
        size_t pos;

        for (pos = 0; pos < slen; pos++) {
            if ( *((u_int8_t *)src + pos) != *(u_int8_t *)needle)
                continue;

            if ((slen - pos) < len)
                return (NULL);

            if (!memcmp((u_int8_t *)src + pos, needle, len))
                return ((u_int8_t *)src + pos);
        }

        return (NULL);
    }

    zcstring utils::md5Hash(const uint8_t *data, size_t len) {
        uint8_t RAW[MD5_DIGEST_LENGTH];
        char *OUT = (char *)memory::alloc((MD5_DIGEST_LENGTH*2)+1);
        MD5(data, len, RAW);
        ssize_t rc = 0;
        for (size_t i = 0; i < sizeof(RAW); i++) {
            rc += sprintf((OUT+rc), "%02x", RAW[i]);
        }
        OUT[rc] = '\0';

        // the memory now belong to caller
        zcstring tmp(OUT, (size_t)rc, true);
        return std::move(tmp);
    }
}