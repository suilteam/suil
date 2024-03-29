//
// Created by dc on 9/7/17.
//
#include <fcntl.h>
#include <sys/param.h>

#include <suil/http/clientapi.h>
#include <sys/mman.h>

namespace suil {
    namespace http {
        namespace client {

#undef  CRLF
#define CRLF "\r\n"

            size_t Form::encode(OBuffer &out) const {
                if (data.empty() && files.empty()) {
                    strace("trying to encode an empty form");
                    return 0;
                }

                if (encoding == URL_ENCODED) {
                    bool first{true};
                    for(auto& pair : data) {
                        if (!first) {
                            out << "&";
                        }
                        first = false;
                        out << pair.first << "=" << pair.second;
                    }

                    return out.size();
                } else {
                    /* multi-part encoded form */
                    for (auto& fd: data) {
                        out << "--" << boundary << CRLF;
                        out << "Content-Disposition: form-data; name=\""
                            << fd.first << "\"" << CRLF;
                        out << CRLF << fd.second << CRLF;
                    }

                    if (files.empty()) {
                        out << "--" << boundary << "--";
                        return out.size();
                    }

                    size_t  content_length{out.size()};
                    /* encode file into upload buffers */
                    for (auto& ff: files) {
                        OBuffer tmp(127);
                        tmp << "--" << boundary << CRLF;
                        tmp << "Content-Disposition: form-data; name=\""
                            << ff.name << "\"; filename=\"" << basename((char *) ff.path.data())
                            << "\"" << CRLF;
                        if (ff.ctype) {
                            /* append content type header */
                            tmp << "Content-Type: " << ff.ctype << CRLF;
                        }
                        tmp << CRLF;
                        content_length += tmp.size() + ff.size;

                        Upload upload(&ff, std::move(String(tmp)));
                        uploads.push_back(std::move(upload));
                        /* \r\n will added at the end of each upload */
                        content_length += sizeofcstr(CRLF);
                    }

                    /* --boundary--  will be appended */
                    content_length += sizeofcstr("----") + boundary.size();
                    return content_length;
                }
            }

            int Form::Upload::open() {
                if (fd > 0) {
                    return fd;
                }
                fd = ::open(file->path(), O_RDWR);
                if (fd < 0) {
                    /* opening file failed */
                    throw Exception::create("open '", file->path(),
                                             "' failed: ", errno_s);
                }

                return fd;
            }

            void Form::Upload::close() {
                if (fd > 0) {
                    ::close(fd);
                    fd = -1;
                }
            }

            void Response::receive(SocketAdaptor &sock, int64_t timeout) {
                OBuffer tmp(1023);
                do {
                    size_t nrd = tmp.capacity();
                    if (!sock.read(&tmp[0], nrd, timeout)) {
                        /* failed to receive headers*/
                        throw Exception::create("receiving Request failed: ", errno_s);
                    }
                    auto remaining = feed2(tmp.data(), nrd);
                    if (remaining == nrd) {
                        throw Exception::create("parsing headers failed: ",
                              http_errno_name((enum http_errno) http_errno));
                    }
                } while (!headers_complete);

                if (body_read || content_length == 0) {
                    strace("%s - Response has no body to read: %lu", sock.id(), content_length);
                    return;
                }

                /* received and parse body */
                bool chunked{(flags & F_CHUNKED) == F_CHUNKED};
                size_t len  = 0, left = chunked ? 2048 : content_length+20;
                // read body in chunks
                tmp.reserve(2048);

                do {
                    tmp.reset(0, true);
                    if (!chunked)
                        left -= len;
                    len = std::min(left, tmp.capacity());
                    // read whatever is available
                    if (!sock.read(&tmp[0], len, timeout)) {
                        throw Exception::create("receive failed: ", errno_s);
                    }

                    // parse header line
                    auto remaining = feed2(tmp.data(), len);
                    if (remaining == len) {
                        throw Exception::create("parsing  body failed: ",
                                     http_errno_name((enum http_errno )http_errno));
                    }
                    // no need to reset buffer
                } while (!body_complete);
            }

            int Response::handle_body_part(const char *at, size_t length) {
                if (reader == nullptr) {
                    return parser::handle_body_part(at, length);
                }
                else {
                    if (reader(at, length)) {
                        return 0;
                    }
                    return -1;
                }
            }

