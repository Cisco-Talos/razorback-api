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
#include <razorback/debug.h>
#include <razorback/nugget.h>

SO_PUBLIC void
Nugget_Destroy(struct Nugget *p_pNugget)
{
    if (p_pNugget->sName != NULL)
        free(p_pNugget->sName);

    if (p_pNugget->sLocation !=NULL)
        free(p_pNugget->sLocation);

    if (p_pNugget->sContact != NULL)
        free(p_pNugget->sContact);

    if (p_pNugget->sNotes !=NULL)
        free(p_pNugget->sNotes);

    free(p_pNugget);
}

