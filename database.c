#define _LARGEFILE64_SOURCE

#include "database.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define SIGNATURE ("\xDE\xAD\xBA\xBE")

struct database * database_init(int fd) {
    lseek64(fd, 0, SEEK_SET);

    write(fd, SIGNATURE, 4);

    uint64_t p = 0;
    write(fd, &p, sizeof(p));

    struct database * storage = malloc(sizeof(*storage));

    storage->fd = fd;
    storage->first_table = 0;
    return storage;
}

static char * database_read_string(int fd) {
    uint16_t length;

    read(fd, &length, sizeof(length));

    char * str = malloc(sizeof(int8_t) * (length + 1));
    read(fd, str, length);
    str[length] = '\0';

    return str;
}

struct database * database_open(int fd) {
    lseek64(fd, 0, SEEK_SET);

    char sign[4];
    if (read(fd, sign, 4) != 4) {
        errno = EINVAL;
        return NULL;
    }

    if (memcmp(sign, SIGNATURE, 4) != 0) {
        errno = EINVAL;
        return NULL;
    }

    struct database * storage = malloc(sizeof(*storage));
    storage->fd = fd;

    read(fd, &storage->first_table, sizeof(storage->first_table));
    return storage;
}


void delete_database(struct database * storage) {
    free(storage);
}

void database_table_delete(struct database_table * table) {
    if (table) {
        free(table->name);

        for (uint16_t i = 0; i < table->columns.amount; ++i) {
            free(table->columns.columns[i].name);
        }

        free(table->columns.columns);
    }

    free(table);
}

struct database_table * database_find_table(struct database * storage, const char * name) {
    uint64_t pointer = storage->first_table;

