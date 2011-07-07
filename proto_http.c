#include "proto_http.h"
#include "stud.h"

int on_body(http_parser *parser, const char *at, size_t len) {
  proxystate *ps = parser->data;
  ps->ph.body = realloc(ps->ph.body, ps->ph.body_sz + len);
  memcpy(ps->ph.body + ps->ph.body_sz, at, len);
  ps->ph.body_sz += len;
  return 0;
}

int on_header_field(http_parser *parser, const char *at, size_t len)
{
  proxystate *ps = parser->data;
  if (ps->ph.last_was_value) {
    ps->ph.nlines++;

    if (ps->ph.nlines == MAX_HEADER_LINES) {
      return -1;  /* max headers.  error.  handle better */
    }

    /* strip off any inbound x-forwarded-{for,proto} headers */
    if (len >= 11 && strncasecmp(at, "x-forwarded", 11) == 0) {
      ps->ph.stripped_last_header = 1;
      ps->ph.nlines--;  /* revert nlines increment so we can resuse the slot */
      return 0;
    }

    CURRENT_LINE->field_len = len;
    CURRENT_LINE->field = malloc(len+1); /* LOSING HERE */
    strncpy(CURRENT_LINE->field, at, len);

    CURRENT_LINE->value = NULL;
    CURRENT_LINE->value_len = 0;
  }
  else {
    assert(CURRENT_LINE->value == NULL);
    assert(CURRENT_LINE->value_len == 0);

    CURRENT_LINE->field_len += len;
    CURRENT_LINE->field = realloc(CURRENT_LINE->field,
        CURRENT_LINE->field_len+1);
    strncat(CURRENT_LINE->field, at, len);

    /* strip off any inbound x-forwarded-{for,proto} headers */
    if (CURRENT_LINE->field_len >= 11 &&
        strncasecmp(CURRENT_LINE->field, "x-forwarded", 11) == 0) {
      free(CURRENT_LINE->field);
      CURRENT_LINE->field = NULL;
      ps->ph.stripped_last_header = 1;
      ps->ph.nlines--;  /* revert nlines increment so we can resuse the slot */
      return 0;
    }
  }

  CURRENT_LINE->field[CURRENT_LINE->field_len] = '\0';
  ps->ph.last_was_value = 0;
  return 0;
}

int on_message_complete(http_parser *parser) {
  proxystate *ps = parser->data;
  ps->ph.done_parsing_http = 1;
  return 0;
}

int on_header_value(http_parser *parser, const char *at, size_t len) {
  proxystate *ps = parser->data;

  /* The previous header was excluded, so let's exclude its value too. */
  if (ps->ph.stripped_last_header) {
    ps->ph.stripped_last_header = 0;  /* reset so it continues for next field */
    return 0;
  }

  if (!ps->ph.last_was_value) {
    CURRENT_LINE->value_len = len;
    CURRENT_LINE->value = malloc(len+1);  /* LOSING HERE */
    strncpy(CURRENT_LINE->value, at, len);
  }
  else {
    CURRENT_LINE->value_len += len;
    CURRENT_LINE->value = realloc(CURRENT_LINE->value,
      CURRENT_LINE->value_len+1);
    strncat(CURRENT_LINE->value, at, len);
  }

  CURRENT_LINE->value[CURRENT_LINE->value_len] = '\0';
  ps->ph.last_was_value = 1;
  return 0;
}

int cb_url(http_parser *parser, const char *at, size_t length) {
  char *uri = malloc(length + 1);
  ((proxystate*)parser->data)->ph.uri = uri;
  memcpy(uri, at, length);

  uri[length] = '\0';

  return 0;
}

#define MAX_IPv6_LENGTH 40
int cb_headers_complete(http_parser *parser) {
  proxystate *ps = parser->data;
  ps->ph.method = parser->method;
  struct sockaddr *sa = (struct sockaddr *)&ps->remote_ip;
  struct sockaddr *found_addr;
  if (sa->sa_family == AF_INET) {
    found_addr = (struct sockaddr *)&(((struct sockaddr_in*)sa)->sin_addr);
  }
  else {
    found_addr = (struct sockaddr *)&(((struct sockaddr_in6*)sa)->sin6_addr);
  }
  ps->ph.nlines++;
  CURRENT_LINE->field = strdup("X-Forwarded-For");
  CURRENT_LINE->field_len = 15; /* strlen("X-Forwarded-For"); */
  CURRENT_LINE->value = malloc(MAX_IPv6_LENGTH);
  inet_ntop(ps->remote_ip.ss_family, found_addr,
            CURRENT_LINE->value, MAX_IPv6_LENGTH);
  CURRENT_LINE->value_len = strlen(CURRENT_LINE->value);

  ps->ph.nlines++;
  CURRENT_LINE->field = strdup("X-Forwarded-Proto");
  CURRENT_LINE->field_len = 17; /* strlen("X-Forwarded-Proto"); */
  CURRENT_LINE->value = strdup("https");
  CURRENT_LINE->value_len = 5; /* strlen("https"); */
  return 0;
}

#define MAX_TOTAL_HEADER_LENGTH 1024 * 64
char *assemble_headers(void *pre_ps) {
  proxystate *ps = pre_ps;
  char *final = malloc(MAX_TOTAL_HEADER_LENGTH);
  char *mover = final;
  int used_sz = 0;
  int i = 0;
  for (i = 0; i < ps->ph.nlines; i++) {
    /* Not sure why some fields < nlines are null.
      started after initing everything to NULL
    if (ps->ph.header[i].field == NULL) {
      printf("Found NULL at nlines: %d\n", i);
      continue;
    } */
    int remaining_sz = 1024 * 64 - used_sz;
    int sz = snprintf(mover, remaining_sz, "%.*s: %.*s\n",
      (int)ps->ph.header[i].field_len,
      ps->ph.header[i].field,
      (int)ps->ph.header[i].value_len,
      ps->ph.header[i].value);
    if (sz >= remaining_sz) {
      return NULL;
    }
    else {
      mover += sz;
      used_sz += sz;
      if (used_sz > MAX_TOTAL_HEADER_LENGTH - 3) {
        /* minus 3 to save room for the \r\n\0 at the end */
        return NULL;  /* reached more header size than we can handle */
      }
    }
/*    int field_sz = ps->ph.header[i].field_len;
    int value_sz = ps->ph.header[i].value_len;
    used_sz += field_sz + 2 + value_sz + 1;
    if (used_sz > 1024 * 64) {
      return NULL;
    }
    memcpy(mover, ps->ph.header[i].field, field_sz);
    mover += field_sz;
    memcpy(mover + field_sz, ": ", 2);
    mover += 2;
    memcpy(mover + field_sz + 2, ps->ph.header[i].value, value_sz);
    mover += value_sz;
    memcpy(mover + field_sz + 2 + 1, "\n", 1);
    mover += 1; */
  }
  final[used_sz]   = '\r';
  final[used_sz+1] = '\n';
  final[used_sz+2] = '\0';
  return final;
}
