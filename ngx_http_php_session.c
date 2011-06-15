#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <parser.c>


typedef struct {
    ngx_conf_t                      *cf;
    ngx_array_t                     *searches; // array of ngx_http_php_session_search_t
    ngx_array_t                     *extractions; // array of ngx_http_php_value_extraction_t
} ngx_http_php_session_loc_conf_t;

typedef struct {
    ngx_str_t                        value;
    ngx_array_t                     *lengths;
    ngx_array_t                     *values;
} ngx_http_php_session_variable_data_t;

typedef struct {
    ngx_uint_t                       result_index;
} ngx_http_php_session_value_t;

typedef struct {
    ngx_uint_t                       result_index;
    ngx_str_t                        result_string;
    
    ngx_http_php_session_variable_data_t session;
    ngx_http_php_session_variable_data_t searchkey;
} ngx_http_php_session_search_t;

typedef struct {
    ngx_uint_t                       result_index;
    ngx_str_t                        result_string;
    
    ngx_http_php_session_variable_data_t value;
} ngx_http_php_session_value_extraction_t;
    
typedef struct {
    ngx_array_t                     *sessions;
} ngx_http_php_session_ctx_t;


// create location confs
static void *ngx_http_php_session_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_php_session_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

// compile variables
static char * ngx_http_php_session_compile_variable(ngx_http_script_compile_t *sc, ngx_conf_t *cf, ngx_http_php_session_variable_data_t *vardata);

// register the variable where the result will be stored in
static char * ngx_http_php_session_register_result_variable(ngx_int_t *index, ngx_conf_t *cf, ngx_http_get_variable_pt getter);

// finding a variable by its index
static ngx_int_t ngx_http_php_session_find_variable_from_array(ngx_array_t *array, ngx_http_php_session_value_t **value, uintptr_t data);

// directive handlers
static char * ngx_http_php_session_parse_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_php_session_strip_formatting_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

// variable getters
static ngx_int_t ngx_http_php_session_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_php_session_strip_formatting_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

    

