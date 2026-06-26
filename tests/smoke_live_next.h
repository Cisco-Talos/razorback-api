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

#ifndef RAZORBACK_TESTS_SMOKE_LIVE_NEXT_H
#define RAZORBACK_TESTS_SMOKE_LIVE_NEXT_H

#include <razorback/messages_next.h>
#include <razorback/runtime_next.h>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <uuid/uuid.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RZB_SMOKE_DEFAULT_RABBITMQ_URL \
    "amqp://razorback:razorback-rabbitmq@localhost:5672/razorback"
#define RZB_SMOKE_DEFAULT_API_URL "http://localhost:5111"
#define RZB_SMOKE_DEFAULT_KEYCLOAK_URL "http://localhost:8080"
#define RZB_SMOKE_DEFAULT_KEYCLOAK_REALM "razorback-dev"
#define RZB_SMOKE_DEFAULT_KEYCLOAK_CLIENT_ID "smoke"
#define RZB_SMOKE_DEFAULT_KEYCLOAK_USERNAME "smoke"
#define RZB_SMOKE_DEFAULT_KEYCLOAK_PASSWORD "razorback-smoke"
#define RZB_SMOKE_DEFAULT_LOCALITY "default"
#define RZB_SMOKE_DEFAULT_SECONDS 25
#define RZB_SMOKE_OPERATIONAL_BLOCK_SIZE 4096ULL
#define RZB_SMOKE_OPERATIONAL_BLOCK_DATA_TYPE "application/pdf"

#if defined(__GNUC__)
#define RZB_SMOKE_UNUSED __attribute__((unused))
#else
#define RZB_SMOKE_UNUSED
#endif

enum RzbSmokeRole
{
    RZB_SMOKE_ROLE_SOURCE = 0,
    RZB_SMOKE_ROLE_INSPECTOR = 1
};

struct RzbSmokeConfig
{
    enum RzbSmokeRole role;
    const char *label;
    const char *envPrefix;
    const char *nuggetUuid;
    const char *nuggetType;
    const char *appType;
    const char *dataType;
    const char *rabbitmqUrl;
    const char *apiUrl;
    const char *locality;
};

struct RzbSmokeHttpResponse
{
    long status;
    char *body;
    size_t bodySize;
};

struct RzbSmokeBroker
{
    amqp_connection_state_t connection;
    amqp_channel_t channel;
    char *helloQueue;
    char *directedQueue;
    char *workQueue;
};

struct RzbSmokeReport
{
    size_t deliveries;
    bool registrationPublished;
    bool livenessPublished;
    bool byePublished;
    bool liveStateSeen;
    bool blockSubmissionPublished;
    bool blockUpdatePublished;
    bool inspectionWorkReceived;
    bool analysisResultPublished;
    bool activeInspectionSeen;
    bool blockSeen;
};

struct RzbSmokeOperation
{
    char *eventId;
    char *updateId;
    char *inspectionId;
    char *sha256;
    const char *dataType;
    unsigned long long size;
    const char *inspectorAppType;
};

static bool RzbSmoke_LiveEnabled(void);
static bool RZB_SMOKE_UNUSED RzbSmoke_RunLive(enum RzbSmokeRole role);
static bool RZB_SMOKE_UNUSED RzbSmoke_RunLiveOperational(void);

static const char *
RzbSmoke_Env(const char *name, const char *fallback)
{
    const char *value = getenv(name);

    if (value == NULL || value[0] == '\0')
        return fallback;
    return value;
}

static bool
RzbSmoke_LiveEnabled(void)
{
    const char *value = getenv("RZB_SMOKE_LIVE");

    return value != NULL &&
           (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
            strcmp(value, "TRUE") == 0 || strcmp(value, "yes") == 0 ||
            strcmp(value, "YES") == 0 || strcmp(value, "on") == 0 ||
            strcmp(value, "ON") == 0);
}

static bool
RzbSmoke_EnvFalse(const char *name)
{
    const char *value = getenv(name);

    return value != NULL &&
           (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 ||
            strcmp(value, "FALSE") == 0 || strcmp(value, "no") == 0 ||
            strcmp(value, "NO") == 0 || strcmp(value, "off") == 0 ||
            strcmp(value, "OFF") == 0);
}

static unsigned long
RzbSmoke_EnvSeconds(const char *name, unsigned long fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || value[0] == '\0')
        return fallback;
    parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0')
        return fallback;
    return parsed;
}

static char *
RzbSmoke_Strdup(const char *value)
{
    char *copy;
    size_t length;

    if (value == NULL)
        return NULL;
    length = strlen(value) + 1U;
    copy = malloc(length);
    if (copy != NULL)
        memcpy(copy, value, length);
    return copy;
}

static char *
RzbSmoke_Format(const char *format, ...)
{
    va_list ap;
    va_list copy;
    int needed;
    char *buffer;

    va_start(ap, format);
    va_copy(copy, ap);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(ap);
        return NULL;
    }
    buffer = calloc((size_t)needed + 1U, sizeof(char));
    if (buffer != NULL)
        vsnprintf(buffer, (size_t)needed + 1U, format, ap);
    va_end(ap);
    return buffer;
}

static char *
RzbSmoke_Uuid(void)
{
    uuid_t uuid;
    char *text = calloc(37U, sizeof(char));

    if (text == NULL)
        return NULL;
    uuid_generate_random(uuid);
    uuid_unparse_lower(uuid, text);
    return text;
}

static char *
RzbSmoke_RandomSha256(void)
{
    char *first = RzbSmoke_Uuid();
    char *second = RzbSmoke_Uuid();
    char *sha = calloc(65U, sizeof(char));
    size_t out = 0;
    const char *parts[2];
    size_t part;

    if (first == NULL || second == NULL || sha == NULL)
        goto cleanup;
    parts[0] = first;
    parts[1] = second;
    for (part = 0; part < 2U; part++) {
        const char *cursor;

        for (cursor = parts[part]; *cursor != '\0' && out < 64U; cursor++) {
            if (*cursor != '-')
                sha[out++] = *cursor;
        }
    }
    if (out != 64U) {
        free(sha);
        sha = NULL;
    }

cleanup:
    free(first);
    free(second);
    return sha;
}

static char *
RzbSmoke_Timestamp(void)
{
    time_t now = time(NULL);
    struct tm tmValue;
    char *text = calloc(25U, sizeof(char));

    if (text == NULL)
        return NULL;
    gmtime_r(&now, &tmValue);
    strftime(text, 25U, "%Y-%m-%dT%H:%M:%S", &tmValue);
    strcat(text, ".000Z");
    return text;
}

static bool
RzbSmoke_OperationInit(const struct RzbSmokeConfig *inspector,
                       struct RzbSmokeOperation *operation)
{
    memset(operation, 0, sizeof(*operation));
    operation->eventId = RzbSmoke_Uuid();
    operation->updateId = RzbSmoke_Uuid();
    operation->sha256 = RzbSmoke_RandomSha256();
    operation->dataType = RZB_SMOKE_OPERATIONAL_BLOCK_DATA_TYPE;
    operation->size = RZB_SMOKE_OPERATIONAL_BLOCK_SIZE;
    operation->inspectorAppType = inspector->appType;
    return operation->eventId != NULL && operation->updateId != NULL &&
           operation->sha256 != NULL;
}

static void
RzbSmoke_OperationClear(struct RzbSmokeOperation *operation)
{
    if (operation == NULL)
        return;
    free(operation->eventId);
    free(operation->updateId);
    free(operation->inspectionId);
    free(operation->sha256);
    memset(operation, 0, sizeof(*operation));
}

static bool
RzbSmoke_WriteOperationReport(const struct RzbSmokeOperation *operation,
                              const char *sdkName)
{
    const char *path = getenv("RZB_SMOKE_OPERATION_REPORT");
    FILE *file;

    if (path == NULL || path[0] == '\0')
        return true;
    file = fopen(path, "w");
    if (file == NULL)
        return false;
    fprintf(file,
            "{\n"
            "  \"block_key\": \"%s:%llu\",\n"
            "  \"data_type\": \"%s\",\n"
            "  \"event_id\": \"%s\",\n"
            "  \"inspector_app_type\": \"%s\",\n",
            operation->sha256, operation->size, operation->dataType,
            operation->eventId, operation->inspectorAppType);
    if (operation->inspectionId != NULL) {
        fprintf(file, "  \"inspection_id\": \"%s\",\n",
                operation->inspectionId);
    }
    fprintf(file,
            "  \"sdk\": \"%s\",\n"
            "  \"sha256\": \"%s\",\n"
            "  \"size\": %llu,\n"
            "  \"update_id\": \"%s\"\n"
            "}\n",
            sdkName, operation->sha256, operation->size,
            operation->updateId);
    if (fclose(file) != 0)
        return false;
    return true;
}