    while (pointer) {
        lseek64(storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next, first_row;
        read(storage->fd, &next, sizeof(next));
        read(storage->fd, &first_row, sizeof(first_row));

        char * table_name = database_read_string(storage->fd);
        if (strcmp(table_name, name) != 0) {
            free(table_name);
            pointer = next;
            continue;
        }

        struct database_table * table = malloc(sizeof(*table));
        table->storage = storage;
        table->position = pointer;
        table->next = next;
        table->first_row = first_row;
        table->name = table_name;

        read(storage->fd, &table->columns.amount, sizeof(table->columns.amount));
        table->columns.columns = malloc(sizeof(*table->columns.columns) * table->columns.amount);

        for (uint16_t i = 0; i < table->columns.amount; ++i) {
            table->columns.columns[i].name = database_read_string(storage->fd);

            uint8_t type;
            read(storage->fd, &type, sizeof(type));
            table->columns.columns[i].type = (enum database_column_type) type;
        }

        return table;
    }

    return NULL;
}

static uint64_t database_write(int fd, void * buf, size_t length) {
    uint64_t offset = lseek64(fd, 0, SEEK_END);
    write(fd, buf, length);
    return offset;
}

static uint64_t database_write_string(int fd, const char * str) {
    uint16_t length = strlen(str);

    uint64_t ret = database_write(fd, &length, sizeof(length));
    write(fd, str, length);
    return ret;
}

void database_table_add(struct database_table * table) {
    struct database_table * another_table = database_find_table(table->storage, table->name);

    if (another_table != NULL) {
        database_table_delete(another_table);
        errno = EINVAL;
        return;
    }

    table->next = table->storage->first_table;
    table->position = database_write(table->storage->fd, &table->next, sizeof(table->next));
    table->storage->first_table = table->position;

    write(table->storage->fd, &table->first_row, sizeof(table->first_row));
    database_write_string(table->storage->fd, table->name);
    write(table->storage->fd, &table->columns.amount, sizeof(table->columns.amount));

    for (uint16_t i = 0; i < table->columns.amount; ++i) {
        database_write_string(table->storage->fd, table->columns.columns[i].name);

        uint8_t type = table->columns.columns[i].type;
        write(table->storage->fd, &type, sizeof(type));
    }

    lseek64(table->storage->fd, 4, SEEK_SET);
    write(table->storage->fd, &table->position, sizeof(table->position));
}

void database_table_remove(struct database_table * table) {
    uint64_t pointer = table->storage->first_table;

    while (pointer) {
        lseek64(table->storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next;
        read(table->storage->fd, &next, sizeof(next));

        if (next == table->position) {
            break;
        }

        pointer = next;
    }

    if (pointer == 0) {
        pointer = 4;
        table->storage->first_table = table->next;
    }

    lseek64(table->storage->fd, (off64_t) pointer, SEEK_SET);
    write(table->storage->fd, &table->next, sizeof(table->next));
}

struct database_row * database_table_add_row(struct database_table * table) {
    struct database_row * row = malloc(sizeof(*row));

    row->table = table;
    row->next = table->first_row;
    row->position = database_write(table->storage->fd, &row->next, sizeof(row->next));
    table->first_row = row->position;

    uint64_t null = 0;
    for (uint16_t i = 0; i < table->columns.amount; ++i) {
        write(table->storage->fd, &null, sizeof(null));
    }

    lseek64(table->storage->fd, (off64_t) (table->position + sizeof(uint64_t)), SEEK_SET);
    write(table->storage->fd, &table->first_row, sizeof(table->first_row));
    return row;
}

struct database_row * database_table_get_first_row(struct database_table * table) {
    if (table->first_row == 0) {
        return NULL;
    }

    struct database_row * row = malloc(sizeof(*row));
    row->position = table->first_row;
    row->table = table;

    lseek64(table->storage->fd, (off64_t) row->position, SEEK_SET);
    read(table->storage->fd, &row->next, sizeof(row->next));

    return row;
}

struct database_row * database_row_next(struct database_row * row) {
    row->position = row->next;

    if (row->next == 0) {
        free(row);
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) row->position, SEEK_SET);
    read(row->table->storage->fd, &row->next, sizeof(row->next));
    return row;
}


void database_row_delete(struct database_row * row) {
    free(row);
}

void database_row_remove(struct database_row * row) {
    uint64_t pointer = row->table->first_row;

    while (pointer) {
        lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next;
        read(row->table->storage->fd, &next, sizeof(next));

        if (next == row->position) {
            break;
        }

        pointer = next;
    }

    if (pointer == 0) {
        pointer = row->table->position + sizeof(uint64_t);
        row->table->first_row = row->next;
    }

    lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);
    write(row->table->storage->fd, &row->next, sizeof(row->next));
}

void database_row_set_value(struct database_row * row, uint16_t index, struct database_value * value) {
    if (index >= row->table->columns.amount) {
        errno = EINVAL;
        return;
    }

    uint64_t pointer = 0;

    if (value) {
        if (row->table->columns.columns[index].type != value->type) {
            errno = EINVAL;
            return;
        }

        switch (value->type) {
            case STORAGE_COLUMN_TYPE_INT:
                pointer = database_write(row->table->storage->fd, &value->value._int, sizeof(value->value._int));
                break;

            case STORAGE_COLUMN_TYPE_UINT:
                pointer = database_write(row->table->storage->fd, &value->value.uint, sizeof(value->value.uint));
                break;

            case STORAGE_COLUMN_TYPE_NUM:
                pointer = database_write(row->table->storage->fd, &value->value.num, sizeof(value->value.num));
                break;

            case STORAGE_COLUMN_TYPE_STR:
                pointer = database_write_string(row->table->storage->fd, value->value.str);
                break;
        }
    }

    lseek64(row->table->storage->fd, (off64_t) (row->position + (1 + index) * sizeof(uint64_t)), SEEK_SET);
    write(row->table->storage->fd, &pointer, sizeof(pointer));
}

