
#include <parser.h>

static ngx_int_t
extract_value(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result)
{
    extract_ctx ctx;
    u_char *pos;

    ctx.search_start = searchkey->data;
    pos = next_separator_by_level(searchkey->data, searchkey->len);
    if (pos == NULL) {
        ctx.search_len = searchkey->len;
    } else {
        ctx.search_len = pos - searchkey->data;
    }
    ctx.search_end = searchkey->data + searchkey->len;
    
    ctx.session_start = session->data;
    pos = next_separator_by_level(session->data, session->len);
    if (pos == NULL) {
        ctx.session_len = session->len;
    } else {
        ctx.session_len = pos - session->data;
    }
    ctx.session_end = session->data + session->len;
    
    if (extract_value_loop(&ctx, result) == NO_RESULT){
        result->data = NULL;
        result->len = 0;
    }
    
    return NGX_OK;
}

static ngx_int_t
extract_value_loop(extract_ctx *ctx, ngx_str_t *result)
{
    
    while (ctx->session_start < ctx->session_end) {        
        if (ngx_strncmp(ctx->search_start, ctx->session_start, ctx->search_len) == 0) {
            if (ctx->search_start + ctx->search_len == ctx->search_end) {
                if (shift_element(&ctx->session_start, &ctx->session_end, &ctx->session_len) == NO_RESULT) {
                    return NO_RESULT;
                }
                if (store_result(ctx, result) == NGX_ERROR) {
                    return NGX_ERROR;
                }
                return GOT_RESULT;
            }
            if (shift_element(&ctx->search_start, &ctx->search_end, &ctx->search_len) == NO_RESULT) {
                return NO_RESULT;
            }
            while (ctx->session_start < ctx->session_end && *ctx->session_start != '{') {
                ctx->session_start++;
            }
            ctx->session_start++;
            ctx->session_len = next_separator_by_level(ctx->session_start, ctx->session_end - ctx->session_start) - ctx->session_start;
            if (extract_value_loop(ctx, result) == GOT_RESULT) {
                return GOT_RESULT;
            } else {
                return NO_RESULT;
            }
        }
        
        if (shift_element(&ctx->session_start, &ctx->session_end, &ctx->session_len) == NO_RESULT) {
            return NO_RESULT;
        }
        if (shift_element(&ctx->session_start, &ctx->session_end, &ctx->session_len) == NO_RESULT) {
            return NO_RESULT;
        }
    }
    return NO_RESULT;
}

static ngx_int_t
store_result(extract_ctx *ctx, ngx_str_t *result)
{
    result->data = ctx->session_start;
    result->len = ctx->session_len;
    return NGX_OK;
}

static ngx_int_t
shift_element(u_char **start, u_char **end, unsigned *len)
{
    u_char *pos;
    if ((*end - *start - *len) <= 0) {
        return NO_RESULT;
    }
    pos = next_separator_by_level(*start + *len + 1, *end - *start - *len - 1);
    if (pos == NULL) {
        *start = *start + *len + 1;
        *len = *end - *start;
        return NGX_OK;
    }
    *start = *start + *len + 1;
    *len = pos - *start;
    return NGX_OK;
}

u_char *
next_separator_by_level(u_char *string, ngx_uint_t len)
{
    ngx_uint_t pos = 0;
    ngx_uint_t nested_level = 0;
    
    while (pos < len) {
        if (*(string + pos) == '{') {
            nested_level++;
            pos++;
            continue;
        }
        if (*(string + pos) == '}') {
            nested_level--;
        }
        if (nested_level > 0) {
            pos++;
            continue;
        }
        if (is_element_separator(string + pos) == IS_SEPARATOR) {
            return string + pos;
        }
        pos++;
    }
    return NULL;
}

static ngx_int_t
is_element_separator(u_char *pos)
{
    ngx_uint_t separators_pos;
    for (separators_pos = 0; separators_pos < element_separators.len; separators_pos++) {
        if (*(element_separators.data + separators_pos) == *pos) {
            return IS_SEPARATOR;
        }
    }
    return IS_NOT_SEPARATOR;
}