static void
RzbSmoke_DefaultConfig(enum RzbSmokeRole role, struct RzbSmokeConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->role = role;
    config->rabbitmqUrl = RzbSmoke_Env("RZB_SMOKE_RABBITMQ_URL",
                                       RzbSmoke_Env("RZB_RABBITMQ_URL",
                                                    RZB_SMOKE_DEFAULT_RABBITMQ_URL));
    config->apiUrl = RzbSmoke_Env("RZB_SMOKE_API_URL",
                                  RZB_SMOKE_DEFAULT_API_URL);
    config->locality = RzbSmoke_Env("RZB_SMOKE_LOCALITY",
                                    RZB_SMOKE_DEFAULT_LOCALITY);
    if (role == RZB_SMOKE_ROLE_SOURCE) {
        config->label = "source";
        config->envPrefix = "SOURCE";
        config->nuggetUuid = "10000000-0000-4000-8000-000000000001";
        config->nuggetType = "collection";
        config->appType = "smoke_source";
        config->dataType = NULL;
    } else {
        config->label = "inspector";
        config->envPrefix = "INSPECTOR";
        config->nuggetUuid = "10000000-0000-4000-8000-000000000002";
        config->nuggetType = "inspector";
        config->appType = "pdf_inspector";
        config->dataType = "application/pdf";
    }
}

static char *
RzbSmoke_QueryEncode(const char *value, bool keepSlash)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t length = 0;
    char *encoded;
    char *out;
    const unsigned char *in;

    for (in = (const unsigned char *)value; *in != '\0'; in++) {
        if (isalnum(*in) || *in == '-' || *in == '_' || *in == '.' ||
            *in == '~' || (keepSlash && *in == '/')) {
            length++;
        } else {
            length += 3U;
        }
    }
    encoded = calloc(length + 1U, sizeof(char));
    if (encoded == NULL)
        return NULL;
    out = encoded;
    for (in = (const unsigned char *)value; *in != '\0'; in++) {
        if (isalnum(*in) || *in == '-' || *in == '_' || *in == '.' ||
            *in == '~' || (keepSlash && *in == '/')) {
            *out++ = (char)*in;
        } else {
            *out++ = '%';
            *out++ = hex[*in >> 4U];
            *out++ = hex[*in & 0x0FU];
        }
    }
    return encoded;
}

static size_t
RzbSmoke_CurlWrite(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t bytes = size * nmemb;
    struct RzbSmokeHttpResponse *response = userp;
    char *next = realloc(response->body, response->bodySize + bytes + 1U);

    if (next == NULL)
        return 0;
    response->body = next;
    memcpy(response->body + response->bodySize, contents, bytes);
    response->bodySize += bytes;
    response->body[response->bodySize] = '\0';
    return bytes;
}

static void
RzbSmoke_HttpResponseClear(struct RzbSmokeHttpResponse *response)
{
    if (response == NULL)
        return;
    free(response->body);
    response->body = NULL;
    response->bodySize = 0;
    response->status = 0;
}

static bool
RzbSmoke_HttpRequest(const char *method, const char *url, const char *bearer,
                     const char *contentType, const char *body,
                     struct RzbSmokeHttpResponse *response)
{
    CURL *curl;
    CURLcode result;
    struct curl_slist *headers = NULL;
    char *authHeader = NULL;

