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
};

struct RzbSmokeReport
{
    size_t deliveries;
    bool registrationPublished;
    bool livenessPublished;
    bool byePublished;
    bool liveStateSeen;
};

static bool RzbSmoke_LiveEnabled(void);
static bool RzbSmoke_RunLive(enum RzbSmokeRole role);

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
RzbSmoke_DeclareTopology(struct RzbSmokeBroker *broker, const char *nuggetUuid)
{
    amqp_queue_declare_ok_t *hello;

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
    broker->directedQueue = RzbNextCnc_DirectedCommandQueue(nuggetUuid);
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
    return RzbSmoke_CheckRpc(broker->connection, "directed_queue_declare");
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
    return RzbSmoke_CheckRpc(broker->connection, "directed_consume");
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
RzbSmoke_HandleMessage(RzbNextRuntime_t *runtime,
                       const struct RzbSmokeConfig *config,
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
RzbSmoke_RunBrokerLoop(const struct RzbSmokeConfig *config, const char *bearer,
                       struct RzbSmokeBroker *broker, unsigned long runSeconds,
                       struct RzbSmokeReport *report)
{
    RzbNextRuntime_t *runtime =
        RzbNextRuntime_CreateGenerated(config->nuggetUuid);
    time_t deadline = time(NULL) + (time_t)runSeconds;
    time_t nextLiveness = 0;
    time_t nextApiCheck = 0;
    bool ok = false;

    if (runtime == NULL)
        return false;
    RzbNextRuntime_Initialize(runtime);
    while (time(NULL) < deadline) {
        char *jsonMessage = RzbSmoke_ConsumeOne(broker, 1);

        if (jsonMessage != NULL) {
            report->deliveries++;
            if (!RzbSmoke_HandleMessage(runtime, config, broker, jsonMessage,
                                        report)) {
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
    ok = report->deliveries > 0 && report->registrationPublished &&
         report->livenessPublished && report->byePublished &&
         report->liveStateSeen;
    if (!ok) {
        fprintf(stderr,
                "dispatcher-next C %s live smoke incomplete: "
                "deliveries=%zu registration=%s liveness=%s bye=%s "
                "api_live_state=%s\n",
                config->label, report->deliveries,
                report->registrationPublished ? "true" : "false",
                report->livenessPublished ? "true" : "false",
                report->byePublished ? "true" : "false",
                report->liveStateSeen ? "true" : "false");
    }

cleanup:
    RzbNextRuntime_Destroy(runtime);
    return ok;
}

static bool
RzbSmoke_RunLive(enum RzbSmokeRole role)
{
    struct RzbSmokeConfig config;
    struct RzbSmokeBroker broker;
    struct RzbSmokeReport report;
    char *bearer;
    unsigned long runSeconds;
    unsigned long cleanupSeconds;
    time_t cleanupDeadline;
    bool cleaned = false;

    RzbSmoke_DefaultConfig(role, &config);
    memset(&report, 0, sizeof(report));
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
    if (!RzbSmoke_BrokerConnect(config.rabbitmqUrl, &broker)) {
        fprintf(stderr, "dispatcher-next C %s live smoke broker connect failed\n",
                config.label);
        free(bearer);
        return false;
    }
    if (!RzbSmoke_DeclareTopology(&broker, config.nuggetUuid) ||
        !RzbSmoke_DuplicateDirectedQueueRejected(&config) ||
        !RzbSmoke_StartConsumers(&broker)) {
        fprintf(stderr, "dispatcher-next C %s live smoke topology failed\n",
                config.label);
        RzbSmoke_BrokerClose(&broker);
        free(bearer);
        return false;
    }
    runSeconds = RzbSmoke_EnvSeconds("RZB_SMOKE_LIVE_SECONDS",
                                     RZB_SMOKE_DEFAULT_SECONDS);
    if (!RzbSmoke_RunBrokerLoop(&config, bearer, &broker, runSeconds,
                                &report)) {
        fprintf(stderr, "dispatcher-next C %s live smoke loop failed\n",
                config.label);
        RzbSmoke_BrokerClose(&broker);
        free(bearer);
        return false;
    }
    RzbSmoke_BrokerClose(&broker);
    cleanupSeconds = RzbSmoke_EnvSeconds("RZB_SMOKE_BYE_CLEANUP_SECONDS", 10);
    cleanupDeadline = time(NULL) + (time_t)cleanupSeconds;
    while (time(NULL) <= cleanupDeadline) {
        if (RzbSmoke_ApiByeCleanup(&config, bearer)) {
            cleaned = true;
            break;
        }
        sleep(1);
    }
    free(bearer);
    if (!cleaned) {
        fprintf(stderr, "dispatcher-next C %s live smoke BYE cleanup failed\n",
                config.label);
        return false;
    }
    printf("dispatcher-next C %s live smoke client passed after %lus\n",
           config.label, runSeconds);
    return true;
}

#endif /* RAZORBACK_TESTS_SMOKE_LIVE_NEXT_H */
