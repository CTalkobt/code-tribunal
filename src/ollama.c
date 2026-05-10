#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "council.h"

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} GrowBuf;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    GrowBuf *g = userdata;
    size_t   n = size * nmemb;

    if (g->len + n + 1 > g->cap) {
        g->cap = (g->len + n + 1) * 2;
        g->buf = realloc(g->buf, g->cap);
        if (!g->buf) return 0;
    }
    memcpy(g->buf + g->len, ptr, n);
    g->len += n;
    g->buf[g->len] = '\0';
    return n;
}

/* Minimal JSON string escape - sufficient for prompt injection into JSON */
static void json_escape(const char *in, char *out, size_t out_size)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 4 < out_size; i++) {
        switch (in[i]) {
        case '"':  out[j++] = '\\'; out[j++] = '"';  break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
        case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
        case '\t': out[j++] = '\\'; out[j++] = 't';  break;
        default:   out[j++] = in[i]; break;
        }
    }
    out[j] = '\0';
}

/* Extract "content" value from a single Ollama streaming JSON line.
   Ollama /api/chat returns one JSON object per line with:
   {"message":{"role":"assistant","content":"..."}, "done":false} */
static int parse_content(const char *line, char *out, size_t out_size)
{
    const char *p = strstr(line, "\"content\":\"");
    if (!p) return 0;
    p += 11;

    size_t j = 0;
    while (*p && j + 1 < out_size) {
        if (p[0] == '\\' && p[1] == '"') { out[j++] = '"'; p += 2; continue; }
        if (p[0] == '\\' && p[1] == 'n') { out[j++] = '\n'; p += 2; continue; }
        if (p[0] == '\\' && p[1] == '\\') { out[j++] = '\\'; p += 2; continue; }
        if (*p == '"') break;
        out[j++] = *p++;
    }
    out[j] = '\0';
    return j > 0;
}

int ollama_chat(const char *model, const char *system_prompt,
                const char *user_msg, char *response, size_t resp_size)
{
    CURL    *curl;
    CURLcode res;
    int      ret = -1;

    /* Escape inputs */
    char *esc_sys  = malloc(MAX_PROMPT_LEN * 2);
    char *esc_user = malloc(MAX_PROMPT_LEN * 2);
    char *payload  = malloc(MAX_PROMPT_LEN * 4);
    if (!esc_sys || !esc_user || !payload) goto cleanup_mem;

    json_escape(system_prompt, esc_sys,  MAX_PROMPT_LEN * 2);
    json_escape(user_msg,      esc_user, MAX_PROMPT_LEN * 2);

    snprintf(payload, MAX_PROMPT_LEN * 4,
        "{"
        "\"model\":\"%s\","
        "\"stream\":true,"
        "\"messages\":["
          "{\"role\":\"system\",\"content\":\"%s\"},"
          "{\"role\":\"user\",\"content\":\"%s\"}"
        "]"
        "}",
        model, esc_sys, esc_user);

    GrowBuf raw = { .buf = malloc(65536), .len = 0, .cap = 65536 };
    if (!raw.buf) goto cleanup_mem;

    curl = curl_easy_init();
    if (!curl) goto cleanup_buf;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            OLLAMA_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &raw);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        300L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "[ollama] curl error: %s\n", curl_easy_strerror(res));
        goto cleanup_curl;
    }

    /* Concatenate content tokens from streaming lines */
    size_t out_len = 0;
    char  *line    = strtok(raw.buf, "\n");
    char   chunk[4096];
    while (line) {
        if (parse_content(line, chunk, sizeof(chunk))) {
            size_t clen = strlen(chunk);
            if (out_len + clen + 1 < resp_size) {
                memcpy(response + out_len, chunk, clen);
                out_len += clen;
                response[out_len] = '\0';
            }
        }
        line = strtok(NULL, "\n");
    }
    ret = (out_len > 0) ? 0 : -1;

cleanup_curl:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
cleanup_buf:
    free(raw.buf);
cleanup_mem:
    free(esc_sys);
    free(esc_user);
    free(payload);
    return ret;
}