    memset(response, 0, sizeof(*response));
    curl = curl_easy_init();
    if (curl == NULL)
        return false;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (bearer != NULL) {
        authHeader = RzbSmoke_Format("Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, authHeader);
    }
    if (contentType != NULL) {
        char *contentTypeHeader = RzbSmoke_Format("Content-Type: %s",
                                                  contentType);
        headers = curl_slist_append(headers, contentTypeHeader);
        free(contentTypeHeader);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RzbSmoke_CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    if (body != NULL)
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    result = curl_easy_perform(curl);
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(authHeader);
    return result == CURLE_OK;
}

static json_object *
RzbSmoke_HttpJson(const struct RzbSmokeHttpResponse *response)
{
    if (response->body == NULL)
        return NULL;
    return json_tokener_parse(response->body);
}

static bool
RzbSmoke_JsonString(json_object *object, const char *field, const char **value)
{
    json_object *child;

    if (!json_object_object_get_ex(object, field, &child) ||
        json_object_get_type(child) != json_type_string) {
        return false;
    }
    *value = json_object_get_string(child);
    return *value != NULL;
}

static json_object *
RzbSmoke_ResponseData(const struct RzbSmokeHttpResponse *response)
{
    json_object *root = RzbSmoke_HttpJson(response);
    json_object *data;

    if (root == NULL)
        return NULL;
    if (!json_object_object_get_ex(root, "data", &data)) {
        json_object_put(root);
        return NULL;
    }
    json_object_get(data);
    json_object_put(root);
    return data;
}

static json_object *
RzbSmoke_FindNamed(const struct RzbSmokeConfig *config, const char *bearer,
                   const char *collection, const char *name)
{
    char *encoded = RzbSmoke_QueryEncode(name, false);
    char *url = RzbSmoke_Format(
        "%s%s?status=active&search=%s&search_field=name&limit=100",
        config->apiUrl, collection, encoded);
    struct RzbSmokeHttpResponse response;
    json_object *root;
    json_object *data;
    size_t index;
    size_t count;

    free(encoded);
    if (url == NULL)
        return NULL;
    if (!RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response)) {
        free(url);
        return NULL;
    }
    free(url);
    if (response.status != 200) {
        RzbSmoke_HttpResponseClear(&response);
        return NULL;
    }
    root = RzbSmoke_HttpJson(&response);
    RzbSmoke_HttpResponseClear(&response);
    if (root == NULL)
        return NULL;
    if (!json_object_object_get_ex(root, "data", &data) ||
        json_object_get_type(data) != json_type_array) {
        json_object_put(root);
        return NULL;
    }
    count = json_object_array_length(data);
    for (index = 0; index < count; index++) {
        json_object *row = json_object_array_get_idx(data, index);
        const char *actual = NULL;

        if (row != NULL && RzbSmoke_JsonString(row, "name", &actual) &&
            strcmp(actual, name) == 0) {
            json_object_get(row);
            json_object_put(root);
            return row;
        }
    }
    json_object_put(root);
    return NULL;
}

static char *
RzbSmoke_BearerToken(void)
{
    const char *token;
    const char *keycloak;
    const char *realm;
    const char *clientId;
    const char *username;
    const char *password;
    char *url;
    char *body;
    struct RzbSmokeHttpResponse response;
    json_object *root;
    const char *accessToken;
    char *copy = NULL;

    if (RzbSmoke_EnvFalse("RZB_SMOKE_API_AUTH"))
        return NULL;
    token = getenv("RZB_SMOKE_API_BEARER_TOKEN");
    if (token == NULL || token[0] == '\0')
        token = getenv("RZB_SMOKE_BEARER_TOKEN");
    if (token != NULL && token[0] != '\0')
        return RzbSmoke_Strdup(token);

    keycloak = RzbSmoke_Env("RZB_SMOKE_KEYCLOAK_URL",
                            RZB_SMOKE_DEFAULT_KEYCLOAK_URL);
    realm = RzbSmoke_Env("RZB_SMOKE_KEYCLOAK_REALM",
                         RZB_SMOKE_DEFAULT_KEYCLOAK_REALM);
    clientId = RzbSmoke_Env("RZB_SMOKE_KEYCLOAK_CLIENT_ID",
                            RZB_SMOKE_DEFAULT_KEYCLOAK_CLIENT_ID);
    username = RzbSmoke_Env("RZB_SMOKE_KEYCLOAK_USERNAME",
                            RZB_SMOKE_DEFAULT_KEYCLOAK_USERNAME);
    password = RzbSmoke_Env("RZB_SMOKE_KEYCLOAK_PASSWORD",
                            RZB_SMOKE_DEFAULT_KEYCLOAK_PASSWORD);
    url = RzbSmoke_Format("%s/realms/%s/protocol/openid-connect/token",
                          keycloak, realm);
    body = RzbSmoke_Format(
        "grant_type=password&client_id=%s&username=%s&password=%s"
        "&scope=openid%%20profile%%20email",
        clientId, username, password);
    if (url == NULL || body == NULL)
        goto cleanup;
    if (!RzbSmoke_HttpRequest("POST", url, NULL,
                              "application/x-www-form-urlencoded", body,
                              &response)) {
        goto cleanup;
    }
    if (response.status != 200) {
        RzbSmoke_HttpResponseClear(&response);
        goto cleanup;
    }
    root = RzbSmoke_HttpJson(&response);
    RzbSmoke_HttpResponseClear(&response);
    if (root == NULL)
        goto cleanup;
    if (RzbSmoke_JsonString(root, "access_token", &accessToken))
        copy = RzbSmoke_Strdup(accessToken);
    json_object_put(root);

cleanup:
    free(url);
    free(body);
    return copy;
}

static char *
RzbSmoke_EnsureNuggetType(const struct RzbSmokeConfig *config,
                          const char *bearer)
{
    json_object *row = RzbSmoke_FindNamed(config, bearer,
                                          "/api/v1/catalog/nugget-types",
                                          config->nuggetType);
    const char *uuidText = NULL;
    char *copy = NULL;
    char *body;
    char *url;
    struct RzbSmokeHttpResponse response;
    json_object *data;

    if (row != NULL) {
        if (RzbSmoke_JsonString(row, "nugget_type_uuid", &uuidText))
            copy = RzbSmoke_Strdup(uuidText);
        json_object_put(row);
        return copy;
    }
    body = RzbSmoke_Format(
        "{\"name\":\"%s\",\"display_name\":\"%s\","
        "\"description\":\"Dispatcher-next SDK live smoke nugget type.\"}",
        config->nuggetType, config->nuggetType);
    url = RzbSmoke_Format("%s/api/v1/catalog/nugget-types", config->apiUrl);
    if (body == NULL || url == NULL)
        goto cleanup;
    if (!RzbSmoke_HttpRequest("POST", url, bearer, "application/json", body,
                              &response)) {
        goto cleanup;
    }
    if (response.status == 409) {
        RzbSmoke_HttpResponseClear(&response);
        row = RzbSmoke_FindNamed(config, bearer,
                                 "/api/v1/catalog/nugget-types",
                                 config->nuggetType);
        if (row != NULL && RzbSmoke_JsonString(row, "nugget_type_uuid",
                                               &uuidText)) {
            copy = RzbSmoke_Strdup(uuidText);
        }
        if (row != NULL)
            json_object_put(row);
        goto cleanup;
    }
    if (response.status == 201) {
        data = RzbSmoke_ResponseData(&response);
        if (data != NULL) {
            if (RzbSmoke_JsonString(data, "nugget_type_uuid", &uuidText))
                copy = RzbSmoke_Strdup(uuidText);
            json_object_put(data);
        }
    }
    RzbSmoke_HttpResponseClear(&response);

cleanup:
    free(body);
    free(url);
    return copy;
}

static bool
RzbSmoke_EnsureAppType(const struct RzbSmokeConfig *config, const char *bearer,
                       const char *nuggetTypeUuid)
{
    json_object *row = RzbSmoke_FindNamed(config, bearer,
                                          "/api/v1/catalog/app-types",
                                          config->appType);
    const char *actual = NULL;
    char *body = NULL;
    char *url = NULL;
    struct RzbSmokeHttpResponse response;
    bool ok = false;

    if (row != NULL) {
        ok = RzbSmoke_JsonString(row, "nugget_type_uuid", &actual) &&
             strcmp(actual, nuggetTypeUuid) == 0;
        json_object_put(row);
        return ok;
    }
    body = RzbSmoke_Format(
        "{\"name\":\"%s\",\"display_name\":\"%s\","
        "\"description\":\"Dispatcher-next SDK live smoke app type.\","
        "\"nugget_type_uuid\":\"%s\"}",
        config->appType, config->appType, nuggetTypeUuid);
    url = RzbSmoke_Format("%s/api/v1/catalog/app-types", config->apiUrl);
    if (body == NULL || url == NULL)
        goto cleanup;
    if (!RzbSmoke_HttpRequest("POST", url, bearer, "application/json", body,
                              &response)) {
        goto cleanup;
    }
    ok = response.status == 201 || response.status == 409;
    RzbSmoke_HttpResponseClear(&response);

cleanup:
    free(body);
    free(url);
    return ok;
}

static bool
RzbSmoke_EnsureNugget(const struct RzbSmokeConfig *config, const char *bearer,
                      const char *nuggetTypeUuid)
{
    char *pathUuid = RzbSmoke_QueryEncode(config->nuggetUuid, true);
    char *url = RzbSmoke_Format("%s/api/v1/catalog/nuggets/%s",
                                config->apiUrl, pathUuid);
    struct RzbSmokeHttpResponse response;
    char *body = NULL;
    bool ok = false;

    free(pathUuid);
    if (url == NULL)
        return false;
    if (!RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response))
        goto cleanup;
    if (response.status == 200) {
        ok = true;
        RzbSmoke_HttpResponseClear(&response);
        goto cleanup;
    }
    if (response.status != 404) {
        RzbSmoke_HttpResponseClear(&response);
        goto cleanup;
    }
    RzbSmoke_HttpResponseClear(&response);
    free(url);
    url = RzbSmoke_Format("%s/api/v1/catalog/nuggets", config->apiUrl);
    body = RzbSmoke_Format(
        "{\"nugget_uuid\":\"%s\",\"nugget_type_uuid\":\"%s\","
        "\"locality\":\"%s\",\"name\":\"C SDK smoke %s\","
        "\"provisioning_source\":\"manual\"}",
        config->nuggetUuid, nuggetTypeUuid, config->locality, config->label);
    if (url == NULL || body == NULL)
        goto cleanup;
    if (!RzbSmoke_HttpRequest("POST", url, bearer, "application/json", body,
                              &response)) {
        goto cleanup;
    }
    ok = response.status == 201 || response.status == 409;
    RzbSmoke_HttpResponseClear(&response);

cleanup:
    free(url);
    free(body);
    return ok;
}

static bool
RzbSmoke_RequireDataType(const struct RzbSmokeConfig *config, const char *bearer)
{
    char *dataType;
    char *url;
    struct RzbSmokeHttpResponse response;
    bool ok;

    if (config->dataType == NULL)
        return true;
    dataType = RzbSmoke_QueryEncode(config->dataType, true);
    url = RzbSmoke_Format("%s/api/v1/catalog/data-types/%s", config->apiUrl,
                          dataType);
    free(dataType);
    if (url == NULL)
        return false;
    ok = RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response) &&
         response.status == 200;
    RzbSmoke_HttpResponseClear(&response);
    free(url);
    return ok;
}

static bool
RzbSmoke_EnsureCatalog(const struct RzbSmokeConfig *config, const char *bearer)
{
    char *nuggetTypeUuid = RzbSmoke_EnsureNuggetType(config, bearer);
    bool ok;

    if (nuggetTypeUuid == NULL)
        return false;
    ok = RzbSmoke_EnsureAppType(config, bearer, nuggetTypeUuid) &&
         RzbSmoke_EnsureNugget(config, bearer, nuggetTypeUuid) &&
         RzbSmoke_RequireDataType(config, bearer);
    free(nuggetTypeUuid);
    return ok;
}

