
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include "testdata.h"
#include "parser.c"


int main (void)
{
    ngx_str_t result;
    char *result_string;
    
    if (extract_value(&test_searchkey1, &test_session, &result) == NGX_OK)
    {
        result_string = malloc(sizeof(char) * result.len);
        snprintf(result_string, result.len + 1, "%s", result.data);
        printf("result: '%s'\n", result_string);
    } else {
        printf("no result\n");
    }
    
    /*if (extract_value(&test_searchkey2, &test_session, &result) == NGX_OK)
    {
        result_string = malloc(sizeof(char) * result.len);
        snprintf(result_string, result.len + 1, "%s", result.data);
        printf("result: '%s'\n", result_string);
    } else {
        printf("no result\n");
    }*/
}