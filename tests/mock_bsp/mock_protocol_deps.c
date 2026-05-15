/* mock_protocol_deps.c — stubs for bms_protocol.c external dependencies (host tests). */
#include "bms_measurements.h"
#include "bms_state.h"
#include <string.h>

static CellSnapshot    s_cells;
static TempSnapshot    s_temps;
static PackMeasurement s_pack;
static BmsState        s_state = BMS_STATE_STANDBY;

const CellSnapshot    *bms_measurements_get_cells(void) { return &s_cells; }
const TempSnapshot    *bms_measurements_get_temps(void) { return &s_temps; }
const PackMeasurement *bms_measurements_get_pack(void)  { return &s_pack; }

BmsState bms_state_get(void)                     { return s_state; }
void     bms_state_request_bootloader_entry(void) {}
