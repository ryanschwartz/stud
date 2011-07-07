#ifndef proto_http_h
#define proto_http_h

#include "http-parser/http_parser.h"

#define CURRENT_LINE (&ps->ph.header[ps->ph.nlines-1])
#define MAX_HEADER_LINES 2000

struct line {
  char *field;
  size_t field_len;
  char *value;
  size_t value_len;
};

typedef struct proto_http {
    http_parser_settings settings;
    http_parser *parser;
    struct line header[MAX_HEADER_LINES];
    char last_was_value;
    int nlines;
    char stripped_last_header;
    enum http_method method;
    char *uri;
    char *body;
    int body_sz;
    char done_parsing_http;
} proto_http;


char *assemble_headers(void *ps);
int on_body(http_parser *parser, const char *at, size_t len);
int on_header_field(http_parser *parser, const char *at, size_t len);
int on_header_value(http_parser *parser, const char *at, size_t len);
int on_message_complete(http_parser *parser);
int on_header_value(http_parser *parser, const char *at, size_t len);
int cb_url(http_parser *parser, const char *at, size_t length);
int cb_headers_complete(http_parser *parser);

#endif