            int Response::handle_headers_complete() {
                /* if reader is configured, */
                if (reader != nullptr) {
                    reader(nullptr, content_length);
                }
                else {
                    body.reserve(content_length + 2);
                }
                return 0;
            }

            bool MemoryOffload::map_region(size_t len) {
                size_t total = len;
                int page_sz = getpagesize();
                total += page_sz-(len % page_sz);
                data = (char *) mmap(nullptr, total, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE , -1, 0);
                if (data == nullptr || errno != 0) {
                    swarn("client::MemoryOffload mmap failed: %s", errno_s);
                    return false;
                }
                is_mapped = true;
                return true;
            }

            MemoryOffload::~MemoryOffload() {
                if (data) {
                    if (is_mapped) {
                        /* unmap mapped memory */
                        munmap(data, offset);
                    }
                    else {
                        /* free allocated memory */
                        free(data);
                    }
                    data = nullptr;
                }
            }

            int Response::msg_complete() {
                if (reader) {
                    reader(nullptr, 0);
                }
                body_read = true;
                return parser::msg_complete();
            }

            const strview Response::contenttype() const {
                auto it = headers.find("Content-Type");
                if (it != headers.end()) {
                    return it->second;
                }

                return strview();
            }

            Request& Request::operator=(Request &&o) noexcept {
                if (this != &o) {
                    sock_ptr = o.sock_ptr;
                    sock = *sock_ptr;
                    headers = std::move(o.headers);
                    arguments = std::move(o.arguments);
                    method = o.method;
                    resource = std::move(o.resource);
                    form = std::move(o.form);
                    body   = std::move(o.body);
                    bodyFd = std::move(o.bodyFd);
                    o.sock_ptr = nullptr;
                    o.method = Method::Unknown;
                    o.cleanup();
                }
                return *this;
            }

            Request::Request(Request &&o) noexcept
                : sock_ptr(o.sock_ptr),
                  sock(*sock_ptr),
                  headers(std::move(o.headers)),
                  arguments(std::move(o.arguments)),
                  method(o.method),
                  resource(std::move(o.resource)),
                  form(std::move(o.form)),
                  body(std::move(o.body)),
                  bodyFd(std::move(o.bodyFd))
            {
                o.sock_ptr = nullptr;
                o.method = Method::Unknown;
            }

            Request& Request::operator<<(Form &&f) {
                form = std::move(f);
                String key("Content-Type");
                String val;
                if (form.encoding == Form::URL_ENCODED) {
                    val = String("application/x-www-form-urlencoded").dup();
                }
                else {
                    val = utils::catstr("multipart/form-data; boundary=", form.boundary);
                }
                headers.emplace(key.dup(), std::move(val));

                return *this;
            }

            Request& Request::operator<<(suil::File&& f) {
                if (!body.empty()) {
                    // warn about overloading body
                    iwarn("request body already set, overloading with file");
                    body.clear();
                }
                bodyFd = std::move(f);
                return Ego;
            }

            OBuffer& Request::buffer(const char *content_type) {
                if (bodyFd.valid()) {
                    // warn about overloading body
                    iwarn("request body already set as a file, overriding with buffer");
                    bodyFd.close();
                }

                hdrs("Content-Type", content_type);
                return body;
            }

            uint8_t& Request::buffer(size_t size, const char *contentType)
            {
                buffer(contentType);
                body.reserve(size);
                body.seek(size);
                return body[0];
            }

            void Request::encodeargs(OBuffer &dst) const {
                if (!arguments.empty()) {
                    dst << "?";
                    bool first{true};
                    for(auto& arg: arguments) {
                        if (!first) {
                            dst << "&";
                        }
                        first = false;
                        dst << arg.first << "=" << arg.second;
                    }
                }
            }

            void Request::encodehdrs(OBuffer &dst) const {
                if (!headers.empty()) {
                    for (auto& hdr: headers) {
                        dst << hdr.first << ": " << hdr.second << CRLF;
                    }
                }
            }

            size_t Request::buildbody() {
                size_t content_length{0};
                if (utils::matchany(method, Method::Put, Method::Post)) {
                    if (form) {
                        /*encode form if available */
                        content_length = form.encode(body);
                    }
                    else {
                        content_length = body.size();
                    }
                }

                return content_length;
            }