struct database_value * database_row_get_value(struct database_row * row, uint16_t index) {
    if (index >= row->table->columns.amount) {
        errno = EINVAL;
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) (row->position + (1 + index) * sizeof(uint64_t)), SEEK_SET);

    uint64_t pointer;
    read(row->table->storage->fd, &pointer, sizeof(pointer));

    if (pointer == 0) {
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);

    struct database_value * value = malloc(sizeof(*value));
    value->type = row->table->columns.columns[index].type;

    switch (value->type) {
        case STORAGE_COLUMN_TYPE_INT:
            read(row->table->storage->fd, &value->value._int, sizeof(value->value._int));
            break;

        case STORAGE_COLUMN_TYPE_UINT:
            read(row->table->storage->fd, &value->value.uint, sizeof(value->value.uint));
            break;

        case STORAGE_COLUMN_TYPE_NUM:
            read(row->table->storage->fd, &value->value.num, sizeof(value->value.num));
            break;

        case STORAGE_COLUMN_TYPE_STR:
            value->value.str = database_read_string(row->table->storage->fd);
            break;
    }

    return value;
}

struct database_joined_table * database_joined_table_new(unsigned int amount) {
    struct database_joined_table * table = malloc(sizeof(*table));

    table->tables.amount = amount;
    table->tables.tables = calloc(amount, sizeof(*table->tables.tables));

    return table;
}

struct database_joined_table * database_joined_table_wrap(struct database_table * table) {
    if (!table) {
        return NULL;
    }

    struct database_joined_table * joined_table = database_joined_table_new(1);
    joined_table->tables.tables[0].table = table;
    joined_table->tables.tables[0].t_column_index = 0;
    joined_table->tables.tables[0].s_column_index = 0;

    return joined_table;
}

const char * database_column_type_to_string(enum database_column_type type) {
    switch (type) {
        case STORAGE_COLUMN_TYPE_INT:
            return "int";

        case STORAGE_COLUMN_TYPE_UINT:
            return "uint";

        case STORAGE_COLUMN_TYPE_NUM:
            return "num";

        case STORAGE_COLUMN_TYPE_STR:
            return "str";

        default:
            return NULL;
    }
}

void database_joined_table_delete(struct database_joined_table * table) {
    if (table) {
        for (int i = 0; i < table->tables.amount; ++i) {
            database_table_delete(table->tables.tables[i].table);
        }

        free(table->tables.tables);
    }

    free(table);
}

struct database_column database_joined_table_get_column(struct database_joined_table * table, uint16_t index) {
    for (int i = 0; i < table->tables.amount; ++i) {
        if (index < table->tables.tables[i].table->columns.amount) {
            return table->tables.tables[i].table->columns.columns[index];
        }

        index -= table->tables.tables[i].table->columns.amount;
    }

    abort();
}

static bool database_value_equals(struct database_value * a, struct database_value * b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }

    switch (a->type) {
        case STORAGE_COLUMN_TYPE_INT:
            switch (b->type) {
                case STORAGE_COLUMN_TYPE_INT:
                    return a->value._int == b->value._int;

                case STORAGE_COLUMN_TYPE_UINT:
                    if (a->value._int < 0) {
                        return false;
                    }

                    return ((uint64_t) a->value._int) == b->value.uint;

                case STORAGE_COLUMN_TYPE_NUM:
                    return ((double) a->value._int) == b->value.num;

                case STORAGE_COLUMN_TYPE_STR:
                    return false;
            }

        case STORAGE_COLUMN_TYPE_UINT:
            switch (b->type) {
                case STORAGE_COLUMN_TYPE_INT:
                    if (b->value._int < 0) {
                        return false;
                    }

                    return a->value.uint == ((uint64_t) b->value._int);

                case STORAGE_COLUMN_TYPE_UINT:
                    return a->value.uint == b->value.uint;

                case STORAGE_COLUMN_TYPE_NUM:
                    return ((double) a->value.uint) == b->value.num;

                case STORAGE_COLUMN_TYPE_STR:
                    return false;
            }

        case STORAGE_COLUMN_TYPE_NUM:
            switch (b->type) {
                case STORAGE_COLUMN_TYPE_INT:
                    return a->value.num == ((double) b->value._int);

                case STORAGE_COLUMN_TYPE_UINT:
                    return a->value.num == ((double) b->value.uint);

                case STORAGE_COLUMN_TYPE_NUM:
                    return a->value.num == b->value.num;

                case STORAGE_COLUMN_TYPE_STR:
                    return false;
            }

        case STORAGE_COLUMN_TYPE_STR:
            switch (b->type) {
                case STORAGE_COLUMN_TYPE_INT:
                case STORAGE_COLUMN_TYPE_UINT:
                case STORAGE_COLUMN_TYPE_NUM:
                    return false;

                case STORAGE_COLUMN_TYPE_STR:
                    return strcmp(a->value.str, b->value.str) == 0;
            }
    }
}