static bool
RzbSmoke_OnlineNugget(const struct RzbSmokeConfig *config, const char *bearer,
                      bool *present)
{
    char *uuid = RzbSmoke_QueryEncode(config->nuggetUuid, false);
    char *url = RzbSmoke_Format(
        "%s/api/v1/admin/nuggets/online?nugget_uuid=%s&limit=100",
        config->apiUrl, uuid);
    struct RzbSmokeHttpResponse response;
    json_object *root;
    json_object *data;
    size_t index;
    size_t count;
    bool ok = false;

    *present = false;
    free(uuid);
    if (url == NULL)
        return false;
    if (!RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response)) {
        free(url);
        return false;
    }
    free(url);
    if (response.status != 200)
        goto cleanup;
    root = RzbSmoke_HttpJson(&response);
    if (root == NULL)
        goto cleanup;
    if (!json_object_object_get_ex(root, "data", &data) ||
        json_object_get_type(data) != json_type_array) {
        json_object_put(root);
        goto cleanup;
    }
    count = json_object_array_length(data);
    for (index = 0; index < count; index++) {
        json_object *row = json_object_array_get_idx(data, index);
        const char *uuidText = NULL;
        const char *policy = NULL;
        const char *availability = NULL;
        const char *queue = NULL;
        char *expectedQueue = RzbNextCnc_DirectedCommandQueue(config->nuggetUuid);

        if (row != NULL &&
            RzbSmoke_JsonString(row, "nugget_uuid", &uuidText) &&
            strcmp(uuidText, config->nuggetUuid) == 0) {
            *present = true;
            ok = RzbSmoke_JsonString(row, "runtime_policy", &policy) &&
                 strcmp(policy, "running") == 0 &&
                 RzbSmoke_JsonString(row, "availability", &availability) &&
                 strcmp(availability, "ready") == 0 &&
                 RzbSmoke_JsonString(row, "command_queue", &queue) &&
                 expectedQueue != NULL && strcmp(queue, expectedQueue) == 0;
            free(expectedQueue);
            json_object_put(root);
            goto cleanup;
        }
        free(expectedQueue);
    }
    ok = true;
    json_object_put(root);

cleanup:
    RzbSmoke_HttpResponseClear(&response);
    return ok;
}

static bool
RzbSmoke_RoutePresent(const struct RzbSmokeConfig *config, const char *bearer,
                      bool *present)
{
    char *uuid;
    char *dataType;
    char *appType;
    char *url;
    struct RzbSmokeHttpResponse response;
    json_object *root;
    json_object *data;
    size_t index;
    size_t count;
    bool ok = false;

    *present = false;
    if (config->dataType == NULL)
        return true;
    uuid = RzbSmoke_QueryEncode(config->nuggetUuid, false);
    dataType = RzbSmoke_QueryEncode(config->dataType, false);
    appType = RzbSmoke_QueryEncode(config->appType, false);
    url = RzbSmoke_Format(
        "%s/api/v1/admin/routes?nugget_uuid=%s&data_type=%s"
        "&app_type=%s&eligible=true&limit=100",
        config->apiUrl, uuid, dataType, appType);
    free(uuid);
    free(dataType);
    free(appType);
    if (url == NULL)
        return false;
    if (!RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response)) {
        free(url);
        return false;
    }
    free(url);
    if (response.status != 200)
        goto cleanup;
    root = RzbSmoke_HttpJson(&response);
    if (root == NULL)
        goto cleanup;
    if (!json_object_object_get_ex(root, "data", &data) ||
        json_object_get_type(data) != json_type_array) {
        json_object_put(root);
        goto cleanup;
    }
    count = json_object_array_length(data);
    for (index = 0; index < count; index++) {
        json_object *row = json_object_array_get_idx(data, index);
        const char *uuidText = NULL;
        const char *dataTypeText = NULL;
        const char *appTypeText = NULL;
        json_object *eligible;

        if (row != NULL &&
            RzbSmoke_JsonString(row, "nugget_uuid", &uuidText) &&
            strcmp(uuidText, config->nuggetUuid) == 0 &&
            RzbSmoke_JsonString(row, "data_type", &dataTypeText) &&
            strcmp(dataTypeText, config->dataType) == 0 &&
            RzbSmoke_JsonString(row, "app_type", &appTypeText) &&
            strcmp(appTypeText, config->appType) == 0 &&
            json_object_object_get_ex(row, "eligible", &eligible) &&
            json_object_get_boolean(eligible)) {
            *present = true;
            break;
        }
    }
    ok = true;
    json_object_put(root);

cleanup:
    RzbSmoke_HttpResponseClear(&response);
    return ok;
}

static bool
RzbSmoke_ApiLiveState(const struct RzbSmokeConfig *config, const char *bearer)
{
    bool present = false;
    bool route = false;

    if (!RzbSmoke_OnlineNugget(config, bearer, &present) || !present)
        return false;
    if (config->dataType == NULL)
        return true;
    return RzbSmoke_RoutePresent(config, bearer, &route) && route;
}

static bool
RzbSmoke_ApiByeCleanup(const struct RzbSmokeConfig *config, const char *bearer)
{
    bool present = true;
    bool route = true;

    if (!RzbSmoke_OnlineNugget(config, bearer, &present) || present)
        return false;
    if (config->dataType == NULL)
        return true;
    return RzbSmoke_RoutePresent(config, bearer, &route) && !route;
}

static bool
RzbSmoke_ActiveInspectionPresent(const struct RzbSmokeConfig *source,
                                 const char *bearer,
                                 struct RzbSmokeOperation *operation,
                                 bool *present)
{
    char *eventId = RzbSmoke_QueryEncode(operation->eventId, false);
    char *appType = RzbSmoke_QueryEncode(operation->inspectorAppType, false);
    char *url = RzbSmoke_Format(
        "%s/api/v1/inspections/active?event_id=%s&app_type=%s&limit=100",
        source->apiUrl, eventId, appType);
    char expectedBlockKey[96];
    struct RzbSmokeHttpResponse response;
    json_object *root;
    json_object *data;
    size_t index;
    size_t count;
    bool ok = false;

    *present = false;
    free(eventId);
    free(appType);
    if (url == NULL)
        return false;
    snprintf(expectedBlockKey, sizeof(expectedBlockKey), "%s:%llu",
             operation->sha256, operation->size);
    if (!RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response)) {
        free(url);
        return false;
    }
    free(url);
    if (response.status != 200)
        goto cleanup;
    root = RzbSmoke_HttpJson(&response);
    if (root == NULL)
        goto cleanup;
    if (!json_object_object_get_ex(root, "data", &data) ||
        json_object_get_type(data) != json_type_array) {
        json_object_put(root);
        goto cleanup;
    }
    count = json_object_array_length(data);
    for (index = 0; index < count; index++) {
        json_object *row = json_object_array_get_idx(data, index);
        const char *eventIdText = NULL;
        const char *inspectionIdText = NULL;
        const char *appTypeText = NULL;
        const char *blockKey = NULL;
        const char *status = NULL;

        if (row != NULL &&
            RzbSmoke_JsonString(row, "event_id", &eventIdText) &&
            strcmp(eventIdText, operation->eventId) == 0 &&
            RzbSmoke_JsonString(row, "inspection_id", &inspectionIdText) &&
            RzbSmoke_JsonString(row, "app_type", &appTypeText) &&
            strcmp(appTypeText, operation->inspectorAppType) == 0 &&
            RzbSmoke_JsonString(row, "block_key", &blockKey) &&
            strcmp(blockKey, expectedBlockKey) == 0 &&
            RzbSmoke_JsonString(row, "status", &status) &&
            (strcmp(status, "pending") == 0 ||
             strcmp(status, "running") == 0 ||
             strcmp(status, "deferred") == 0)) {
            if (operation->inspectionId == NULL)
                operation->inspectionId = RzbSmoke_Strdup(inspectionIdText);
            *present = true;
            break;
        }
    }
    ok = true;
    json_object_put(root);

cleanup:
    RzbSmoke_HttpResponseClear(&response);
    return ok;
}

static bool
RzbSmoke_BlockPresent(const struct RzbSmokeConfig *source, const char *bearer,
                      const struct RzbSmokeOperation *operation, bool *present)
{
    char *sha = RzbSmoke_QueryEncode(operation->sha256, true);
    char *url = RzbSmoke_Format("%s/api/v1/blocks/sha256/%s/%llu",
                                source->apiUrl, sha, operation->size);
    struct RzbSmokeHttpResponse response;
    bool ok;

    *present = false;
    free(sha);
    if (url == NULL)
        return false;
    ok = RzbSmoke_HttpRequest("GET", url, bearer, NULL, NULL, &response);
    free(url);
    if (!ok)
        return false;
    if (response.status == 200) {
        *present = true;
        ok = true;
    } else if (response.status == 404) {
        ok = true;
    } else {
        ok = false;
    }
    RzbSmoke_HttpResponseClear(&response);
    return ok;
}

static bool
RzbSmoke_CheckRpc(amqp_connection_state_t connection, const char *operation)
{
    amqp_rpc_reply_t reply = amqp_get_rpc_reply(connection);

    if (reply.reply_type == AMQP_RESPONSE_NORMAL)
        return true;
    fprintf(stderr, "RabbitMQ %s failed: reply_type=%d library_error=%d\n",
            operation, reply.reply_type, reply.library_error);
    return false;
}

