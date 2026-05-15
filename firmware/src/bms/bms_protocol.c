/* bms_protocol.c — UART protocol framing, CRC, and packet dispatch. */
#include "bms_protocol.h"
#include "bms_protocol_ids.h"
#include "bms_config.h"
#include "bms_measurements.h"
#include "bms_faults.h"
#include "bms_outputs.h"
#include "bms_state.h"
#include "board_uart.h"
#include "board_clock.h"
#include <string.h>

/* ── CRC-16/CCITT-FALSE ──────────────────────────────────────────────────── */
uint16_t bms_protocol_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8u);
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1u) ^ 0x1021u) : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

/* ── Frame RX state machine ───────────────────────────────────────────────── */
typedef enum {
    RX_SOF0, RX_SOF1, RX_PKT_ID_LO, RX_PKT_ID_HI,
    RX_FLAGS, RX_SEQ, RX_LEN_LO, RX_LEN_HI, RX_PAYLOAD, RX_CRC_LO, RX_CRC_HI
} RxState;

#define RX_BUF_SIZE  (PROTOCOL_MAX_PAYLOAD + FRAME_OVERHEAD)

static struct {
    RxState  state;
    uint8_t  buf[RX_BUF_SIZE];
    uint16_t buf_pos;
    uint16_t pkt_id;
    uint8_t  flags;
    uint8_t  seq;
    uint16_t payload_len;
} s_rx;

static uint8_t s_tx_buf[RX_BUF_SIZE];

/* ── Response helpers ─────────────────────────────────────────────────────── */
static void send_response(uint16_t pkt_id, uint8_t seq,
                           const uint8_t *payload, uint16_t payload_len,
                           bool is_error) {
    uint16_t frame_len = FRAME_OVERHEAD + payload_len;
    s_tx_buf[0] = FRAME_SOF_0;
    s_tx_buf[1] = FRAME_SOF_1;
    s_tx_buf[2] = (uint8_t)(pkt_id & 0xFFu);
    s_tx_buf[3] = (uint8_t)(pkt_id >> 8u);
    s_tx_buf[4] = PROTOCOL_FLAGS_IS_RESPONSE | (is_error ? PROTOCOL_FLAGS_IS_ERROR : 0u);
    s_tx_buf[5] = seq;
    s_tx_buf[6] = (uint8_t)(payload_len & 0xFFu);
    s_tx_buf[7] = (uint8_t)(payload_len >> 8u);
    if (payload && payload_len) {
        memcpy(&s_tx_buf[8], payload, payload_len);
    }
    uint16_t crc = bms_protocol_crc16(s_tx_buf, (uint16_t)(frame_len - 2u));
    s_tx_buf[frame_len - 2u] = (uint8_t)(crc >> 8u);
    s_tx_buf[frame_len - 1u] = (uint8_t)(crc & 0xFFu);
    board_uart_write(s_tx_buf, frame_len);
}

static void send_error(uint16_t pkt_id, uint8_t seq, ProtoError err) {
    uint8_t payload = (uint8_t)err;
    send_response(pkt_id, seq, &payload, 1u, true);
}

/* ── Packet handlers ──────────────────────────────────────────────────────── */
static void handle_get_capabilities(uint8_t seq) {
    uint8_t resp[PKT_CAPABILITIES_RESP_SIZE];
    memset(resp, 0, sizeof(resp));
    resp[0]  = (uint8_t)(FIRMWARE_TYPE_BMS_APP & 0xFFu);
    resp[1]  = (uint8_t)(FIRMWARE_TYPE_BMS_APP >> 8u);
    resp[2]  = FW_VERSION_MAJOR;
    resp[3]  = FW_VERSION_MINOR;
    resp[4]  = FW_VERSION_PATCH;
    resp[5]  = (uint8_t)(HW_PROFILE_ID & 0xFFu);
    resp[6]  = (uint8_t)(HW_PROFILE_ID >> 8u);
    resp[7]  = (uint8_t)(PROTOCOL_VERSION & 0xFFu);
    resp[8]  = (uint8_t)(PROTOCOL_VERSION >> 8u);
    resp[9]  = (uint8_t)(CONFIG_SCHEMA_VERSION & 0xFFu);
    resp[10] = (uint8_t)(CONFIG_SCHEMA_VERSION >> 8u);
    resp[11] = (uint8_t)TOTAL_CELL_COUNT;
    resp[12] = (uint8_t)TOTAL_TEMP_COUNT;
    uint32_t ff = BMS_APP_FEATURE_FLAGS;
    resp[13] = (uint8_t)(ff); resp[14] = (uint8_t)(ff >> 8);
    resp[15] = (uint8_t)(ff >> 16); resp[16] = (uint8_t)(ff >> 24);
    resp[17] = PROTOCOL_MAX_PAYLOAD_LOG2;
    uint32_t app_sz = APP_REGION_SIZE;
    resp[18] = (uint8_t)app_sz; resp[19] = (uint8_t)(app_sz>>8);
    resp[20] = (uint8_t)(app_sz>>16); resp[21] = (uint8_t)(app_sz>>24);
    uint32_t cfg_sz = CONFIG_SLOT_SIZE;
    resp[22] = (uint8_t)cfg_sz; resp[23] = (uint8_t)(cfg_sz>>8);
    resp[24] = (uint8_t)(cfg_sz>>16); resp[25] = (uint8_t)(cfg_sz>>24);
    send_response(PKT_GET_CAPABILITIES, seq, resp, sizeof(resp), false);
}

