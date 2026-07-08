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

#include <razorback/config_next.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <yaml.h>

extern char **environ;

struct ConfigBinding
{
    const struct RzbNextConfigKey *key;
    char *ownedString;
};

static bool
RzbNextConfig_Strndup(const char *value, size_t length, char **copy)
{
    char *allocated;

    if (value == NULL || copy == NULL)
        return false;
    allocated = malloc(length + 1);
    if (allocated == NULL)
        return false;
    memcpy(allocated, value, length);
    allocated[length] = '\0';
    *copy = allocated;
    return true;
}

static bool
RzbNextConfig_KeySegmentValid(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0')
        return false;
    if (!islower((unsigned char)value[0]) && !isdigit((unsigned char)value[0]))
        return false;
    for (index = 1; value[index] != '\0'; index++) {
        if (!islower((unsigned char)value[index]) &&
            !isdigit((unsigned char)value[index]) &&
            value[index] != '_') {
            return false;
        }
    }
    return true;
}

static bool
RzbNextConfig_KeyValid(const char *key)
{
    char *segment;
    char *cursor;
    char *copy;
    bool valid = true;

    if (key == NULL || key[0] == '\0')
        return false;
    copy = strdup(key);
    if (copy == NULL)
        return false;
    segment = copy;
    for (cursor = copy; ; cursor++) {
        if (*cursor == '.' || *cursor == '\0') {
            char saved = *cursor;

            *cursor = '\0';
            if (!RzbNextConfig_KeySegmentValid(segment))
                valid = false;
            *cursor = saved;
            if (saved == '\0')
                break;
            segment = cursor + 1;
        }
    }
    free(copy);
    return valid;
}

static bool
RzbNextConfig_FileExists(const char *path)
{
    struct stat statbuf;

    return path != NULL && path[0] != '\0' && stat(path, &statbuf) == 0 &&
           S_ISREG(statbuf.st_mode);
}

static size_t
RzbNextConfig_KeyCount(const struct RzbNextConfigKey *keys)
{
    size_t count = 0;

    if (keys == NULL)
        return 0;
    while (keys[count].key != NULL)
        count++;
    return count;
}

static bool
RzbNextConfig_ValidateBindings(struct ConfigBinding *bindings, size_t count)
{
    size_t index;
    size_t other;

    if (bindings == NULL || count == 0)
        return false;
    for (index = 0; index < count; index++) {
        if (bindings[index].key == NULL ||
            bindings[index].key->dest == NULL ||
            !RzbNextConfig_KeyValid(bindings[index].key->key)) {
            return false;
        }
        for (other = index + 1; other < count; other++) {
            if (strcmp(bindings[index].key->key, bindings[other].key->key) == 0)
                return false;
        }
    }
    return true;
}

static struct ConfigBinding *
RzbNextConfig_FindBinding(struct ConfigBinding *bindings, size_t count,
                          const char *key)
{
    size_t index;

    for (index = 0; index < count; index++) {
        if (strcmp(bindings[index].key->key, key) == 0)
            return &bindings[index];
    }
    return NULL;
}

static bool
RzbNextConfig_HasPrefixBinding(struct ConfigBinding *bindings, size_t count,
                               const char *prefix)
{
    size_t index;
    size_t length;

    length = strlen(prefix);
    for (index = 0; index < count; index++) {
        if (strncmp(bindings[index].key->key, prefix, length) == 0 &&
            bindings[index].key->key[length] == '.') {
            return true;
        }
    }
    return false;
}

static bool
RzbNextConfig_IsOwnedSection(struct ConfigBinding *bindings, size_t count,
                             const char *key)
{
    size_t index;
    size_t sectionLength;
    const char *dot;

    dot = strchr(key, '.');
    sectionLength = dot == NULL ? strlen(key) : (size_t)(dot - key);
    for (index = 0; index < count; index++) {
        if (strncmp(bindings[index].key->key, key, sectionLength) == 0 &&
            (bindings[index].key->key[sectionLength] == '.' ||
             bindings[index].key->key[sectionLength] == '\0')) {
            return true;
        }
    }
    return false;
}

