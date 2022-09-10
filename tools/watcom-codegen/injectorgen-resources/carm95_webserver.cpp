
#include "carm95_webserver.h"

#include <stdio.h>

#if HOOK_WEBSERVER

#include <ulfius.h>

#include <string.h>
#include <vector>

extern "C" {

#define ARRAY_SIZE(A) (sizeof(A)/sizeof(*(A)))

typedef struct webserver_hook_element {
    const char *name;
    function_hook_state_t *state;
    const char *file;
    int line;
} webserver_hook_element;

static struct _u_instance ulfius_instance;
static std::vector<webserver_hook_element> hook_elements;

static json_t* wrap_result_json(int code, const char *message, json_t *obj) {
    json_t *result;

    result = json_object();
    json_object_set(result, "code", json_integer(code));
    json_object_set(result, "message", json_string(message));
    if (obj != NULL) {
        json_object_set(result, "data", obj);
    }

    return result;
}

static json_t* wrap_result_success_json(json_t *obj) {
    return wrap_result_json(0, "success", obj);
}

static json_t* get_symbol_as_json(int index) {
    json_t *symbol_obj;

    symbol_obj = json_object();
    json_object_set(symbol_obj, "name", json_string(hook_elements[index].name));
    json_object_set(symbol_obj, "file", json_string(hook_elements[index].file));
    json_object_set(symbol_obj, "line", json_integer(hook_elements[index].line));
    json_object_set(symbol_obj, "state", json_integer(*hook_elements[index].state));

    return symbol_obj;
}

static int callback_get_helloworld(const struct _u_request *request, struct _u_response *response, void *user_data) {
    (void)request;
    (void)user_data;

    ulfius_set_string_body_response(response, 200, "Hello World!");
    return U_CALLBACK_COMPLETE;
}

static int callback_get_info(const struct _u_request *request, struct _u_response *response, void *user_data) {
    json_t *data;
    json_t *state_data;

    (void)request;
    (void)user_data;

    data = json_object();
    json_object_set(data, "jansson_version", json_string(jansson_version_str()));

    state_data = json_object();
    json_object_set(state_data, "unavailable", json_integer(HOOK_UNAVAILABLE));
    json_object_set(state_data, "enabled", json_integer(HOOK_ENABLED));
    json_object_set(state_data, "disabled", json_integer(HOOK_DISABLED));
    json_object_set(data, "states", state_data);

    ulfius_set_json_body_response(response, 200, wrap_result_success_json(data));
    return U_CALLBACK_COMPLETE;
}

static int callback_get_hooks(const struct _u_request *request, struct _u_response *response, void *user_data) {
    json_t *data;

    (void)request;
    (void)user_data;

    data = json_array();
    for (size_t i = 0; i < hook_elements.size(); i++) {
        json_array_append(data, get_symbol_as_json(i));
    }
    ulfius_set_json_body_response(response, 200, wrap_result_success_json(data));
    return U_CALLBACK_COMPLETE;
}

static int string_to_long(const char *s, long *result) {
    char *endptr;

    if (s == NULL || *s == '\0') {
        return 1;
    }
    *result = strtol(s, &endptr, 10);
    if (*endptr != '\0') {
        return 1;
    }
    return 0;
}

static int callback_get_hook_id(const struct _u_request *request, struct _u_response *response, void *user_data) {
    long hook_i;

    (void)user_data;

    if (string_to_long(u_map_get(request->map_url, "id"), &hook_i) != 0) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Invalid integer", NULL));
        return U_CALLBACK_COMPLETE;
    }

    if (hook_i < 0 || (size_t)hook_i >= hook_elements.size()) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Id out of range", NULL));
        return U_CALLBACK_COMPLETE;
    }

    ulfius_set_json_body_response(response, 200, wrap_result_success_json(get_symbol_as_json(hook_i)));
    return U_CALLBACK_COMPLETE;
}

