#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <parser.c>


typedef struct {
    ngx_conf_t                      *cf;
    ngx_array_t                     *searches; // array of ngx_http_php_session_search_t
} ngx_http_php_session_loc_conf_t;

typedef struct {
    ngx_uint_t                       result_index;
    ngx_str_t                        result_string;
    
    ngx_str_t                        session;
    ngx_array_t                     *session_lengths;
    ngx_array_t                     *session_values;
    
    ngx_str_t                        searchkey;
    ngx_array_t                     *searchkey_lengths;
    ngx_array_t                     *searchkey_values;
} ngx_http_php_session_search_t;
    
typedef struct {
    ngx_array_t                     *sessions;
} ngx_http_php_session_ctx_t;


static void *ngx_http_php_session_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_php_session_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char * ngx_http_php_session_parse_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_php_session_compile_session(ngx_conf_t *cf, ngx_http_php_session_search_t *search);
static ngx_int_t ngx_http_php_session_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
    

static ngx_command_t  ngx_http_php_session_commands[] = {
 { ngx_string("php_session_parse"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
    ngx_http_php_session_parse_directive, //ngx_conf_set_keyval_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL},
    ngx_null_command
};

static ngx_http_module_t  ngx_http_php_session_module_ctx = {
    NULL,                                   /* preconfiguration */
    NULL,                                   /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_php_session_create_loc_conf,   /* create location configuration */
    ngx_http_php_session_merge_loc_conf     /* merge location configuration */
};

ngx_module_t  ngx_http_php_session_module = {
    NGX_MODULE_V1,
    &ngx_http_php_session_module_ctx,      /* module context */
    ngx_http_php_session_commands,         /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};
static void *
ngx_http_php_session_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_php_session_loc_conf_t *pscf;
    
    pscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_php_session_loc_conf_t));
    if (pscf == NULL) {
        return NGX_CONF_ERROR;
    }
    
    pscf->cf = cf;
    
    return pscf;
}

static char *
ngx_http_php_session_merge_loc_conf(ngx_conf_t *cf, void *parent, 
    void *child)
{
    ngx_http_php_session_loc_conf_t *prev = parent;
    ngx_http_php_session_loc_conf_t *conf = child;
    
    if (prev->searches != NULL)
    {
        conf->searches = prev->searches;
    }
    
    return NGX_CONF_OK;
}


static char *
ngx_http_php_session_parse_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_php_session_loc_conf_t     *pscf = conf;
    ngx_str_t                           *value;
    ngx_http_php_session_search_t       *search;
    ngx_http_variable_t                 *v;
    ngx_int_t                            index;
    
    if (pscf->searches == NULL)
    {
        pscf->searches = ngx_array_create(cf->pool, cf->args->nelts, sizeof(ngx_http_php_session_search_t));
        if(pscf->searches == NULL) {
            return NGX_CONF_ERROR;
        }
    }
    
    search = ngx_array_push(pscf->searches);
    if (search == NULL) {
        return NGX_CONF_ERROR;
    }
    
    value = cf->args->elts;
    
    if (value[1].data[0] != '$') {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid variable name \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }
    
    value[1].len--;
    value[1].data++;
    
    v = ngx_http_add_variable(cf, &value[1], NGX_HTTP_VAR_CHANGEABLE);
    if (v == NULL) {
        return NGX_CONF_ERROR;
    }
    
    index = ngx_http_get_variable_index(cf, &value[1]);
    if (index == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }
    
    if (v->get_handler == NULL)
    {
        v->get_handler = ngx_http_php_session_variable;
    }
    
    search->result_index = index;
    
    search->session.len = value[2].len;
    search->session.data = value[2].data;
    search->searchkey.len = value[3].len;
    search->searchkey.data = value[3].data;
    
    if (ngx_http_php_session_compile_session(cf, search) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}

static char *
ngx_http_php_session_compile_session(ngx_conf_t *cf, ngx_http_php_session_search_t *search)
{
    ngx_http_php_session_loc_conf_t *pslc = ngx_http_conf_get_module_loc_conf(cf, ngx_http_php_session_module);
    
    ngx_http_script_compile_t sc_session, sc_searchkey;
    ngx_memzero(&sc_session, sizeof(ngx_http_script_compile_t));
    ngx_memzero(&sc_searchkey, sizeof(ngx_http_script_compile_t));
    
    sc_session.cf = pslc->cf;
    sc_session.source = &search->session;
    sc_session.lengths = &search->session_lengths;
    sc_session.values = &search->session_values;
    sc_session.variables = ngx_http_script_variables_count(&search->session);
    sc_session.complete_lengths = 1;
    sc_session.complete_values = 1;
    
    if (ngx_http_script_compile(&sc_session) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    
    sc_searchkey.cf = pslc->cf;
    sc_searchkey.source = &search->searchkey;
    sc_searchkey.lengths = &search->searchkey_lengths;
    sc_searchkey.values = &search->searchkey_values;
    sc_searchkey.variables = ngx_http_script_variables_count(&search->searchkey);
    sc_searchkey.complete_lengths = 1;
    sc_searchkey.complete_values = 1;
    
    if (ngx_http_script_compile(&sc_searchkey) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_php_session_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
    printf("session_variable_handler\n");
    
    ngx_http_php_session_loc_conf_t     *pscf = ngx_http_get_module_loc_conf(r, ngx_http_php_session_module);
    ngx_uint_t                           search_num;
    ngx_http_php_session_search_t       *search;
    ngx_http_variable_value_t           *vv;
    
    for (search_num = 0; search_num < pscf->searches->nelts; search_num++) {
        search = (ngx_http_php_session_search_t *) pscf->searches->elts + (search_num * sizeof(ngx_http_php_session_search_t));
        if (search->result_index == data)
        {
            break;
        }
    }
    
    if (ngx_http_script_run(r, &search->session, search->session_lengths->elts, 0 , search->session_values->elts) == NULL) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "session evaluation failed");
        return NGX_ERROR;
    }
    
    if (ngx_http_script_run(r, &search->searchkey, search->searchkey_lengths->elts, 0 , search->searchkey_values->elts) == NULL) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "searchkey evaluation failed");
        return NGX_ERROR;
    }
    
    if (extract_value(&search->searchkey, &search->session, &search->result_string) != NGX_OK) {
        return NGX_ERROR;
    }
    
    vv = r->variables + search->result_index;
    vv->len = search->result_string.len;
    vv->data = search->result_string.data;
    vv->valid = 1;
    
    return NGX_OK;
}