static bool
RzbNextConfig_ParseBool(const char *value, bool *parsed)
{
    if (value == NULL || parsed == NULL)
        return false;
    if (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
        *parsed = true;
        return true;
    }
    if (strcasecmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
        strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) {
        *parsed = false;
        return true;
    }
    return false;
}

static bool
RzbNextConfig_ApplyValue(struct ConfigBinding *binding, const char *value)
{
    long parsedInt;
    char *end = NULL;
    char *copy;
    bool parsedBool;

    if (binding == NULL || value == NULL)
        return false;
    switch (binding->key->type) {
    case RZB_NEXT_CONFIG_STRING:
        copy = strdup(value);
        if (copy == NULL)
            return false;
        free(binding->ownedString);
        binding->ownedString = copy;
        *(char **)binding->key->dest = copy;
        return true;

    case RZB_NEXT_CONFIG_INT:
        errno = 0;
        parsedInt = strtol(value, &end, 10);
        while (end != NULL && isspace((unsigned char)*end))
            end++;
        if (errno != 0 || end == value || (end != NULL && *end != '\0') ||
            parsedInt < INT_MIN || parsedInt > INT_MAX) {
            return false;
        }
        *(int *)binding->key->dest = (int)parsedInt;
        return true;

    case RZB_NEXT_CONFIG_BOOL:
        if (!RzbNextConfig_ParseBool(value, &parsedBool))
            return false;
        *(bool *)binding->key->dest = parsedBool;
        return true;
    }
    return false;
}

static char *
RzbNextConfig_JoinKey(const char *prefix, const char *name)
{
    char *joined;
    size_t length;

    if (name == NULL || name[0] == '\0')
        return NULL;
    if (prefix == NULL || prefix[0] == '\0')
        return strdup(name);
    length = strlen(prefix) + 1 + strlen(name) + 1;
    joined = malloc(length);
    if (joined == NULL)
        return NULL;
    snprintf(joined, length, "%s.%s", prefix, name);
    return joined;
}

static bool
RzbNextConfig_LoadMapping(yaml_document_t *document, yaml_node_t *node,
                          const char *prefix,
                          struct ConfigBinding *bindings, size_t count)
{
    yaml_node_pair_t *pair;

    if (node == NULL || node->type != YAML_MAPPING_NODE)
        return false;
    for (pair = node->data.mapping.pairs.start;
         pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *keyNode = yaml_document_get_node(document, pair->key);
        yaml_node_t *valueNode = yaml_document_get_node(document, pair->value);
        const char *keyText;
        char *fullKey;
        bool result = false;

        if (keyNode == NULL || keyNode->type != YAML_SCALAR_NODE ||
            valueNode == NULL) {
            return false;
        }
        keyText = (const char *)keyNode->data.scalar.value;
        if (!RzbNextConfig_KeySegmentValid(keyText))
            return false;
        fullKey = RzbNextConfig_JoinKey(prefix, keyText);
        if (fullKey == NULL)
            return false;

        if (valueNode->type == YAML_MAPPING_NODE) {
            if (RzbNextConfig_HasPrefixBinding(bindings, count, fullKey)) {
                result = RzbNextConfig_LoadMapping(document, valueNode, fullKey,
                                                   bindings, count);
            }
        } else if (valueNode->type == YAML_SCALAR_NODE) {
            struct ConfigBinding *binding;

            binding = RzbNextConfig_FindBinding(bindings, count, fullKey);
            if (binding != NULL) {
                result = RzbNextConfig_ApplyValue(
                    binding,
                    (const char *)valueNode->data.scalar.value
                );
            }
        }
        free(fullKey);
        if (!result)
            return false;
    }
    return true;
}