static bool
RzbSmoke_BrokerConnect(const char *url, struct RzbSmokeBroker *broker)
{
    char *mutableUrl = RzbSmoke_Strdup(url);
    struct amqp_connection_info info;
    amqp_socket_t *socket;

    memset(broker, 0, sizeof(*broker));
    if (mutableUrl == NULL)
        return false;
    amqp_default_connection_info(&info);
    if (amqp_parse_url(mutableUrl, &info) != AMQP_STATUS_OK || info.ssl) {
        free(mutableUrl);
        return false;
    }
    broker->connection = amqp_new_connection();
    socket = amqp_tcp_socket_new(broker->connection);
    if (socket == NULL ||
        amqp_socket_open(socket, info.host, info.port) != AMQP_STATUS_OK) {
        free(mutableUrl);
        return false;
    }
    broker->channel = 1;
    if (amqp_login(broker->connection, info.vhost, 0, 131072, 10,
                   AMQP_SASL_METHOD_PLAIN, info.user,
                   info.password).reply_type != AMQP_RESPONSE_NORMAL) {
        free(mutableUrl);
        return false;
    }
    amqp_channel_open(broker->connection, broker->channel);
    if (!RzbSmoke_CheckRpc(broker->connection, "channel_open")) {
        free(mutableUrl);
        return false;
    }
    free(mutableUrl);
    return true;
}

static void
RzbSmoke_BrokerClose(struct RzbSmokeBroker *broker)
{
    if (broker == NULL || broker->connection == NULL)
        return;
    amqp_channel_close(broker->connection, broker->channel, AMQP_REPLY_SUCCESS);
    amqp_connection_close(broker->connection, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(broker->connection);
    free(broker->helloQueue);
    free(broker->directedQueue);
    free(broker->workQueue);
    memset(broker, 0, sizeof(*broker));
}

static char *
RzbSmoke_CopyBytesString(amqp_bytes_t bytes)
{
    char *copy = calloc(bytes.len + 1U, sizeof(char));

    if (copy == NULL)
        return NULL;
    memcpy(copy, bytes.bytes, bytes.len);
    return copy;
}

static bool
RzbSmoke_DeclareTopology(struct RzbSmokeBroker *broker,
                         const struct RzbSmokeConfig *config)
{
    amqp_queue_declare_ok_t *hello;
    amqp_table_entry_t quorumEntry;
    amqp_table_t quorumArgs;

    amqp_exchange_declare(
        broker->connection, broker->channel,
        amqp_cstring_bytes(RZB_NEXT_EXCHANGE_DISPATCHER_HELLO),
        amqp_cstring_bytes("fanout"), 0, 0, 0, 0, amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "exchange_declare"))
        return false;
    hello = amqp_queue_declare(broker->connection, broker->channel,
                               amqp_empty_bytes, 0, 0, 1, 1,
                               amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "hello_queue_declare") ||
        hello == NULL) {
        return false;
    }
    broker->helloQueue = RzbSmoke_CopyBytesString(hello->queue);
    broker->directedQueue = RzbNextCnc_DirectedCommandQueue(config->nuggetUuid);
    if (broker->helloQueue == NULL || broker->directedQueue == NULL)
        return false;
    amqp_queue_bind(broker->connection, broker->channel,
                    amqp_cstring_bytes(broker->helloQueue),
                    amqp_cstring_bytes(RZB_NEXT_EXCHANGE_DISPATCHER_HELLO),
                    amqp_cstring_bytes(""), amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "hello_queue_bind"))
        return false;
    amqp_queue_declare(broker->connection, broker->channel,
                       amqp_cstring_bytes(broker->directedQueue), 0, 0, 1, 1,
                       amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "directed_queue_declare"))
        return false;
    if (config->role != RZB_SMOKE_ROLE_INSPECTOR)
        return true;

    broker->workQueue = RzbSmoke_Format("%s.%s",
                                        RZB_NEXT_QUEUE_INSPECTOR_PREFIX,
                                        config->appType);
    if (broker->workQueue == NULL)
        return false;
    quorumEntry.key = amqp_cstring_bytes("x-queue-type");
    quorumEntry.value.kind = AMQP_FIELD_KIND_UTF8;
    quorumEntry.value.value.bytes = amqp_cstring_bytes("quorum");
    quorumArgs.num_entries = 1;
    quorumArgs.entries = &quorumEntry;
    amqp_queue_declare(broker->connection, broker->channel,
                       amqp_cstring_bytes(broker->workQueue), 0, 1, 0, 0,
                       quorumArgs);
    return RzbSmoke_CheckRpc(broker->connection, "inspector_work_queue_declare");
}

static bool
RzbSmoke_DuplicateDirectedQueueRejected(const struct RzbSmokeConfig *config)
{
    struct RzbSmokeBroker duplicate;
    char *directedQueue;
    bool rejected = false;
    amqp_rpc_reply_t reply;

    if (!RzbSmoke_BrokerConnect(config->rabbitmqUrl, &duplicate))
        return false;
    directedQueue = RzbNextCnc_DirectedCommandQueue(config->nuggetUuid);
    if (directedQueue == NULL)
        goto cleanup;
    amqp_queue_declare(duplicate.connection, duplicate.channel,
                       amqp_cstring_bytes(directedQueue), 0, 0, 1, 1,
                       amqp_empty_table);
    reply = amqp_get_rpc_reply(duplicate.connection);
    rejected = reply.reply_type != AMQP_RESPONSE_NORMAL;

cleanup:
    free(directedQueue);
    RzbSmoke_BrokerClose(&duplicate);
    return rejected;
}

static bool
RzbSmoke_StartConsumers(struct RzbSmokeBroker *broker)
{
    amqp_basic_consume(broker->connection, broker->channel,
                       amqp_cstring_bytes(broker->helloQueue),
                       amqp_empty_bytes, 0, 0, 1, amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "hello_consume"))
        return false;
    amqp_basic_consume(broker->connection, broker->channel,
                       amqp_cstring_bytes(broker->directedQueue),
                       amqp_empty_bytes, 0, 0, 1, amqp_empty_table);
    if (!RzbSmoke_CheckRpc(broker->connection, "directed_consume"))
        return false;
    if (broker->workQueue == NULL)
        return true;
    amqp_basic_consume(broker->connection, broker->channel,
                       amqp_cstring_bytes(broker->workQueue),
                       amqp_empty_bytes, 0, 0, 1, amqp_empty_table);
    return RzbSmoke_CheckRpc(broker->connection, "inspector_work_consume");
}

static struct RzbNextMessageHeader *
RzbSmoke_DeliveryHeaders(const amqp_table_t *table, size_t *count)
{
    struct RzbNextMessageHeader *headers;
    int index;

    *count = 0;
    if (table == NULL || table->num_entries <= 0)
        return NULL;
    headers = calloc((size_t)table->num_entries, sizeof(*headers));
    if (headers == NULL)
        return NULL;
    for (index = 0; index < table->num_entries; index++) {
        amqp_table_entry_t entry = table->entries[index];
        if (entry.value.kind == AMQP_FIELD_KIND_UTF8 ||
            entry.value.kind == AMQP_FIELD_KIND_BYTES) {
            headers[*count].name = RzbSmoke_CopyBytesString(entry.key);
            headers[*count].value =
                RzbSmoke_CopyBytesString(entry.value.value.bytes);
            if (headers[*count].name != NULL && headers[*count].value != NULL)
                (*count)++;
        }
    }
    return headers;
}

static void
RzbSmoke_DeliveryHeadersClear(struct RzbNextMessageHeader *headers, size_t count)
{
    size_t index;

    for (index = 0; index < count; index++) {
        free(headers[index].name);
        free(headers[index].value);
    }
    free(headers);
}

