

ngx_str_t element_separators = ngx_string(";|}");

#define ELEMENT_IS_SEPARATOR 0
#define ELEMENT_IS_NOT_SEPARATOR 1

#define NEXT_ELEMENT_OK 0
#define NO_NEXT_ELEMENT 1

#define GOT_RESULT 0
#define NO_RESULT 1

static ngx_int_t extract_value(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result);
static ngx_int_t extract_value_next_level(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result);
static ngx_int_t next_element(ngx_str_t *search_element, ngx_str_t *searchkey);
static ngx_int_t is_element_separator(u_char *);
static ngx_int_t store_result(ngx_str_t *session_element, ngx_str_t *result);
