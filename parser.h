
typedef struct {
    u_char *search_start;
    u_char *search_end;
    unsigned search_len;
    u_char *session_start;
    u_char *session_end;
    unsigned session_len;
} extract_ctx;


ngx_str_t element_separators = ngx_string(";|}");

#define IS_SEPARATOR 0
#define IS_NOT_SEPARATOR 1

#define NEXT_ELEMENT_OK 0
#define NO_NEXT_ELEMENT 1

#define GOT_RESULT 0
#define NO_RESULT 1

static ngx_int_t extract_value(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result);
static ngx_int_t extract_value_loop(extract_ctx *ctx, ngx_str_t *result);
static ngx_int_t is_element_separator(u_char *);
u_char * next_separator_by_level(u_char *string, ngx_uint_t len);
static ngx_int_t shift_element(u_char **start, u_char **end, unsigned *len);
static ngx_int_t store_result(extract_ctx *ctx, ngx_str_t *result);