static char *
RzbSmoke_ConsumeOne(struct RzbSmokeBroker *broker, int timeoutSeconds)
{
    struct timeval timeout = { timeoutSeconds, 0 };
    amqp_envelope_t envelope;
    amqp_rpc_reply_t reply;
    struct RzbNextMessageHeader *headers = NULL;
    struct RzbNextDecodedRabbitMqMessage *decoded = NULL;
    size_t headerCount = 0;
    char *json = NULL;

    memset(&envelope, 0, sizeof(envelope));
    reply = amqp_consume_message(broker->connection, &envelope, &timeout, 0);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        return NULL;
    if ((envelope.message.properties._flags & AMQP_BASIC_HEADERS_FLAG) != 0) {
        headers = RzbSmoke_DeliveryHeaders(&envelope.message.properties.headers,
                                           &headerCount);
    }
    if (RzbNextRabbitMq_DecodeMessage(envelope.message.body.bytes,
                                      envelope.message.body.len, headers,
                                      headerCount, NULL, 0, &decoded)) {
        json = RzbSmoke_Strdup(decoded->jsonMessage);
        amqp_basic_ack(broker->connection, broker->channel,
                       envelope.delivery_tag, 0);
    } else {
        amqp_basic_reject(broker->connection, broker->channel,
                          envelope.delivery_tag, 0);
    }
    RzbNextDecodedRabbitMqMessage_Destroy(decoded);
    RzbSmoke_DeliveryHeadersClear(headers, headerCount);
    amqp_destroy_envelope(&envelope);
    return json;
}

static bool
RzbSmoke_Publish(struct RzbSmokeBroker *broker, const char *jsonMessage,
                 uint64_t expirationSeconds)
{
    struct RzbNextPreparedRabbitMqMessage *prepared = NULL;
    amqp_basic_properties_t properties;
    amqp_table_entry_t *entries = NULL;
    size_t index;
    int result;
    bool ok = false;

    if (!RzbNextRabbitMq_PrepareMessage(jsonMessage, NULL, NULL, NULL,
                                        &prepared)) {
        return false;
    }
    memset(&properties, 0, sizeof(properties));
    properties._flags = AMQP_BASIC_CONTENT_TYPE_FLAG |
                        AMQP_BASIC_DELIVERY_MODE_FLAG |
                        AMQP_BASIC_HEADERS_FLAG;
    properties.content_type = amqp_cstring_bytes(prepared->contentType);
    properties.delivery_mode = 1;
    if (prepared->contentEncoding != NULL) {
        properties._flags |= AMQP_BASIC_CONTENT_ENCODING_FLAG;
        properties.content_encoding =
            amqp_cstring_bytes(prepared->contentEncoding);
    }
    entries = calloc(prepared->headerCount, sizeof(*entries));
    if (entries == NULL)
        goto cleanup;
    for (index = 0; index < prepared->headerCount; index++) {
        entries[index].key = amqp_cstring_bytes(prepared->headers[index].name);
        entries[index].value.kind = AMQP_FIELD_KIND_UTF8;
        entries[index].value.value.bytes =
            amqp_cstring_bytes(prepared->headers[index].value);
    }
    properties.headers.num_entries = (int)prepared->headerCount;
    properties.headers.entries = entries;
    if (expirationSeconds > 0U) {
        char expiration[32];

        snprintf(expiration, sizeof(expiration), "%llu",
                 (unsigned long long)(expirationSeconds * 1000U));
        properties._flags |= AMQP_BASIC_EXPIRATION_FLAG;
        properties.expiration = amqp_cstring_bytes(expiration);
        result = amqp_basic_publish(
            broker->connection, broker->channel,
            amqp_cstring_bytes(prepared->route->exchange),
            amqp_cstring_bytes(prepared->route->routingKey), 1, 0,
            &properties,
            (amqp_bytes_t){ prepared->bodySize, prepared->body });
    } else {
        result = amqp_basic_publish(
            broker->connection, broker->channel,
            amqp_cstring_bytes(prepared->route->exchange),
            amqp_cstring_bytes(prepared->route->routingKey), 1, 0,
            &properties,
            (amqp_bytes_t){ prepared->bodySize, prepared->body });
    }
    ok = result == AMQP_STATUS_OK;

cleanup:
    free(entries);
    RzbNextPreparedRabbitMqMessage_Destroy(prepared);
    return ok;
}

static char *
RzbSmoke_RegistrationRequest(RzbNextRuntime_t *runtime,
                             const struct RzbSmokeConfig *config,
                             const char *requestId, const char *createdAt)
{
    char *dataTypes = NULL;
    char *message;

    if (config->dataType != NULL) {
        dataTypes = RzbSmoke_Format("\"data_types\":[\"%s\"],",
                                    config->dataType);
    }
    message = RzbSmoke_Format(
        "{"
        "\"schema_name\":\"razorback.cnc.registration_request\","
        "\"schema_version\":1,"
        "\"request_id\":\"%s\","
        "\"nugget_uuid\":\"%s\","
        "\"process_uuid\":\"%s\","
        "\"nugget_type\":\"%s\","
        "\"app_type\":\"%s\","
        "%s"
        "\"capabilities\":{"
        "\"component_version\":\"0.0.0\","
        "\"sdk_name\":\"razorback-c\","
        "\"sdk_version\":\"0.0.0\","
        "\"supported_message_body_modes\":[\"inline\",\"zlib\",\"claim_check\"],"
        "\"supports_deferred_results\":%s"
        "},"
        "\"desired_runtime_policy\":\"running\","
        "\"created_at\":\"%s\""
        "}",
        requestId, config->nuggetUuid, RzbNextRuntime_ProcessUuid(runtime),
        config->nuggetType, config->appType,
        dataTypes == NULL ? "" : dataTypes,
        config->role == RZB_SMOKE_ROLE_INSPECTOR ? "true" : "false",
        createdAt);
    free(dataTypes);
    return message;
}

static char *
RzbSmoke_BlockSubmission(const struct RzbSmokeConfig *config,
                         const struct RzbSmokeOperation *operation,
                         const char *createdAt)
{
    return RzbSmoke_Format(
        "{"
        "\"schema_name\":\"%s\","
        "\"schema_version\":1,"
        "\"event_id\":\"%s\","
        "\"source_nugget_uuid\":\"%s\","
        "\"block\":{"
        "\"sha256\":\"%s\","
        "\"size\":%llu,"
        "\"data_type\":\"%s\""
        "},"
        "\"stored\":true,"
        "\"event_metadata\":[{"
        "\"name\":\"filename\","
        "\"type\":\"string\","
        "\"value\":\"c-sdk-smoke-%s.pdf\""
        "}],"
        "\"created_at\":\"%s\""
        "}",
        RZB_NEXT_SCHEMA_BLOCK_SUBMISSION, operation->eventId,
        config->nuggetUuid, operation->sha256, operation->size,
        operation->dataType, operation->eventId, createdAt);
}

static char *
RzbSmoke_BlockUpdate(const struct RzbSmokeConfig *config,
                     const struct RzbSmokeOperation *operation,
                     const char *createdAt)
{
    return RzbSmoke_Format(
        "{"
        "\"schema_name\":\"%s\","
        "\"schema_version\":1,"
        "\"update_id\":\"%s\","
        "\"source_nugget_uuid\":\"%s\","
        "\"block\":{"
        "\"sha256\":\"%s\","
        "\"size\":%llu,"
        "\"data_type\":\"%s\""
        "},"
        "\"metadata_updates\":[{"
        "\"name\":\"filename\","
        "\"type\":\"string\","
        "\"created_at\":\"%s\","
        "\"value\":\"c-sdk-smoke-update-%s.pdf\""
        "}],"
        "\"created_at\":\"%s\""
        "}",
        RZB_NEXT_SCHEMA_BLOCK_UPDATE, operation->updateId,
        config->nuggetUuid, operation->sha256, operation->size,
        operation->dataType, createdAt, operation->updateId, createdAt);
}

static bool
RzbSmoke_PublishOperationalMessages(struct RzbSmokeBroker *broker,
                                    const struct RzbSmokeConfig *config,
                                    const struct RzbSmokeOperation *operation,
                                    struct RzbSmokeReport *report)
{
    char *createdAt = RzbSmoke_Timestamp();
    char *submission = NULL;
    char *update = NULL;
    bool ok = false;

    if (createdAt == NULL)
        return false;
    submission = RzbSmoke_BlockSubmission(config, operation, createdAt);
    update = RzbSmoke_BlockUpdate(config, operation, createdAt);
    ok = submission != NULL && update != NULL &&
         RzbSmoke_Publish(broker, submission, 0U) &&
         RzbSmoke_Publish(broker, update, 0U);
    if (ok) {
        report->blockSubmissionPublished = true;
        report->blockUpdatePublished = true;
    }
    free(createdAt);
    free(submission);
    free(update);
    return ok;
}