static ngx_command_t  ngx_http_php_session_commands[] = {
 { ngx_string("php_session_parse"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
    ngx_http_php_session_parse_directive, //ngx_conf_set_keyval_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL},
 { ngx_string("php_session_strip_formatting"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
    ngx_http_php_session_strip_formatting_directive,
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

    pscf->searches = NULL;
    pscf->extractions = NULL;
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
ngx_http_php_session_compile_variable(ngx_http_script_compile_t *sc, ngx_conf_t *cf, ngx_http_php_session_variable_data_t *vardata)
{
    ngx_memzero(sc, sizeof(ngx_http_script_compile_t));

    sc->cf = cf;
    sc->source = &vardata->value;
    sc->lengths = &vardata->lengths;
    sc->values = &vardata->values;
    *sc->lengths = NULL;
    *sc->values = NULL;
    sc->variables = ngx_http_script_variables_count(&vardata->value);
    sc->complete_lengths = 1;
    sc->complete_values = 1;
    
    if (ngx_http_script_compile(sc) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static char *
ngx_http_php_session_register_result_variable(ngx_int_t *index, ngx_conf_t *cf, ngx_http_get_variable_pt getter)
{
    ngx_str_t              *value;
    ngx_http_variable_t    *v;

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
    
    *index = ngx_http_get_variable_index(cf, &value[1]);
    if (*index == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }
    
    if (v->get_handler == NULL)
    {
        v->get_handler = *getter;
        v->data = *index;
    }
    
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_php_session_find_variable_from_array(ngx_array_t *array, ngx_http_php_session_value_t **value, uintptr_t data)
{
    ngx_uint_t                          i;
    ngx_http_php_session_value_t       *tmp_value;
    u_char                             *tmp_void;

    tmp_void = (u_char*) array->elts;

    for (i = 0; i < array->nelts; i++) {
        tmp_value = (ngx_http_php_session_value_t*) tmp_void;
        if (tmp_value->result_index == data) {
            *value = tmp_value;
            break;
        }
        tmp_void += array->size;
    }
    if (i == array->nelts)
    {
        return NGX_ERROR;
    }
    return NGX_OK;
}

static char *
ngx_http_php_session_parse_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_script_compile_t           sc_session, sc_searchkey;
    ngx_http_php_session_loc_conf_t    *pscf = conf;
    ngx_http_php_session_search_t      *search;
    ngx_int_t                           index;
    ngx_str_t                          *value;

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
    
    if (ngx_http_php_session_register_result_variable(&index, cf, ngx_http_php_session_variable) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    search->result_index = index;

    value = cf->args->elts;

    search->session.value.len = value[2].len;
    search->session.value.data = value[2].data;
    search->searchkey.value.len = value[3].len;
    search->searchkey.value.data = value[3].data;
    

    if (ngx_http_php_session_compile_variable(&sc_session, cf, &search->session) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (ngx_http_php_session_compile_variable(&sc_searchkey, cf, &search->searchkey) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}

static char *
ngx_http_php_session_strip_formatting_directive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_php_session_loc_conf_t        *pscf = conf;
    ngx_http_php_session_value_extraction_t     *extraction;
    ngx_http_script_compile_t               sc_extraction;
    ngx_int_t                               index;
    ngx_str_t                              *value;
    
    if (pscf->extractions == NULL)
    {
        pscf->extractions = ngx_array_create(cf->pool, cf->args->nelts, sizeof(ngx_http_php_session_value_extraction_t));
        if(pscf->extractions == NULL) {
            return NGX_CONF_ERROR;
        }
    }
    
    extraction = ngx_array_push(pscf->extractions);
    if (extraction == NULL) {
        return NGX_CONF_ERROR;
    }

    if (ngx_http_php_session_register_result_variable(&index, cf, ngx_http_php_session_strip_formatting_variable) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    extraction->result_index = index;

    value = cf->args->elts;

    extraction->value.value.len = value[2].len;
    extraction->value.value.data = value[2].data;

    if (ngx_http_php_session_compile_variable(&sc_extraction, cf, &extraction->value) != NGX_CONF_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_php_session_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{    
    ngx_http_php_session_loc_conf_t     *pscf = ngx_http_get_module_loc_conf(r, ngx_http_php_session_module);
    ngx_http_php_session_search_t       *search = NULL;

    if (ngx_http_php_session_find_variable_from_array(pscf->searches, (ngx_http_php_session_value_t**) &search, data) != NGX_OK)
    {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "error finding the right extraction variable");
        return NGX_ERROR;
    }
    
    if (ngx_http_script_run(r, &search->session.value, search->session.lengths->elts, 0 , search->session.values->elts) == NULL) {
        
        return NGX_ERROR;
    }
    
    if (ngx_http_script_run(r, &search->searchkey.value, search->searchkey.lengths->elts, 0 , search->searchkey.values->elts) == NULL) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "searchkey evaluation failed");
        return NGX_ERROR;
    }
    
    if (extract_value(&search->searchkey.value, &search->session.value, &search->result_string) != NGX_OK) {
        return NGX_ERROR;
    }
    
    v->len = search->result_string.len;
    v->data = search->result_string.data;
    v->valid = 1;
    
    return NGX_OK;
}

static ngx_int_t
ngx_http_php_session_strip_formatting_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_php_session_loc_conf_t     *pscf = ngx_http_get_module_loc_conf(r, ngx_http_php_session_module);
    ngx_http_php_session_value_extraction_t   *extraction;

    if (ngx_http_php_session_find_variable_from_array(pscf->extractions, (ngx_http_php_session_value_t**) &extraction, data) != NGX_OK)
    {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "error finding the right extraction variable");
        return NGX_ERROR;
    }
    
    if (ngx_http_script_run(r, &extraction->value.value, extraction->value.lengths->elts, 0 , extraction->value.values->elts) == NULL) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "extraction value evaluation failed");
        return NGX_ERROR;
    }

    if (value_strip_format(&extraction->value.value, &extraction->result_string) != NGX_OK) {
    	return NGX_ERROR;
    }
    
    v->len = extraction->result_string.len;
    v->data = extraction->result_string.data;
    v->valid = 1;
    
    return NGX_OK;

}
