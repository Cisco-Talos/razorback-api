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

#ifndef TEST_JSON_BUFFER_SUPPORT_H
#define TEST_JSON_BUFFER_SUPPORT_H

#include <json.h>

json_object *json_buffer_test_parent_object(void);
void json_buffer_assert_matches_schema(const char *schema_name,
                                       json_object *instance);
void json_buffer_assert_field_matches_schema(json_object *parent,
                                             const char *field_name,
                                             const char *schema_name);

#endif