static const char *
RzbSmoke_SchemaName(const char *jsonMessage)
{
    static char schemaName[128];
    json_object *object = json_tokener_parse(jsonMessage);
    const char *value = NULL;

    schemaName[0] = '\0';
    if (object == NULL)
        return schemaName;
    if (RzbSmoke_JsonString(object, "schema_name", &value))
        snprintf(schemaName, sizeof(schemaName), "%s", value);
    json_object_put(object);
    return schemaName;
}

static bool
RzbSmoke_HandleInspectionWork(const struct RzbSmokeConfig *config,
                              const struct RzbSmokeOperation *operation,
                              const char *jsonMessage,
                              struct RzbSmokeBroker *broker,
                              struct RzbSmokeReport *report)
{
    json_object *root;
    json_object *event;
    json_object *block;
    json_object *sizeObject;
    const char *eventId = NULL;
    const char *appType = NULL;
    const char *workKind = NULL;
    const char *sha256 = NULL;
    const char *dataType = NULL;
    char *createdAt = NULL;
    char *result = NULL;
    bool matched = false;
    bool ok = false;

    if (operation == NULL || config->role != RZB_SMOKE_ROLE_INSPECTOR)
        return false;
    root = json_tokener_parse(jsonMessage);
    if (root == NULL)
        return false;
    ok = RzbSmoke_JsonString(root, "app_type", &appType) &&
         strcmp(appType, config->appType) == 0 &&
         RzbSmoke_JsonString(root, "work_kind", &workKind) &&
         strcmp(workKind, "inspect") == 0 &&
         json_object_object_get_ex(root, "event", &event) &&
         RzbSmoke_JsonString(event, "event_id", &eventId) &&
         strcmp(eventId, operation->eventId) == 0 &&
         json_object_object_get_ex(root, "block", &block) &&
         RzbSmoke_JsonString(block, "sha256", &sha256) &&
         strcmp(sha256, operation->sha256) == 0 &&
         RzbSmoke_JsonString(block, "data_type", &dataType) &&
         strcmp(dataType, operation->dataType) == 0 &&
         json_object_object_get_ex(block, "size", &sizeObject) &&
         (unsigned long long)json_object_get_int64(sizeObject) ==
             operation->size;
    matched = ok;
    if (ok) {
        createdAt = RzbSmoke_Timestamp();
        result = RzbNextAnalysisResult_BuildCompleted(
            jsonMessage, config->nuggetUuid, createdAt, NULL,
            "[{\"name\":\"smoke_result\",\"type\":\"string\",\"value\":\"completed\"}]",
            NULL, NULL);
        ok = result != NULL && RzbSmoke_Publish(broker, result, 0U);
    }
    if (ok) {
        report->inspectionWorkReceived = true;
        report->analysisResultPublished = true;
    }
    free(createdAt);
    free(result);
    json_object_put(root);
    return matched ? ok : true;
}

static bool
RzbSmoke_HandleMessage(RzbNextRuntime_t *runtime,
                       const struct RzbSmokeConfig *config,
                       const struct RzbSmokeOperation *operation,
                       struct RzbSmokeBroker *broker, const char *jsonMessage,
                       struct RzbSmokeReport *report)
{
    const char *schemaName = RzbSmoke_SchemaName(jsonMessage);

    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DISPATCHER_HELLO) == 0) {
        if (RzbNextRuntime_ObserveDispatcherHello(runtime, jsonMessage) &&
            !report->registrationPublished) {
            char *requestId = RzbSmoke_Uuid();
            char *createdAt = RzbSmoke_Timestamp();
            char *registration =
                RzbSmoke_RegistrationRequest(runtime, config, requestId,
                                             createdAt);

            if (requestId == NULL || createdAt == NULL ||
                registration == NULL) {
                free(requestId);
                free(createdAt);
                free(registration);
                return false;
            }
            RzbNextRuntime_BeginRegistration(runtime, requestId);
            if (!RzbSmoke_Publish(broker, registration, 0U)) {
                free(requestId);
                free(createdAt);
                free(registration);
                return false;
            }
            report->registrationPublished = true;
            free(requestId);
            free(createdAt);
            free(registration);
        }
        return true;
    }
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED) == 0) {
        struct RzbNextRuntimeTransition transition;

        return RzbNextRuntime_RegistrationAccepted(runtime, jsonMessage,
                                                   &transition) &&
               transition.kind == RZB_NEXT_RUNTIME_TRANSITION_REGISTERED;
    }
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED) == 0) {
        struct RzbNextRuntimeTransition transition;

        RzbNextRuntime_RegistrationRejected(runtime, jsonMessage, &transition);
        return false;
    }
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND) == 0) {
        struct RzbNextRuntimeDirectedResult result;

        if (!RzbNextRuntime_ApplyDirectedCommand(runtime, jsonMessage, &result))
            return false;
        if (result.effect == RZB_NEXT_RUNTIME_DIRECTED_SHUTDOWN) {
            char *createdAt = RzbSmoke_Timestamp();
            char *bye = RzbNextRuntime_BuildBye(runtime, "terminate_command",
                                                createdAt);
            bool ok = bye != NULL && RzbSmoke_Publish(broker, bye, 0U);

            free(createdAt);
            free(bye);
            report->byePublished = ok;
            return ok;
        }
        if (result.effect == RZB_NEXT_RUNTIME_DIRECTED_REREGISTER)
            report->registrationPublished = false;
        return true;
    }
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_INSPECTION_WORK) == 0)
        return RzbSmoke_HandleInspectionWork(config, operation, jsonMessage,
                                             broker, report);
    return false;
}

static bool
RzbSmoke_PublishLiveness(RzbNextRuntime_t *runtime,
                         struct RzbSmokeBroker *broker,
                         struct RzbSmokeReport *report)
{
    struct RzbNextRuntimeLivenessPlan plan;
    char *createdAt = RzbSmoke_Timestamp();
    bool ok;

    if (createdAt == NULL)
        return false;
    ok = RzbNextRuntime_LivenessPlan(runtime, createdAt, &plan);
    free(createdAt);
    if (!ok)
        return false;
    ok = RzbSmoke_Publish(broker, plan.message, plan.messageExpiration);
    if (ok)
        report->livenessPublished = true;
    RzbNextRuntime_LivenessPlanClear(&plan);
    return ok;
}

static bool
RzbSmoke_ReportComplete(const struct RzbSmokeConfig *config,
                        const struct RzbSmokeOperation *operation,
                        const struct RzbSmokeReport *report)
{
    if (report->deliveries == 0 || !report->registrationPublished ||
        !report->livenessPublished || !report->byePublished ||
        !report->liveStateSeen) {
        return false;
    }
    if (operation == NULL)
        return true;
    if (config->role == RZB_SMOKE_ROLE_INSPECTOR)
        return report->inspectionWorkReceived && report->analysisResultPublished;
    return report->blockSubmissionPublished && report->blockUpdatePublished &&
           report->blockSeen;
}

static bool
RzbSmoke_RunBrokerLoop(const struct RzbSmokeConfig *config, const char *bearer,
                       const struct RzbSmokeConfig *peerConfig,
                       struct RzbSmokeOperation *operation,
                       struct RzbSmokeBroker *broker, unsigned long runSeconds,
                       struct RzbSmokeReport *report)
{
    RzbNextRuntime_t *runtime =
        RzbNextRuntime_CreateGenerated(config->nuggetUuid);
    time_t deadline = time(NULL) + (time_t)runSeconds;
    time_t nextLiveness = 0;
    time_t nextApiCheck = 0;
    time_t nextOperationalCheck = 0;
    bool ok = false;

