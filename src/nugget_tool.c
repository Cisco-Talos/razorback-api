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

#include "nugget_tool.h"

#include <razorback/log.h>

#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <dlfcn.h>
#endif

bool
NuggetTool_LoadModule(const char *path, struct NuggetToolModule *module)
{
    if (path == NULL || module == NULL)
        return false;

    memset(module, 0, sizeof(*module));
    module->path = strdup(path);
    if (module->path == NULL)
        return false;

#ifdef _MSC_VER
    module->handle = LoadLibraryA(path);
    if (module->handle == NULL) {
        rzb_log(LOG_ERR, LOG_C_NUGGET,
                "%s: Failed to load nugget module '%s' (%lu)",
                __func__, path, GetLastError());
        goto error;
    }

    *(void **)&module->initNug = GetProcAddress(module->handle, "initNug");
    *(void **)&module->shutdownNug = GetProcAddress(module->handle, "shutdownNug");
#else
    module->handle = dlopen(path, RTLD_LOCAL | RTLD_NOW);
    if (module->handle == NULL) {
        rzb_log(LOG_ERR, LOG_C_NUGGET,
                "%s: Failed to load nugget module '%s': %s",
                __func__, path, dlerror());
        goto error;
    }

    *(void **)&module->initNug = dlsym(module->handle, "initNug");
    *(void **)&module->shutdownNug = dlsym(module->handle, "shutdownNug");
#endif

    if (module->initNug == NULL || module->shutdownNug == NULL) {
        rzb_log(LOG_ERR, LOG_C_NUGGET,
                "%s: Nugget module '%s' is missing initNug/shutdownNug",
                __func__, path);
        goto error;
    }

    return true;

error:
    NuggetTool_UnloadModule(module);
    return false;
}

void
NuggetTool_UnloadModule(struct NuggetToolModule *module)
{
    if (module == NULL)
        return;

#ifdef _MSC_VER
    if (module->handle != NULL)
        FreeLibrary(module->handle);
#else
    if (module->handle != NULL)
        dlclose(module->handle);
#endif

    free(module->path);
    memset(module, 0, sizeof(*module));
}
