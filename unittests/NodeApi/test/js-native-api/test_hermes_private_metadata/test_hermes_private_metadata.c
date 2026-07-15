#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static int wrapped_value = 42;
static int finalize_count = 0;
static const napi_type_tag test_tag = {
    0x1edf75a38336451dULL,
    0xa5ed9ce2e4c00c38ULL,
};

static void CountFinalizer(napi_env env, void* data, void* hint) {
  finalize_count++;
}

static napi_value GetFinalizeCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_create_int32(env, finalize_count, &result));
  return result;
}

static napi_value WrapRoundTrip(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value object;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &object, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Expected one object.");

  NODE_API_CALL(env, napi_wrap(env, object, &wrapped_value, NULL, NULL, NULL));

  void* unwrapped = NULL;
  NODE_API_CALL(env, napi_unwrap(env, object, &unwrapped));
  NODE_API_ASSERT(
      env, unwrapped == &wrapped_value, "Unexpected wrapped value.");

  void* removed = NULL;
  NODE_API_CALL(env, napi_remove_wrap(env, object, &removed));

  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, removed == &wrapped_value, &result));
  return result;
}

static napi_value WrapPrototypeIsolation(napi_env env,
                                         napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 2, "Expected parent and child objects.");

  NODE_API_CALL(env, napi_wrap(env, args[0], &wrapped_value, NULL, NULL, NULL));

  void* child_data = NULL;
  napi_status child_status = napi_unwrap(env, args[1], &child_data);

  void* parent_data = NULL;
  NODE_API_CALL(env, napi_remove_wrap(env, args[0], &parent_data));

  napi_value result;
  NODE_API_CALL(
      env,
      napi_get_boolean(env,
                       child_status == napi_invalid_arg && child_data == NULL &&
                           parent_data == &wrapped_value,
                       &result));
  return result;
}

static napi_value AddFinalizer(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value object;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &object, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Expected one object.");

  NODE_API_CALL(
      env, napi_add_finalizer(env, object, NULL, CountFinalizer, NULL, NULL));
  return NULL;
}

static napi_value TagWrapAndCheck(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value object;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &object, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Expected one object.");

  NODE_API_CALL(env, napi_type_tag_object(env, object, &test_tag));
  NODE_API_CALL(env, napi_wrap(env, object, &wrapped_value, NULL, NULL, NULL));

  bool matches = false;
  NODE_API_CALL(env,
                napi_check_object_type_tag(env, object, &test_tag, &matches));

  void* removed = NULL;
  NODE_API_CALL(env, napi_remove_wrap(env, object, &removed));

  napi_value result;
  NODE_API_CALL(
      env,
      napi_get_boolean(env, matches && removed == &wrapped_value, &result));
  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_GETTER("finalizeCount", GetFinalizeCount),
      DECLARE_NODE_API_PROPERTY("wrapRoundTrip", WrapRoundTrip),
      DECLARE_NODE_API_PROPERTY("wrapPrototypeIsolation",
                                WrapPrototypeIsolation),
      DECLARE_NODE_API_PROPERTY("addFinalizer", AddFinalizer),
      DECLARE_NODE_API_PROPERTY("tagWrapAndCheck", TagWrapAndCheck),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));
  return exports;
}
EXTERN_C_END
