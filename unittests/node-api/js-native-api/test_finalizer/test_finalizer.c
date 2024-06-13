#include <assert.h>
#include <js_native_api.h>
#include <stdlib.h>
#include "../common.h"

static int test_value = 1;
static int finalize_count = 0;

static napi_value GetFinalizeCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_create_int32(env, finalize_count, &result));
  return result;
}

static void FinalizeExternal(napi_env env, void *data, void *hint) {
  int *actual_value = (int *)data;
  NODE_API_ASSERT_RETURN_VOID(
      env,
      actual_value == &test_value,
      "The correct pointer was passed to the finalizer");
  finalize_count++;
}

static napi_value AddPropertyWithFinalizer(
    napi_env env,
    napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Expected one argument.");

  napi_valuetype argtype;
  NODE_API_CALL(env, napi_typeof(env, arg, &argtype));

  NODE_API_ASSERT(env, argtype == napi_object, "Expected an object value.");

  napi_value external_value;
  NODE_API_CALL(
      env,
      napi_create_external(
          env,
          &test_value,
          FinalizeExternal,
          NULL, /* finalize_hint */
          &external_value));

  NODE_API_CALL(
      env, napi_set_named_property(env, arg, "External", external_value));

  return NULL;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_GETTER("finalizeCount", GetFinalizeCount),
      DECLARE_NODE_API_PROPERTY(
          "addPropertyWithFinalizer", AddPropertyWithFinalizer),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env,
          exports,
          sizeof(descriptors) / sizeof(*descriptors),
          descriptors));

  return exports;
}
EXTERN_C_END
