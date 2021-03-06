//
// Created by dc on 8/28/17.
//
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <suil/http/auth.h>
#include <suil/base64.h>

namespace suil {
    namespace http {

        bool Jwt::decode(Jwt& jwt,String &&jwtstr, String& secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.data(), '.');
            if (tok_sig == nullptr) {
                /* invalid json token */
                throw Error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            String data(jwtstr.data());

            /* get signature hash  */
            String signature =
                    utils::SHA_HMAC256(secret, data, true);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.data());

            if (strcmp(tok_sig, signature.data()) != 0) {
                /* token invalid */
                return false;
            }

            auto parts = data.split(".");
            if (parts.size() != 2) {
                /* invalid token */
                /* invalid json token */
                throw Error::unauthorized(
                        "invalid token");
            }

            String header(utils::base64::decode(parts[0]));
            strace("header: %s", header.data());
            iod::json_decode(jwt.header, header);
            String payload(utils::base64::decode(parts[1]));
            strace("header: %s", payload.data());
            iod::json_decode(jwt.payload, payload);
            return true;
        }

        bool Jwt::verify(String&& jwtstr, String &secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.data(), '.');
            if (tok_sig == nullptr) {
                /* invalid json token */
                throw Error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            String data(jwtstr.data());

            /* get signature hash  */
            String signature =
                    utils::SHA_HMAC256(secret, data);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.data());

            return strcmp(tok_sig, signature.data()) == 0;
        }

        String Jwt::encode(String& secret) {
            /* encode jwt */
            /* 1. base64(hdr) & base64(payload)*/
            std::string raw_hdr(iod::json_encode(header));
            String hdr(std::move(utils::base64::encode(raw_hdr)));
            std::string raw_data(iod::json_encode(payload));
            String data(std::move(utils::base64::encode(raw_data)));

            /* 2. base64(hdr).base64(payload) */
            OBuffer tmp(hdr.size() + data.size() + 4);
            tmp << hdr << "." << data;

            /* 3. HMAC_Sha256 (base64(hdr).base64(payload))*/
            String signature =
                    utils::SHA_HMAC256(secret,
                                       (const uint8_t *) tmp.data(),
                                       tmp.size(), true);

            /* 4. header.payload.signature */
            tmp << "." << signature;
            strace("encoded jwt: %s", tmp.data());

            return std::move(String(tmp));
        }

        String rand_8byte_salt::operator()(const String &) {
            /* simple generate random bytes */
            uint8_t key[8];
            RAND_bytes(key, 8);
            return std::move(utils::hexstr(key, 8));
        }

        String pbkdf2_sha1_hash::operator()(String &passwd, String& salt) {
            /* simply hash using SHA_256 appsalt.passwd.salt*/
            String tmp = utils::catstr(appsalt, ".", salt);
            uint8_t out[SHA_DIGEST_LENGTH];
            int rc = PKCS5_PBKDF2_HMAC_SHA1(passwd.data(), (int) passwd.size(),
                              (const uint8_t*)tmp.data(), (int) tmp.size(),
                                   1000, SHA_DIGEST_LENGTH, out);

            if (rc) {
                /* convert buffer to byte string */
                return std::move(utils::hexstr(out, SHA_DIGEST_LENGTH));
            }

            return String();
        }

        void JwtAuthorization::before(Request& req, Response& resp, Context& ctx) {
            ctx.jwtAuth = this;
            if (req.route().AUTHORIZE) {
                String tkhdr;
                if (use.use == JwtUse::HEADER) {
                    auto auth = req.header(use.key);
                    if (auth.empty() ||
                        strncasecmp(auth.data(), "Bearer ", 7) != 0) {
                        /* user is not authorized */
                        authrequest(resp);
                    }
                    /* temporary dup of the token */
                    tkhdr = String(auth.data(), auth.size(), false);
                    ctx.actualToken = String(&auth[7], auth.size() - 7, false);
                }
                else {
                    // token
                    CookieIterator cookies = req();
                    auto auth = cookies[use.key];
                    if (!auth) {
                        /* user is not authorized */
                        authrequest(resp);
                    }
                    tkhdr = auth;
                    ctx.actualToken = auth;
                }

                if (ctx.actualToken.empty()) {
                    /* no need to proceed, token is invalid */
                    authrequest(resp);
                }

                String token(ctx.actualToken.dup());

                bool valid{false};
                try {
                    valid = Jwt::decode(ctx.jwt, std::move(token), key);
                }
                catch(...) {
                    /* error decoding token */
                    authrequest(resp, "Invalid authorization token.");
                }

                if (!valid || (ctx.jwt.exp() < time(nullptr))) {
                    /* token unauthorized */
                    authrequest(resp);
                }

                if (!req.route().AUTHORIZE.check(ctx.jwt.roles())) {
                    /* token does not have permission to access resource */
                    authrequest(resp, "Access to resource denied.");
                }

                ctx.tokenHdr = tkhdr;
            }
        }

        void JwtAuthorization::after(Request&, http::Response& resp, Context& ctx) {
            /* if authorized token should have been set */
            if (ctx.sendTok) {
                /* append token to request */
                String tok{nullptr};
                /* encode the current json web-token*/
                tok = utils::catstr("Bearer ", ctx.actualToken);
                resp << tok;

                if (use.use == JwtUse::HEADER) {
                    /* token is sent in header */
                    resp.header(use.key.peek(), std::move(tok));
                }
                else {
                    // send token in cookie
                    Cookie cookie(use.key.data());
                    cookie.domain(domain.peek());
                    cookie.path(path.peek());
                    cookie.value(ctx.actualToken.dup());
                    cookie.expires(time(nullptr) + expiry);
                    resp.cookie(cookie);
                }
            } else if(!ctx.redirectUrl.empty()) {
                resp.redirect(Status::FOUND, ctx.redirectUrl.data());
                deleteCookie(resp);
            } else if (ctx.requestAuth) {
                /* send authentication Request */
                resp.header("WWW-Authenticate", authenticate);
                if (ctx.tokenHdr) {
                    throw Error::unauthorized(ctx.tokenHdr.data());
                }
                else {
                    throw Error::unauthorized();
                }
            }
        }
    }
}