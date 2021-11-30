#pragma once

#include <json-c/json.h>
#include "database.h"

enum json_api_action {
    JSON_API_TYPE_CREATE_TABLE = 0,
    JSON_API_TYPE_DROP_TABLE = 1,
    JSON_API_TYPE_INSERT = 2,
    JSON_API_TYPE_DELETE = 3,
    JSON_API_TYPE_SELECT = 4,
    JSON_API_TYPE_UPDATE = 5,
};

struct json_api_create_table_request {
    char * table_name;
    struct {
        unsigned int amount;
        struct {
            char * name;
            enum database_column_type type;
        } * columns;
    } columns;
};

struct json_api_drop_table_request {
    char * table_name;
};

struct json_api_insert_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct {
        unsigned int amount;
        struct database_value ** values;
    } values;
};

enum json_api_operator {
    JSON_API_OPERATOR_EQ = 0,
    JSON_API_OPERATOR_NE = 1,
    JSON_API_OPERATOR_LT = 2,
    JSON_API_OPERATOR_GT = 3,
    JSON_API_OPERATOR_LE = 4,
    JSON_API_OPERATOR_GE = 5,
    JSON_API_OPERATOR_AND = 6,
    JSON_API_OPERATOR_OR = 7,
};

struct json_api_where {
    enum json_api_operator op;

    union {
        struct {
            char * column;
            struct database_value * value;
        };

        struct {
            struct json_api_where * left;
            struct json_api_where * right;
        };
    };
};

struct json_api_delete_request {
    char * table_name;
    struct json_api_where * where;
};

struct json_api_select_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct json_api_where * where;
    unsigned int offset;
    unsigned int limit;
    struct {
        unsigned int amount;
        struct {
            char * table;
            char * t_column;
            char * s_column;
        } * joins;
    } joins;
};

struct json_api_update_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct {
        unsigned int amount;
        struct database_value ** values;
    } values;
    struct json_api_where * where;
};

enum json_api_action json_api_get_action(struct json_object * object);

struct json_api_create_table_request json_api_to_create_table_request(struct json_object * object);
struct json_api_drop_table_request json_api_to_drop_table_request(struct json_object * object);
struct json_api_insert_request json_api_to_insert_request(struct json_object * object);
struct json_api_delete_request json_api_to_delete_request(struct json_object * object);
struct json_api_select_request json_api_to_select_request(struct json_object * object);
struct json_api_update_request json_api_to_update_request(struct json_object * object);

struct json_object * json_api_make_success(struct json_object * answer);
struct json_object * json_api_make_error(const char * msg);

struct json_object * json_api_from_value(struct database_value * value);