            void Request::submit(int64_t timeout) {
                OBuffer head(1023);
                size_t  content_length{buildbody()};
                hdrs("Content-Length", content_length);

                head << method_name(method) << " " << resource;
                encodeargs(head);
                head << " HTTP/1.1" << CRLF;
                hdrs("Date", Datetime()(Datetime::HTTP_FMT));
                encodehdrs(head);
                head << CRLF;
                size_t nwr = sock.send(head.data(), head.size(), timeout);
                if (nwr != head.size()) {
                    /* sending headers failed, no need to send body */
                    throw Exception::create("send Request failed: (", nwr, ",",
                             head.size(), ")", errno_s);
                }
                if (content_length == 0) {
                    sock.flush();
                    return;
                }

                if (Ego.bodyFd.valid()) {
                    // send body as a file
                    /* send file */
                    int fd = Ego.bodyFd.raw();
                    auto size =(size_t) Ego.bodyFd.tell();
                    nwr = sock.sendfile(fd, 0, size, timeout);
                    if (nwr != size) {
                        throw Exception::create("sending file: '", fd,
                                                "' failed: ", errno_s);
                    }
                }
                else if (!body.empty()) {
                    /* send body */
                    nwr = sock.send(body.data(), body.size(), timeout);
                    if (nwr != body.size()) {
                        /* sending headers failed, no need to send body */
                        throw Exception::create("send Request failed: ", errno_s);
                    }
                }

                if (!form.uploads.empty()) {
                    /* send upload files one after the other */
                    for (auto &up: form.uploads) {
                        int ffd = up.open();
                        nwr = sock.send(up.head(), up.head.size(), timeout);
                        if (nwr != up.head.size()) {
                            /* sending upload head failed, no need to send body */
                            throw Exception::create("send Request failed: ", errno_s);
                        }

                        /* send file */
                        nwr = sock.sendfile(ffd, 0, up.file->size, timeout);
                        if (nwr != up.file->size) {
                            throw Exception::create("uploading file: '", up.file->path(),
                                                     "' failed: ", errno_s);
                        }

                        nwr = sock.send(CRLF, sizeofcstr(CRLF), timeout);
                        if (nwr != sizeofcstr(CRLF)) {
                            throw Exception::create("send CRLF failed: ", errno_s);
                        }

                        /* close boundary*/
                        up.close();
                    }

                    /* send end of boundary tag */
                    head.reset(1023, true);
                    head << "--" << form.boundary << "--"
                         << CRLF;

                    nwr = sock.send(head.data(), head.size(), timeout);
                    if (nwr != head.size()) {
                        throw Exception::create("send terminal boundary failed: ", errno_s);
                    }
                }
                sock.flush();
            }

            Response Session::perform(handle_t& h, Method m, const char *resource, request_builder_t& builder, ResponseWriter& rd) {
                Request& req = h.req;
                Response resp;
                req.reset(m, resource, false);

                for(auto& hdr: headers) {
                    String key(hdr.first.data(), hdr.first.size(), false);
                    String val(hdr.second(), hdr.second.size(), false);
                    req.hdr(std::move(key), std::move(val));
                }

                if (builder != nullptr && !builder(req)) {
                    throw Exception::create("building Request '", resource, "' failed");
                }

                if (!req.sock.isopen()) {
                    /* open a new socket for the Request */
                    if (!req.sock.connect(addr, timeout)) {
                        throw Exception::create("Connecting to '", host(), ":",
                                                 port, "' failed: ", errno_s);
                    }
                }

                req.submit(timeout);
                resp.reader = rd;
                resp.receive(req.sock, timeout);
                return std::move(resp);
            }


            void Session::connect(handle_t &h, CaseMap<String> /* @TODO implement headers */) {
                Response resp = std::move(perform(h, Method::Connect, "/"));
                if (resp.status() != Status::OK) {
                    /* connecting to server failed */
                    throw Exception::create("sending CONNECT to '", host(), "' failed: ",
                                            http::status_text(resp.status()));
                }

                /* cleanup Request and return it fresh */
                h.req.cleanup();
            }

            Session::handle_t Session::connect() {
                auto h = handle();
                auto& req = h.req;
                if (!req.sock.isopen()) {
                    /* open a new socket for the Request */
                    if (!req.sock.connect(addr, timeout)) {
                        idebug("connecting to '%s' failed - %s", host(), errno_s);
                    }
                }
                return std::move(h);
            }

            Response Session::head(handle_t &h, const char *resource, CaseMap<String> /* @TODO implement headers */) {
                Response resp = std::move(perform(h, Method::Connect, resource));
                return std::move(resp);
            }

#undef CRLF
        }
    }
}