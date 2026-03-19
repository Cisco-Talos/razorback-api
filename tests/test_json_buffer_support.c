/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <check.h>

#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/event.h>
#include <razorback/hash.h>
#include <razorback/judgment.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/nugget.h>

#include "test_json_buffer_support.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

uint32_t
Config_getHashType(void)
{
    return HASH_TYPE_SHA256;
}

void
rzb_log(unsigned level, uint64_t component, const char *fmt, ...)
{
    va_list args;

    (void)level;
    (void)component;
    (void)fmt;

    va_start(args, fmt);
    va_end(args);
}

static void
block_data_cleanup(struct BlockData *data)
{
    if (data == NULL)
        return;
    if (data->file != NULL)
        fclose(data->file);
    free(data->fileName);
    free(data->pointer);
}

void
Block_Destroy(struct Block *block)
{
    if (block == NULL)
        return;

    if (block->pId != NULL)
        BlockId_Destroy(block->pId);
    if (block->pParentId != NULL)
        BlockId_Destroy(block->pParentId);
    Block_Destroy(block->pParentBlock);
    if (block->pMetaDataList != NULL)
        List_Destroy(block->pMetaDataList);
    block_data_cleanup(&block->data);
    free(block);
}

void
EventId_Destroy(struct EventId *eventId)
{
    free(eventId);
}

void
Event_Destroy(struct Event *event)
{
    if (event == NULL)
        return;

    EventId_Destroy(event->pId);
    EventId_Destroy(event->pParentId);
    Event_Destroy(event->pParent);
    Block_Destroy(event->pBlock);
    if (event->pMetaDataList != NULL)
        List_Destroy(event->pMetaDataList);
    free(event);
}

void
Judgment_Destroy(struct Judgment *judgment)
{
    if (judgment == NULL)
        return;

    EventId_Destroy(judgment->pEventId);
    if (judgment->pBlockId != NULL)
        BlockId_Destroy(judgment->pBlockId);
    if (judgment->pMetaDataList != NULL)
        List_Destroy(judgment->pMetaDataList);
    free(judgment->sMessage);
    free(judgment);
}

void
Nugget_Destroy(struct Nugget *nugget)
{
    if (nugget == NULL)
        return;

    free(nugget->sName);
    free(nugget->sLocation);
    free(nugget->sContact);
    free(nugget->sNotes);
    free(nugget);
}

json_object *
json_buffer_test_parent_object(void)
{
    json_object *parent;

    parent = json_object_new_object();
    ck_assert_ptr_ne(parent, NULL);
    return parent;
}

static void
write_instance_file(FILE *stream, const char *jsonText)
{
    ck_assert_ptr_ne(stream, NULL);
    ck_assert_ptr_ne(jsonText, NULL);
    ck_assert_int_ge(fputs(jsonText, stream), 0);
    ck_assert_int_eq(fflush(stream), 0);
}

void
json_buffer_assert_matches_schema(const char *schema_name, json_object *instance)
{
    char *schema_path = NULL;
    char instance_template[] = "/tmp/razorback-json-schema-XXXXXX";
    const char *json_text;
    FILE *instance_stream;
    int instance_fd;
    pid_t child;
    int status;

    ck_assert_ptr_ne(schema_name, NULL);
    ck_assert_ptr_ne(instance, NULL);
    ck_assert_msg(RAZORBACK_JV[0] != '\0',
                  "RAZORBACK_JV is empty; schema validation is unavailable");
    ck_assert_int_ne(asprintf(&schema_path, "%s/%s", RAZORBACK_SCHEMA_DIR,
                              schema_name), -1);

    instance_fd = mkstemp(instance_template);
    ck_assert_int_ne(instance_fd, -1);
    instance_stream = fdopen(instance_fd, "w");
    ck_assert_ptr_ne(instance_stream, NULL);

    json_text = json_object_to_json_string_ext(instance, JSON_C_TO_STRING_PLAIN);
    write_instance_file(instance_stream, json_text);
    ck_assert_int_eq(fclose(instance_stream), 0);

    child = fork();
    ck_assert_int_ne(child, -1);

    if (child == 0) {
        execl(RAZORBACK_JV, RAZORBACK_JV, "-q", "-f", schema_path,
              instance_template, (char *)NULL);
        _exit(127);
    }

    ck_assert_int_eq(waitpid(child, &status, 0), child);
    ck_assert_msg(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                  "schema validation failed for %s against %s",
                  instance_template, schema_path);

    ck_assert_int_eq(unlink(instance_template), 0);
    free(schema_path);
}

void
json_buffer_assert_field_matches_schema(json_object *parent,
                                        const char *field_name,
                                        const char *schema_name)
{
    json_object *field;

    ck_assert_ptr_ne(parent, NULL);
    ck_assert_ptr_ne(field_name, NULL);
    field = json_object_object_get(parent, field_name);
    ck_assert_ptr_ne(field, NULL);
    json_buffer_assert_matches_schema(schema_name, field);
}