static bool
RzbNextConfig_LoadYamlFile(const char *path, bool required,
                           struct ConfigBinding *bindings, size_t count)
{
    FILE *file;
    yaml_parser_t parser;
    yaml_document_t document;
    yaml_node_t *root;
    bool loaded = false;
    bool result = false;

    if (!RzbNextConfig_FileExists(path))
        return !required;
    file = fopen(path, "rb");
    if (file == NULL)
        return false;
    if (!yaml_parser_initialize(&parser)) {
        fclose(file);
        return false;
    }
    yaml_parser_set_input_file(&parser, file);
    loaded = yaml_parser_load(&parser, &document) != 0;
    yaml_parser_delete(&parser);
    fclose(file);
    if (!loaded)
        return false;

    root = yaml_document_get_root_node(&document);
    if (root != NULL) {
        result = RzbNextConfig_LoadMapping(&document, root, "", bindings,
                                           count);
    }
    yaml_document_delete(&document);
    return result;
}

static char *
RzbNextConfig_EnvToKey(const char *envName, const char *prefix)
{
    char *key;
    size_t prefixLength;
    size_t input;
    size_t output = 0;

    prefixLength = strlen(prefix);
    if (strncmp(envName, prefix, prefixLength) != 0 ||
        envName[prefixLength] != '_') {
        return NULL;
    }
    envName += prefixLength + 1;
    if (envName[0] == '\0')
        return NULL;
    key = calloc(strlen(envName) + 1, sizeof(char));
    if (key == NULL)
        return NULL;

    for (input = 0; envName[input] != '\0'; input++) {
        if (envName[input] == '_' && envName[input + 1] == '_') {
            key[output++] = '.';
            input++;
        } else {
            key[output++] = (char)tolower((unsigned char)envName[input]);
        }
    }
    key[output] = '\0';
    return key;
}

static bool
RzbNextConfig_ApplyEnv(const char *envPrefix, struct ConfigBinding *bindings,
                       size_t count)
{
    char **env;

    for (env = environ; env != NULL && *env != NULL; env++) {
        char *equals = strchr(*env, '=');
        char *envName;
        char *key;
        struct ConfigBinding *binding;
        bool applied;

        if (equals == NULL)
            continue;
        if (!RzbNextConfig_Strndup(*env, (size_t)(equals - *env), &envName))
            return false;
        key = RzbNextConfig_EnvToKey(envName, envPrefix);
        free(envName);
        if (key == NULL)
            continue;
        if (!RzbNextConfig_KeyValid(key)) {
            free(key);
            return false;
        }
        binding = RzbNextConfig_FindBinding(bindings, count, key);
        if (binding == NULL) {
            bool owned = RzbNextConfig_IsOwnedSection(bindings, count, key);

            free(key);
            if (owned)
                return false;
            continue;
        }
        applied = RzbNextConfig_ApplyValue(binding, equals + 1);
        free(key);
        if (!applied)
            return false;
    }
    return true;
}

SO_PUBLIC bool
RzbNextConfig_Load(const char *baseFile, const char *localFile,
                   const char *envPrefix,
                   const struct RzbNextConfigKey *keys)
{
    struct ConfigBinding *bindings;
    size_t count;
    size_t index;
    bool result;

    count = RzbNextConfig_KeyCount(keys);
    if (count == 0)
        return false;
    bindings = calloc(count, sizeof(*bindings));
    if (bindings == NULL)
        return false;
    for (index = 0; index < count; index++)
        bindings[index].key = &keys[index];

    result = RzbNextConfig_ValidateBindings(bindings, count) &&
             RzbNextConfig_LoadYamlFile(baseFile, true, bindings, count) &&
             RzbNextConfig_LoadYamlFile(localFile, false, bindings, count) &&
             RzbNextConfig_ApplyEnv(envPrefix == NULL ? "RZB" : envPrefix,
                                    bindings, count);

    free(bindings);
    return result;
}
