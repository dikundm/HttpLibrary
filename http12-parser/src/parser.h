//
// Created by s.fionov on 23.12.16.
//

#ifndef HTTP_PARSER_PARSER_H
#define HTTP_PARSER_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "logger.h"
#include "http_header.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum parser_type {
    HTTP1_REQUEST = 1,
    HTTP1_RESPONSE,
    HTTP2_REQUEST,
    HTTP2_RESPONSE
};

enum http_version {
    HTTP1 = 1,
    HTTP2 = 2,
};

enum connection_type {
    HTTP_SERVER_CONNECTION = 1,
    HTTP_CLIENT_CONNECTION = 2,
};

struct http_message {
    int stream_id;
    struct http_headers *headers;
};

struct http_parser_context {
    union {
        struct http1_parser_context *h1;
        struct http2_parser_context *h2;
    };
    /* Parser type */
    enum http_version version;
    enum connection_type type;
    /* Logger */
    logger *log;
    /* Callbacks and attachment */
    struct parser_callbacks *callbacks;
    void *attachment;
};

struct parser_callbacks {
    /**
     * HTTP/1.1 headers callback
     * @param headers HTTP headers of current HTTP message
     */
    void (*h1_headers)(void *attachment, struct http_headers *headers);

    /**
     * HTTP/1.1 start of data callback
     * @param headers HTTP headers of current HTTP message
     * @return 1 tells parser to decompress data, 0 tells to remain data uncompressed
     */
    bool (*h1_data_started)(void *attachment, struct http_headers *headers);

    /**
     * HTTP/1.1 data callback
     * @param headers HTTP headers of current HTTP message
     * @param data Processed HTTP/1.1 body data
     * @param length Data length
     */
    void (*h1_data)(void *attachment, struct http_headers *headers, const char *data, size_t length);

    /**
     * HTTP/1.1 end of data callback
     * @param headers HTTP headers of current HTTP message
     */
    void (*h1_data_finished)(void *attachment, struct http_headers *headers);

    /**
     * HTTP/2 frame callback
     * @param ...
     */
    void (*h2_frame)();

    /**
     * HTTP/2 header callback
     * @param headers HTTP headers of current HTTP message
     */
    void (*h2_headers)(void *attachment, struct http_headers *headers, int32_t stream_id);

    /**
     * HTTP/2 start of data callback
     * @param headers HTTP headers of current HTTP message
     * @return tells parser to decompress data, 0 tells to remain data uncompressed
     */
    bool (*h2_data_started)(void *attachment, struct http_headers *headers, int32_t stream_id);

    /**
     * HTTP/2 data callback
     * @param headers HTTP headers of current HTTP message
     * @param data Processed HTTP/2 stream data
     * @param length Data length
     */
    void (*h2_data)(void *attachment, struct http_headers *headers, int32_t stream_id, const char *data, size_t length);

    /**
     * HTTP/2 end of data callback
     * @param headers HTTP headers of current HTTP message
     */
    void (*h2_data_finished)(void *attachment, struct http_headers *headers, int32_t stream_id, int is_reset);

    /**
     * HTTP protocol has generated raw data output (in result of send methods or internal logic)
     * @param data Raw output data to write to socket
     * @param len Length of data
     */
    void (*raw_output)(void *attachment, const char *data, size_t len);
};

/**
 * Parser error type
 */
enum error_type {
    PARSER_OK = 0,
    PARSER_ALREADY_CONNECTED_ERROR = 101,
    PARSER_HTTP_PARSE_ERROR = 102,
    PARSER_ZLIB_ERROR = 103,
    PARSER_NULL_POINTER_ERROR = 104,
    PARSER_INVALID_ARGUMENT_ERROR = 105,
    PARSER_INVALID_STATE_ERROR = 106,
};


extern int http_parser_open(logger *log,
                            enum http_version version,
                            enum connection_type type,
                            struct parser_callbacks *callbacks,
                            void *attachment,
                            struct http_parser_context **p_context);

extern int http_parser_input(struct http_parser_context *context, const char *data, size_t length);

extern int http_parser_close(struct http_parser_context *context);

extern int http_parser_h1_send_headers(struct http_parser_context *context, struct http_headers *headers);
extern int http_parser_h1_send_data(struct http_parser_context *context, const char *data, size_t len, bool eof);
extern int http_parser_h2_send_settings(struct http_parser_context *context );
extern int http_parser_h2_send_headers(struct http_parser_context *context, int32_t stream_id, struct http_headers *headers);
extern int http_parser_h2_send_data(struct http_parser_context *context, int32_t stream_id, const char *data, size_t len, bool eof);
extern int http_parser_h2_reset_stream(struct http_parser_context *context, int32_t stream_id, int error_code);
extern int http_parser_h2_goaway(struct http_parser_context *context, int error_code);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif //HTTP_PARSER_PARSER_H