uint16_t database_joined_table_get_columns_amount(struct database_joined_table * table) {
    uint16_t amount = 0;

    for (int i = 0; i < table->tables.amount; ++i) {
        amount += table->tables.tables[i].table->columns.amount;
    }

    return amount;
}

static bool database_joined_row(struct database_joined_row * row, uint16_t index) {
    return database_value_equals(
            database_joined_row_get_value(row, row->table->tables.tables[index].s_column_index),
            database_row_get_value(row->rows[index], row->table->tables.tables[index].t_column_index)
    );
}

static void database_joined_row_roll(struct database_joined_row * row) {
    for (int i = 1; i < row->table->tables.amount; ++i) {
        if (!database_joined_row(row, i)) {
            row->rows[i] = database_row_next(row->rows[i]);

            for (int j = i + 1; j < row->table->tables.amount; ++j) {
                database_row_delete(row->rows[j]);
                row->rows[j] = database_table_get_first_row(row->table->tables.tables[j].table);
            }

            for (int j = i; j > 0; --j) {
                if (row->rows[j] == NULL) {
                    row->rows[j] = database_table_get_first_row(row->table->tables.tables[j].table);
                    row->rows[j - 1] = database_row_next(row->rows[j - 1]);
                }
            }

            if (row->rows[0] == NULL) {
                return;
            }

            i = 0;
        }
    }
}

void database_joined_row_delete(struct database_joined_row * row) {
    if (row) {
        for (int i = 0; i < row->table->tables.amount; ++i) {
            database_row_delete(row->rows[i]);
        }

        free(row->rows);
    }

    free(row);
}

struct database_joined_row * database_joined_row_next(struct database_joined_row * row) {
    uint16_t last_index = row->table->tables.amount - 1;

    row->rows[last_index] = database_row_next(row->rows[last_index]);
    for (int i = (int) last_index; i > 0; --i) {
        if (row->rows[i] == NULL) {
            row->rows[i] = database_table_get_first_row(row->table->tables.tables[i].table);
            row->rows[i - 1] = database_row_next(row->rows[i - 1]);
        }
    }

    if (row->rows[0] == NULL) {
        database_joined_row_delete(row);
        return NULL;
    }

    database_joined_row_roll(row);
    if (row->rows[0] == NULL) {
        database_joined_row_delete(row);
        return NULL;
    }

    return row;
}

struct database_joined_row * database_joined_table_get_first_row(struct database_joined_table * table) {
    struct database_joined_row * row = malloc(sizeof(*row));

    row->table = table;
    row->rows = malloc(sizeof(struct database_row *) * table->tables.amount);
    for (int i = 0; i < table->tables.amount; ++i) {
        row->rows[i] = NULL;
    }

    for (int i = 0; i < table->tables.amount; ++i) {
        row->rows[i] = database_table_get_first_row(table->tables.tables[i].table);

        if (row->rows[i] == NULL) {
            database_joined_row_delete(row);
            return NULL;
        }
    }

    database_joined_row_roll(row);
    if (row->rows[0] == NULL) {
        database_joined_row_delete(row);
        return NULL;
    }

    return row;
}

struct database_value * database_joined_row_get_value(struct database_joined_row * row, uint16_t index) {
    for (int i = 0; i < row->table->tables.amount; ++i) {
        if (index < row->table->tables.tables[i].table->columns.amount) {
            return database_row_get_value(row->rows[i], index);
        }

        index -= row->table->tables.tables[i].table->columns.amount;
    }

    return NULL;
}
