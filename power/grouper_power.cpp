/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "powerhal.h"

static struct powerhal_info *pInfo;
static struct input_dev_map input_devs[] = {
		{-1, "elan-touchscreen\n"}
       };

static void grouper_power_init(struct power_module *module)
{
    if (!pInfo)
        pInfo = (powerhal_info*)calloc(1, sizeof(powerhal_info));

    pInfo->input_devs = input_devs;
    pInfo->input_cnt = sizeof(input_devs)/sizeof(struct input_dev_map);
    common_power_init(module, pInfo);
}

static void grouper_power_set_interactive(struct power_module *module, int on)
{
    common_power_set_interactive(module, pInfo, on);
}

static void grouper_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    common_power_hint(module, pInfo, hint, data);
}

static int grouper_power_open(__attribute__ ((unused)) const hw_module_t *module, const char *name,
                          __attribute__ ((unused)) hw_device_t **device)
{
    if (strcmp(name, POWER_HARDWARE_MODULE_ID))
        return -EINVAL;

    if (!pInfo) {
        pInfo = (powerhal_info*)calloc(1, sizeof(powerhal_info));

        common_power_open(pInfo);
    }

    return 0;
}

static void grouper_set_feature(__attribute__ ((unused)) struct power_module *module, feature_t feature, __attribute__ ((unused)) int state)
{
    switch (feature) {
    case POWER_FEATURE_DOUBLE_TAP_TO_WAKE:
        ALOGW("Double tap to wake is not supported\n");
        break;
    default:
        ALOGW("Error setting the feature, it doesn't exist %d\n", feature);
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    open: grouper_power_open,
};

struct power_module HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: POWER_MODULE_API_VERSION_0_3,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: POWER_HARDWARE_MODULE_ID,
        name: "Grouper Power HAL",
        author: "NVIDIA",
        methods: &power_module_methods,
        dso: NULL,
        reserved: {0},
    },

    init: grouper_power_init,
    setInteractive: grouper_power_set_interactive,
    powerHint: grouper_power_hint,
    setFeature: grouper_set_feature,
};