    if (runtime == NULL)
        return false;
    RzbNextRuntime_Initialize(runtime);
    while (time(NULL) < deadline) {
        char *jsonMessage = RzbSmoke_ConsumeOne(broker, 1);

        if (jsonMessage != NULL) {
            report->deliveries++;
            if (!RzbSmoke_HandleMessage(runtime, config, operation, broker,
                                        jsonMessage, report)) {
                free(jsonMessage);
                goto cleanup;
            }
            free(jsonMessage);
        }
        if (RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY &&
            time(NULL) >= nextLiveness) {
            if (!RzbSmoke_PublishLiveness(runtime, broker, report))
                goto cleanup;
            nextLiveness = time(NULL) + 10;
        }
        if (RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY &&
            !report->liveStateSeen && time(NULL) >= nextApiCheck) {
            report->liveStateSeen = RzbSmoke_ApiLiveState(config, bearer);
            nextApiCheck = time(NULL) + 1;
        }
        if (operation != NULL &&
            config->role == RZB_SMOKE_ROLE_SOURCE &&
            RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY &&
            report->liveStateSeen && time(NULL) >= nextOperationalCheck) {
            bool peerReady = true;

            if (peerConfig != NULL)
                peerReady = RzbSmoke_ApiLiveState(peerConfig, bearer);
            if (!report->blockSubmissionPublished && peerReady) {
                if (!RzbSmoke_PublishOperationalMessages(broker, config,
                                                         operation, report))
                    goto cleanup;
            }
            if (report->blockSubmissionPublished &&
                !report->activeInspectionSeen) {
                bool present = false;

                if (RzbSmoke_ActiveInspectionPresent(config, bearer,
                                                     operation, &present) &&
                    present) {
                    report->activeInspectionSeen = true;
                }
            }
            if (report->blockSubmissionPublished && !report->blockSeen) {
                bool present = false;

                if (RzbSmoke_BlockPresent(config, bearer, operation,
                                          &present) &&
                    present) {
                    report->blockSeen = true;
                }
            }
            nextOperationalCheck = time(NULL) + 1;
        }
        if (operation != NULL && RzbSmoke_ReportComplete(config, operation,
                                                        report)) {
            break;
        }
    }
    if (!report->byePublished) {
        char *createdAt = RzbSmoke_Timestamp();
        char *bye = RzbNextRuntime_BuildBye(runtime, "shutdown", createdAt);

        if (bye == NULL || !RzbSmoke_Publish(broker, bye, 0U)) {
            free(createdAt);
            free(bye);
            goto cleanup;
        }
        report->byePublished = true;
        free(createdAt);
        free(bye);
    }
    ok = RzbSmoke_ReportComplete(config, operation, report);
    if (!ok) {
        fprintf(stderr,
                "dispatcher-next C %s live smoke incomplete: "
                "deliveries=%zu registration=%s liveness=%s bye=%s "
                "api_live_state=%s block_submission=%s block_update=%s "
                "inspection_work=%s analysis_result=%s "
                "active_inspection=%s block_seen=%s\n",
                config->label, report->deliveries,
                report->registrationPublished ? "true" : "false",
                report->livenessPublished ? "true" : "false",
                report->byePublished ? "true" : "false",
                report->liveStateSeen ? "true" : "false",
                report->blockSubmissionPublished ? "true" : "false",
                report->blockUpdatePublished ? "true" : "false",
                report->inspectionWorkReceived ? "true" : "false",
                report->analysisResultPublished ? "true" : "false",
                report->activeInspectionSeen ? "true" : "false",
                report->blockSeen ? "true" : "false");
    }

cleanup:
    RzbNextRuntime_Destroy(runtime);
    return ok;
}

static bool
RzbSmoke_RunLiveConfigured(const struct RzbSmokeConfig *config,
                           const char *bearer,
                           const struct RzbSmokeConfig *peerConfig,
                           struct RzbSmokeOperation *operation)
{
    struct RzbSmokeBroker broker;
    struct RzbSmokeReport report;
    unsigned long runSeconds;
    unsigned long cleanupSeconds;
    time_t cleanupDeadline;
    bool cleaned = false;

    memset(&report, 0, sizeof(report));
    if (!RzbSmoke_BrokerConnect(config->rabbitmqUrl, &broker)) {
        fprintf(stderr, "dispatcher-next C %s live smoke broker connect failed\n",
                config->label);
        return false;
    }
    if (!RzbSmoke_DeclareTopology(&broker, config) ||
        !RzbSmoke_DuplicateDirectedQueueRejected(config) ||
        !RzbSmoke_StartConsumers(&broker)) {
        fprintf(stderr, "dispatcher-next C %s live smoke topology failed\n",
                config->label);
        RzbSmoke_BrokerClose(&broker);
        return false;
    }
    runSeconds = RzbSmoke_EnvSeconds("RZB_SMOKE_LIVE_SECONDS",
                                     RZB_SMOKE_DEFAULT_SECONDS);
    if (!RzbSmoke_RunBrokerLoop(config, bearer, peerConfig, operation,
                                &broker, runSeconds, &report)) {
        fprintf(stderr, "dispatcher-next C %s live smoke loop failed\n",
                config->label);
        RzbSmoke_BrokerClose(&broker);
        return false;
    }
    RzbSmoke_BrokerClose(&broker);
    cleanupSeconds = RzbSmoke_EnvSeconds("RZB_SMOKE_BYE_CLEANUP_SECONDS", 10);
    cleanupDeadline = time(NULL) + (time_t)cleanupSeconds;
    while (time(NULL) <= cleanupDeadline) {
        if (RzbSmoke_ApiByeCleanup(config, bearer)) {
            cleaned = true;
            break;
        }
        sleep(1);
    }
    if (!cleaned) {
        fprintf(stderr, "dispatcher-next C %s live smoke BYE cleanup failed\n",
                config->label);
        return false;
    }
    printf("dispatcher-next C %s live smoke client passed after %lus\n",
           config->label, runSeconds);
    return true;
}

static bool RZB_SMOKE_UNUSED
RzbSmoke_RunLive(enum RzbSmokeRole role)
{
    struct RzbSmokeConfig config;
    char *bearer;
    bool ok;

    RzbSmoke_DefaultConfig(role, &config);
    bearer = RzbSmoke_BearerToken();
    if (!RzbSmoke_EnvFalse("RZB_SMOKE_API_AUTH") && bearer == NULL) {
        fprintf(stderr, "dispatcher-next C %s live smoke could not get token\n",
                config.label);
        return false;
    }
    if (!RzbSmoke_EnsureCatalog(&config, bearer)) {
        fprintf(stderr, "dispatcher-next C %s live smoke catalog setup failed\n",
                config.label);
        free(bearer);
        return false;
    }
    ok = RzbSmoke_RunLiveConfigured(&config, bearer, NULL, NULL);
    free(bearer);
    return ok;
}

static bool RZB_SMOKE_UNUSED
RzbSmoke_RunLiveOperational(void)
{
    struct RzbSmokeConfig source;
    struct RzbSmokeConfig inspector;
    struct RzbSmokeOperation operation;
    char *bearer;
    pid_t child;
    int status = 0;
    bool sourceOk;
    bool childOk;

    RzbSmoke_DefaultConfig(RZB_SMOKE_ROLE_SOURCE, &source);
    RzbSmoke_DefaultConfig(RZB_SMOKE_ROLE_INSPECTOR, &inspector);
    if (!RzbSmoke_OperationInit(&inspector, &operation))
        return false;
    bearer = RzbSmoke_BearerToken();
    if (!RzbSmoke_EnvFalse("RZB_SMOKE_API_AUTH") && bearer == NULL) {
        fprintf(stderr, "dispatcher-next C operational live smoke could not get token\n");
        RzbSmoke_OperationClear(&operation);
        return false;
    }
    if (!RzbSmoke_EnsureCatalog(&source, bearer) ||
        !RzbSmoke_EnsureCatalog(&inspector, bearer)) {
        fprintf(stderr, "dispatcher-next C operational live smoke catalog setup failed\n");
        free(bearer);
        RzbSmoke_OperationClear(&operation);
        return false;
    }
    child = fork();
    if (child < 0) {
        free(bearer);
        RzbSmoke_OperationClear(&operation);
        return false;
    }
    if (child == 0) {
        bool ok = RzbSmoke_RunLiveConfigured(&inspector, bearer, NULL,
                                             &operation);

        free(bearer);
        RzbSmoke_OperationClear(&operation);
        _exit(ok ? 0 : 1);
    }
    sourceOk = RzbSmoke_RunLiveConfigured(&source, bearer, &inspector,
                                          &operation);
    if (waitpid(child, &status, 0) < 0)
        childOk = false;
    else
        childOk = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (sourceOk && childOk) {
        if (!RzbSmoke_WriteOperationReport(&operation, "c"))
            sourceOk = false;
    }
    if (sourceOk && childOk) {
        printf("dispatcher-next C operational live smoke passed for event %s "
               "and block %s:%llu\n",
               operation.eventId, operation.sha256, operation.size);
    }
    free(bearer);
    RzbSmoke_OperationClear(&operation);
    return sourceOk && childOk;
}

#endif /* RAZORBACK_TESTS_SMOKE_LIVE_NEXT_H */