static void handle_get_values(uint8_t seq) {
    const PackMeasurement *pack  = bms_measurements_get_pack();
    uint8_t resp[PKT_VALUES_RESP_SIZE];
    memset(resp, 0, sizeof(resp));
    uint32_t vbat = (uint32_t)pack->vbat_mv;
    resp[0]=(uint8_t)vbat; resp[1]=(uint8_t)(vbat>>8); resp[2]=(uint8_t)(vbat>>16); resp[3]=(uint8_t)(vbat>>24);
    uint32_t vpk = (uint32_t)pack->vpack_mv;
    resp[4]=(uint8_t)vpk; resp[5]=(uint8_t)(vpk>>8); resp[6]=(uint8_t)(vpk>>16); resp[7]=(uint8_t)(vpk>>24);
    uint32_t ib = (uint32_t)pack->i_batt_ma;
    resp[8]=(uint8_t)ib; resp[9]=(uint8_t)(ib>>8); resp[10]=(uint8_t)(ib>>16); resp[11]=(uint8_t)(ib>>24);
    uint16_t st = (uint16_t)bms_state_get();
    resp[12]=(uint8_t)st; resp[13]=(uint8_t)(st>>8);
    uint64_t af = bms_faults_get_active();
    for (int i=0;i<8;i++) { resp[14+i]=(uint8_t)(af>>(8*i)); }
    uint64_t lf = bms_faults_get_latched();
    for (int i=0;i<8;i++) { resp[22+i]=(uint8_t)(lf>>(8*i)); }
    resp[30] = bms_outputs_get_state();
    uint32_t up = board_clock_get_ms();
    resp[31]=(uint8_t)up; resp[32]=(uint8_t)(up>>8); resp[33]=(uint8_t)(up>>16); resp[34]=(uint8_t)(up>>24);
    uint8_t mf = (pack->vbat_valid?4u:0u)|(pack->vpack_valid?8u:0u);
    resp[35] = mf;
    send_response(PKT_GET_VALUES, seq, resp, sizeof(resp), false);
}

static void handle_get_cells(uint8_t seq, const uint8_t *payload, uint16_t len) {
    bool include_validity = (len > 0 && (payload[0] & 0x01u));
    const CellSnapshot *cells = bms_measurements_get_cells();
    uint16_t resp_len = include_validity ? PKT_GET_CELLS_RESP_FULL : PKT_GET_CELLS_RESP_BASE;
    uint8_t resp[PKT_GET_CELLS_RESP_FULL];
    memset(resp, 0, sizeof(resp));
    resp[0] = TOTAL_CELL_COUNT & 0xFFu;
    resp[1] = (TOTAL_CELL_COUNT >> 8u) & 0xFFu;
    for (uint8_t i = 0; i < TOTAL_CELL_COUNT; i++) {
        uint16_t mv = cells->mv[i];
        resp[2 + i*2]   = (uint8_t)(mv & 0xFFu);
        resp[2 + i*2+1] = (uint8_t)(mv >> 8u);
    }
    uint32_t ts = cells->timestamp_ms;
    resp[152]=(uint8_t)ts; resp[153]=(uint8_t)(ts>>8); resp[154]=(uint8_t)(ts>>16); resp[155]=(uint8_t)(ts>>24);
    if (include_validity) {
        for (uint8_t i = 0; i < TOTAL_CELL_COUNT; i++) {
            if (cells->valid[i]) { resp[156 + i/8] |= (uint8_t)(1u << (i%8u)); }
        }
    }
    send_response(PKT_GET_CELLS, seq, resp, resp_len, false);
}

