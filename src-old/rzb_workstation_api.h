#ifndef RZB_WORKSTATION_API_H
#define RZB_WORKSTATION_API_H

#include <rzb_api_types.h>

typedef struct _WORKSTATIONAPI
{
    /* Standard */
    init_rzb_fp initRZB;
    fini_rzb_fp finiRZB;
    set_dbg_mode_fp setDebugMode;

    /* Workstation */
    get_route_table_fp getRouteTable;
    get_logs_by_num_fp getLogsByNum;
} WorkstationAPI;

extern const WorkstationAPI rzb_workstation;

#endif /* RZB_WORKSTATION_API_H */

