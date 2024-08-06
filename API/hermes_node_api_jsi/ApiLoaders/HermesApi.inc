// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// The Hermes functions sorted alphabetically

#ifndef HERMES_FUNC
#define HERMES_FUNC(func)
#endif

#ifndef HERMES_INSPECTOR_FUNC
#define HERMES_INSPECTOR_FUNC(func)
#endif

HERMES_FUNC(hermes_dump_crash_data)
HERMES_FUNC(hermes_sampling_profiler_enable)
HERMES_FUNC(hermes_sampling_profiler_disable)
HERMES_FUNC(hermes_sampling_profiler_add)
HERMES_FUNC(hermes_sampling_profiler_remove)
HERMES_FUNC(hermes_sampling_profiler_dump_to_file)
HERMES_FUNC(hermes_config_enable_default_crash_handler)
HERMES_FUNC(hermes_set_inspector)

HERMES_INSPECTOR_FUNC(hermes_create_local_connection)
HERMES_INSPECTOR_FUNC(hermes_delete_local_connection)
HERMES_INSPECTOR_FUNC(hermes_local_connection_send_message)
HERMES_INSPECTOR_FUNC(hermes_local_connection_disconnect)

#undef HERMES_FUNC
#undef HERMES_INSPECTOR_FUNC