static int callback_post_hook_id(const struct _u_request *request, struct _u_response *response, void *user_data) {
    long hook_i;
    int new_state;
    json_t *json_req_root;
    json_t *json_req_state;
    json_error_t json_error;

    (void)user_data;

    if (string_to_long(u_map_get(request->map_url, "id"), &hook_i) != 0) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Invalid integer", NULL));
        return U_CALLBACK_COMPLETE;
    }

    if (hook_i < 0 || (size_t)hook_i >= hook_elements.size()) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Out of range", NULL));
        return U_CALLBACK_COMPLETE;
    }

    json_req_root = ulfius_get_json_body_request(request, &json_error);
    if (json_req_root == NULL) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, json_error.text, NULL));
        return U_CALLBACK_COMPLETE;
    }

    if (!json_is_object(json_req_root)) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Expected a object", NULL));
        return U_CALLBACK_COMPLETE;
    }

    json_req_state = json_object_get(json_req_root, "state");

    if (!json_is_integer(json_req_state)) {
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "State must be an integer", NULL));
        return U_CALLBACK_COMPLETE;
    }

    new_state = (int)json_integer_value(json_req_state);

    switch (new_state) {
    case HOOK_DISABLED:
    case HOOK_ENABLED:
    case HOOK_UNAVAILABLE:
        break;
    default:
        ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Invalid hook state", NULL));
        return U_CALLBACK_COMPLETE;
    }

    if (new_state == HOOK_UNAVAILABLE) {
        if (*hook_elements[hook_i].state != HOOK_UNAVAILABLE) {
            ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Can't mark hook as unavailable", NULL));
            return U_CALLBACK_COMPLETE;
        }
    }

    if (*hook_elements[hook_i].state == HOOK_UNAVAILABLE) {
        if (new_state != HOOK_UNAVAILABLE) {
            ulfius_set_json_body_response(response, 400, wrap_result_json(1, "Can't mark unavailable hook as either enabled/disabled", NULL));
            return U_CALLBACK_COMPLETE;
        }
    }

    *hook_elements[hook_i].state = (function_hook_state_t)new_state;

    ulfius_set_json_body_response(response, 200, wrap_result_success_json(json_null()));
    return U_CALLBACK_COMPLETE;
}

int start_hook_webserver(int port) {
    fprintf(stderr, "webserver knows about %d states.\n", (int)hook_elements.size());

    if (ulfius_global_init() != U_OK) {
        fprintf(stderr, "ulfius_global_init failed.\n");
        return 1;
    }

    if (ulfius_init_instance(&ulfius_instance, port, NULL, NULL) != U_OK) {
        fprintf(stderr, "ulfius_init_instance failed.\n");
        return 1;
    }

    if (ulfius_add_endpoint_by_val(&ulfius_instance, "GET", NULL, "/helloworld", 0, &callback_get_helloworld, NULL) != U_OK) {
        fprintf(stderr, "Failed to add GET /helloworld\n");
    }
    if (ulfius_add_endpoint_by_val(&ulfius_instance, "GET", NULL, "/info", 0, &callback_get_info, NULL) != U_OK) {
        fprintf(stderr, "Failed to add GET /info\n");
    }
    if (ulfius_add_endpoint_by_val(&ulfius_instance, "GET", NULL, "/hooks", 0, &callback_get_hooks, NULL) != U_OK) {
        fprintf(stderr, "Failed to add GET /hooks\n");
    }
    if (ulfius_add_endpoint_by_val(&ulfius_instance, "GET", NULL, "/hook/:id", 0, &callback_get_hook_id, NULL) != U_OK) {
        fprintf(stderr, "Failed to add GET /hook/:id\n");
    }
    if (ulfius_add_endpoint_by_val(&ulfius_instance, "POST", NULL, "/hook/:id", 0, &callback_post_hook_id, NULL) != U_OK) {
        fprintf(stderr, "Failed to add POST /hook/:id\n");
    }

    // Start the framework
    if (ulfius_start_framework(&ulfius_instance) != U_OK) {
        printf("ulfius_start_framework failed\n");
        return 1;
    }
    printf("Start framework on port %d\n", ulfius_instance.port);

    return 0;
}

void stop_hook_webserver() {
    ulfius_stop_framework(&ulfius_instance);
    ulfius_clean_instance(&ulfius_instance);
    memset(&ulfius_instance, 0, sizeof(ulfius_instance));
    ulfius_global_close();
}

void webserver_register_variable(const char *name, function_hook_state_t *state, const char *file, int line) {
    hook_elements.push_back({name, state, file, line});
}

#else

int start_hook_webserver(int port) {
    printf("Hook webserver startup skipped => server not built-in\n"):
        return 1;
}

void stop_hook_webserver() {
}

void webserver_register_variable(const char *name, function_hook_state_t *state, const char *file, int line) {
}

#endif

}
