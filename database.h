#pragma once
#include <stdint.h>

static const char * const JOINED_TABLE_NAME = "joined table";

enum database_column_type {
    STORAGE_COLUMN_TYPE_INT = 0,
    STORAGE_COLUMN_TYPE_UINT = 1,
    STORAGE_COLUMN_TYPE_NUM = 2,
    STORAGE_COLUMN_TYPE_STR = 3,
};

struct database {
    int fd;
    uint64_t first_table;
};

struct database_column {
    char * name;
    enum database_column_type type;
};

struct database_table {
    struct database * storage;

    uint64_t position;
    uint64_t next;

    uint64_t first_row;
    char * name;

    struct {
        uint16_t amount;
        struct database_column * columns;
    } columns;
};

struct database_row {
    struct database_table * table;

    uint64_t position;
    uint64_t next;
};

struct database_value {
    enum database_column_type type;

    union {
        int64_t _int;
        uint64_t uint;
        double num;
        char * str;
    } value;
};

struct database_joined_table {
    struct {
        unsigned int amount;
        struct {
            struct database_table * table;
            uint16_t t_column_index;
            uint16_t s_column_index;
        } * tables;
    } tables;
};

struct database_joined_row {
    struct database_joined_table * table;
    struct database_row ** rows;
};

struct database * database_init(int fd);
struct database * database_open(int fd);
void delete_database(struct database * storage);

struct database_table * database_find_table(struct database * storage, const char * name);

void database_table_delete(struct database_table * table);

void database_table_add(struct database_table * table);
void database_table_remove(struct database_table * table);
struct database_row * database_table_get_first_row(struct database_table * table);
struct database_row * database_table_add_row(struct database_table * table);

void database_row_delete(struct database_row * row);

struct database_row * database_row_next(struct database_row * row);
void database_row_remove(struct database_row * row);
struct database_value * database_row_get_value(struct database_row * row, uint16_t index);
void database_row_set_value(struct database_row * row, uint16_t index, struct database_value * value);

void database_value_destroy(struct database_value value);
void database_value_delete(struct database_value * value);

const char * database_column_type_to_string(enum database_column_type type);

struct database_joined_table * database_joined_table_new(unsigned int amount);
struct database_joined_table * database_joined_table_wrap(struct database_table * table);
void database_joined_table_delete(struct database_joined_table * table);

uint16_t database_joined_table_get_columns_amount(struct database_joined_table * table);
struct database_column database_joined_table_get_column(struct database_joined_table * table, uint16_t index);
struct database_joined_row * database_joined_table_get_first_row(struct database_joined_table * table);

void database_joined_row_delete(struct database_joined_row * row);

struct database_joined_row * database_joined_row_next(struct database_joined_row * row);
struct database_value * database_joined_row_get_value(struct database_joined_row * row, uint16_t index);
