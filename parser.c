
#include <parser.h>

static ngx_int_t
extract_value(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result)
{
    
    if (extract_value_next_level(searchkey, session, result) == NO_RESULT){
        result->data = NULL;
        result->len = 0;
    }
    
    return NGX_OK;
}

static ngx_int_t
extract_value_next_level(ngx_str_t *searchkey, ngx_str_t *session, ngx_str_t *result)
{
    ngx_int_t    next_search_element;
    ngx_int_t    next_session_element;
    
    ngx_str_t    sub_session;
    ngx_str_t    sub_searchkey;
    
    ngx_str_t    session_element;
    ngx_str_t    search_element;
    
    session_element.data = NULL;
    search_element.data = NULL;
    
    next_session_element = NEXT_ELEMENT_OK;
    next_search_element = next_element(&search_element, searchkey);
    while (next_session_element == NEXT_ELEMENT_OK) {
        next_session_element = next_element(&session_element, session);
        if (ngx_strncmp(search_element.data, session_element.data, search_element.len) == 0) {
            next_search_element = next_element(&search_element, searchkey);
            next_session_element = next_element(&session_element, session);
            if (next_search_element == NO_NEXT_ELEMENT) {
                if (store_result(&session_element, result) == NGX_OK) {
                    return GOT_RESULT;
                } else {
                    return NO_RESULT;
                }
            } else {
                if (*session_element.data != 'a') {
                    printf("not enough nested levels in session_data\n");
                    return NO_RESULT;
                }
                while (*session_element.data != '{') {
                    session_element.data++;
                    session_element.len--;
                    if (session_element.len == 0) {
                        return NO_RESULT;
                    }
                }
                session_element.data++;
                session_element.len--;
                session_element.len--; // cut off } in the end of the string
                sub_session.data = session_element.data;
                sub_session.len = session_element.len;
                sub_searchkey.data = search_element.data;
                sub_searchkey.len = searchkey->data + searchkey->len - search_element.data;
                if (extract_value_next_level(&sub_searchkey, &sub_session, result) == GOT_RESULT){
                    return GOT_RESULT;
                }
            }
        }
        next_session_element = next_element(&session_element, session);
    }
    
    return NO_RESULT;
}

static ngx_int_t
store_result(ngx_str_t *session_element, ngx_str_t *result)
{
    result->data = session_element->data;
    result->len = session_element->len + 1;
    
    return NGX_OK;
}

static ngx_int_t
next_element(ngx_str_t *data_element, ngx_str_t *data)
{
    u_char              *data_pos;
    u_char              *data_element_start = NULL;
    u_char              *data_element_end = NULL;
    ngx_int_t            nested_level = 0;
    

    if (data_element->data == NULL) {
        data_element->data = data->data;
        data_element->len = 0;
        data_element_start = data->data;
    }
    
    data_pos = data_element->data + data_element->len;
    while (is_element_separator(data_pos+1) == ELEMENT_IS_SEPARATOR && data_pos < data->data + data->len) {
        data_pos++;
    }
    if (data_pos >= data->data + data->len) {
        return NO_NEXT_ELEMENT;
    }
    while (data_element_end == NULL && data_pos < data->data + data->len) {
        if (*data_pos == '{') {
            nested_level++;
        }
        if (*data_pos == '}') {
            nested_level--;
        }
        if (nested_level > 0) {
            data_pos++;
            continue;
        }
        if (is_element_separator(data_pos) == ELEMENT_IS_SEPARATOR) {
            if (data_element_start == 0) {
                data_element_start = data_pos + 1;
            } else {
                data_element_end = data_pos - 1;
            }
         }
         data_pos++;
    }
    if (data_element_end == NULL && data_pos == data->data + data->len) {
        data_element_end = data_pos;
    }
    if (data_element_start != NULL) {
        while (data_element_start <= data_element->data + data_element->len && *data_element_start == '}') {
            data_element_start++;
        }
    }
    data_element->data = data_element_start;
    data_element->len = data_element_end - data_element_start;
    return NEXT_ELEMENT_OK;
}

static ngx_int_t
is_element_separator(u_char *pos)
{
    ngx_uint_t separators_pos;
    for (separators_pos = 0; separators_pos < element_separators.len; separators_pos++) {
        if (*(element_separators.data + separators_pos) == *pos) {
            return ELEMENT_IS_SEPARATOR;
        }
    }
    return ELEMENT_IS_NOT_SEPARATOR;
}