static void handle_get_temps(uint8_t seq) {
    const TempSnapshot *temps = bms_measurements_get_temps();
    uint8_t resp[PKT_GET_TEMPS_RESP_SIZE];
    memset(resp, 0, sizeof(resp));
    resp[0] = TOTAL_TEMP_COUNT & 0xFFu;
    resp[1] = 0u;
    for (uint8_t i = 0; i < TOTAL_TEMP_COUNT; i++) {
        int16_t t = temps->cx10[i];
        resp[2 + i*2]   = (uint8_t)((uint16_t)t & 0xFFu);
        resp[2 + i*2+1] = (uint8_t)((uint16_t)t >> 8u);
    }
    send_response(PKT_GET_TEMPS, seq, resp, sizeof(resp), false);
}

static void handle_get_faults(uint8_t seq) {
    uint8_t resp[16];
    uint64_t af = bms_faults_get_active();
    uint64_t lf = bms_faults_get_latched();
    for (int i=0;i<8;i++) { resp[i]=(uint8_t)(af>>(8*i)); }
    for (int i=0;i<8;i++) { resp[8+i]=(uint8_t)(lf>>(8*i)); }
    send_response(PKT_GET_FAULTS, seq, resp, sizeof(resp), false);
}

static void handle_get_config(uint8_t seq) {
    const BmsConfig *cfg = bms_config_get();
    send_response(PKT_GET_CONFIG, seq, (const uint8_t *)cfg, CONFIG_SCHEMA_SIZE, false);
}

static void handle_validate_config(uint8_t seq, const uint8_t *payload, uint16_t len) {
    if (len != CONFIG_SCHEMA_SIZE) { send_error(PKT_VALIDATE_CONFIG, seq, PROTO_ERR_BAD_LENGTH); return; }
    uint16_t err_off;
    uint8_t resp[3] = {0, 0xFF, 0xFF};
    BmsResult r = bms_config_validate((const BmsConfig *)payload, &err_off);
    resp[0] = (r == BMS_OK) ? 0u : 1u;
    resp[1] = (uint8_t)(err_off & 0xFFu);
    resp[2] = (uint8_t)(err_off >> 8u);
    send_response(PKT_VALIDATE_CONFIG, seq, resp, sizeof(resp), false);
}

static void handle_set_config_ram(uint8_t seq, const uint8_t *payload, uint16_t len) {
    if (len != CONFIG_SCHEMA_SIZE) { send_error(PKT_SET_CONFIG_RAM, seq, PROTO_ERR_BAD_LENGTH); return; }
    uint16_t err_off;
    BmsResult r = bms_config_apply_ram((const BmsConfig *)payload);
    uint8_t resp[3] = {(r==BMS_OK)?0u:1u, 0xFF, 0xFF};
    if (r != BMS_OK) {
        bms_config_validate((const BmsConfig *)payload, &err_off);
        resp[1] = (uint8_t)(err_off & 0xFFu);
        resp[2] = (uint8_t)(err_off >> 8u);
    }
    send_response(PKT_SET_CONFIG_RAM, seq, resp, sizeof(resp), false);
}

static void handle_store_config(uint8_t seq, const uint8_t *payload, uint16_t len) {
    if (len != CONFIG_SCHEMA_SIZE) { send_error(PKT_STORE_CONFIG, seq, PROTO_ERR_BAD_LENGTH); return; }
    BmsResult r = bms_config_store((const BmsConfig *)payload);
    uint8_t resp = (r == BMS_OK) ? 0u : (uint8_t)PROTO_ERR_CONFIG_INVALID;
    send_response(PKT_STORE_CONFIG, seq, &resp, 1u, r != BMS_OK);
}

static void handle_enter_bootloader(uint8_t seq, const uint8_t *payload, uint16_t len) {
    if (len < 4) { send_error(PKT_ENTER_BOOTLOADER, seq, PROTO_ERR_BAD_LENGTH); return; }
    uint32_t magic = (uint32_t)payload[0] | ((uint32_t)payload[1]<<8) |
                     ((uint32_t)payload[2]<<16) | ((uint32_t)payload[3]<<24);
    if (magic != BL_ENTRY_FLAG) { send_error(PKT_ENTER_BOOTLOADER, seq, PROTO_ERR_BAD_STATE); return; }
    /* Ack before reset */
    send_response(PKT_ENTER_BOOTLOADER, seq, NULL, 0u, false);
    bms_state_request_bootloader_entry();
}

/* ── Frame dispatch ───────────────────────────────────────────────────────── */
static void dispatch_packet(uint16_t pkt_id, uint8_t seq,
                             const uint8_t *payload, uint16_t payload_len) {
    switch (pkt_id) {
        case PKT_GET_CAPABILITIES:   handle_get_capabilities(seq); break;
        case PKT_GET_VALUES:         handle_get_values(seq); break;
        case PKT_GET_CELLS:          handle_get_cells(seq, payload, payload_len); break;
        case PKT_GET_TEMPS:          handle_get_temps(seq); break;
        case PKT_GET_FAULTS:         handle_get_faults(seq); break;
        case PKT_GET_CONFIG:         handle_get_config(seq); break;
        case PKT_VALIDATE_CONFIG:    handle_validate_config(seq, payload, payload_len); break;
        case PKT_SET_CONFIG_RAM:     handle_set_config_ram(seq, payload, payload_len); break;
        case PKT_STORE_CONFIG:       handle_store_config(seq, payload, payload_len); break;
        case PKT_ENTER_BOOTLOADER:   handle_enter_bootloader(seq, payload, payload_len); break;
        default:
            send_error(pkt_id, seq, PROTO_ERR_UNKNOWN_PACKET);
            break;
    }
}

/* ── RX tick ──────────────────────────────────────────────────────────────── */
void bms_protocol_init(void) {
    s_rx.state = RX_SOF0;
    s_rx.buf_pos = 0;
}

void bms_protocol_tick(void) {
    while (board_uart_rx_ready()) {
        uint8_t b = board_uart_read_byte();
        switch (s_rx.state) {
            case RX_SOF0:
                if (b == FRAME_SOF_0) {
                    s_rx.buf[0] = b; s_rx.buf_pos = 1;
                    s_rx.state = RX_SOF1;
                }
                break;
            case RX_SOF1:
                if (b == FRAME_SOF_1) { s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_PKT_ID_LO; }
                else { s_rx.state = RX_SOF0; }
                break;
            case RX_PKT_ID_LO:
                s_rx.pkt_id = b; s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_PKT_ID_HI; break;
            case RX_PKT_ID_HI:
                s_rx.pkt_id |= (uint16_t)b << 8u; s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_FLAGS; break;
            case RX_FLAGS:
                s_rx.flags = b; s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_SEQ; break;
            case RX_SEQ:
                s_rx.seq = b; s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_LEN_LO; break;
            case RX_LEN_LO:
                s_rx.payload_len = b; s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_LEN_HI; break;
            case RX_LEN_HI:
                s_rx.payload_len |= (uint16_t)b << 8u; s_rx.buf[s_rx.buf_pos++] = b;
                if (s_rx.payload_len > PROTOCOL_MAX_PAYLOAD) {
                    s_rx.state = RX_SOF0; /* frame too large, discard */
                } else {
                    s_rx.state = (s_rx.payload_len > 0) ? RX_PAYLOAD : RX_CRC_LO;
                }
                break;
            case RX_PAYLOAD:
                s_rx.buf[s_rx.buf_pos++] = b;
                if (s_rx.buf_pos >= (uint16_t)(FRAME_OVERHEAD - 2u + s_rx.payload_len)) {
                    s_rx.state = RX_CRC_LO;
                }
                break;
            case RX_CRC_LO:
                s_rx.buf[s_rx.buf_pos++] = b; s_rx.state = RX_CRC_HI; break;
            case RX_CRC_HI: {
                s_rx.buf[s_rx.buf_pos++] = b;
                uint16_t frame_len = s_rx.buf_pos;
                uint16_t recv_crc  = ((uint16_t)s_rx.buf[frame_len-2] << 8u) | s_rx.buf[frame_len-1];
                uint16_t calc_crc  = bms_protocol_crc16(s_rx.buf, (uint16_t)(frame_len - 2u));
                if (recv_crc == calc_crc) {
                    const uint8_t *payload = &s_rx.buf[FRAME_OVERHEAD - 2u];
                    dispatch_packet(s_rx.pkt_id, s_rx.seq, payload, s_rx.payload_len);
                } else {
                    send_error(s_rx.pkt_id, s_rx.seq, PROTO_ERR_BAD_CRC);
                }
                s_rx.state = RX_SOF0;
                break;
            }
        }
    }
}
