/* board_can.c — bxCAN driver for STM32F303VC.
 *
 * Bus: 500 kbit/s, standard (11-bit) IDs only.
 * APB1 = 36 MHz → BRP=4, TS1=13, TS2=4, SJW=1 → 36M/(4×18) = 500 kbit/s
 * Sample point: (1+13)/18 = 77.8 %
 *
 * Filters: single filter bank 0, mask-mode, accept all IDs.
 * Transmit: mailbox polling, no interrupts.
 */
#include "board_can.h"
#include "board_pins.h"
#include "bms_hal.h"

/* ── Bit-timing register value ────────────────────────────────────────────── */
/* BTR[9:0]=BRP-1, BTR[19:16]=TS1-1, BTR[22:20]=TS2-1, BTR[25:24]=SJW-1 */
#define CAN_BTR_500KBPS  ((0u << 24) | (3u << 20) | (12u << 16) | 3u)

/* ── TSR mailbox-empty bits ───────────────────────────────────────────────── */
#define TSR_TME0  (1u << 26)
#define TSR_TME1  (1u << 27)
#define TSR_TME2  (1u << 28)

void board_can_init(void) {
    /* GPIO: PA11=CAN_RX, PA12=CAN_TX → AF9, push-pull, no pull */
    CAN_PORT->MODER &= ~((3u << (CAN_RX_PIN * 2u)) | (3u << (CAN_TX_PIN * 2u)));
    CAN_PORT->MODER |=  (GPIO_MODER_AF << (CAN_RX_PIN * 2u)) |
                        (GPIO_MODER_AF << (CAN_TX_PIN * 2u));
    CAN_PORT->AFR[1] &= ~(0xFu << ((CAN_RX_PIN - 8u) * 4u)) &
                         ~(0xFu << ((CAN_TX_PIN - 8u) * 4u));
    CAN_PORT->AFR[1] |=  (CAN_AF << ((CAN_RX_PIN - 8u) * 4u)) |
                         (CAN_AF << ((CAN_TX_PIN - 8u) * 4u));

    /* Enter init mode */
    CAN->MCR |= CAN_MCR_INRQ;
    CAN->MCR &= ~CAN_MCR_SLEEP;
    while (!(CAN->MSR & CAN_MSR_INAK)) { /* wait for INAK */ }

    /* ABOM: auto bus-off recovery; TXFP: mailboxes in FIFO order */
    CAN->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;
    CAN->MCR &= ~CAN_MCR_TTCM; /* time-triggered mode off */

    CAN->BTR = CAN_BTR_500KBPS;

    /* Filter bank 0: mask mode, 32-bit, accept all, assigned to FIFO 0 */
    CAN->FMR   |= CAN_FMR_FINIT;        /* enter filter init */
    CAN->FA1R  &= ~(1u << 0);           /* deactivate bank 0 */
    CAN->FS1R  |= (1u << 0);            /* 32-bit scale */
    CAN->FM1R  &= ~(1u << 0);           /* mask mode */
    CAN->FFA1R &= ~(1u << 0);           /* assign to FIFO 0 */
    CAN->sFilterRegister[0].FR1 = 0u;   /* id = 0 */
    CAN->sFilterRegister[0].FR2 = 0u;   /* mask = 0 → accept all */
    CAN->FA1R  |= (1u << 0);            /* activate bank 0 */
    CAN->FMR   &= ~CAN_FMR_FINIT;       /* leave filter init */

    /* Leave init mode */
    CAN->MCR &= ~CAN_MCR_INRQ;
    while (CAN->MSR & CAN_MSR_INAK) { /* wait for normal mode */ }
}

BmsResult board_can_send(uint32_t id, const uint8_t *data, uint8_t len) {
    if (len > 8u) { len = 8u; }

    /* Find a free mailbox (spin up to ~72000 iterations ≈ 1 ms at 72 MHz) */
    uint8_t  mbox = 0xFFu;
    uint32_t deadline = 72000u;
    while (deadline--) {
        uint32_t tsr = CAN->TSR;
        if (tsr & TSR_TME0)      { mbox = 0u; break; }
        else if (tsr & TSR_TME1) { mbox = 1u; break; }
        else if (tsr & TSR_TME2) { mbox = 2u; break; }
    }
    if (mbox == 0xFFu) { return BMS_ERR_TIMEOUT; }

    /* Load frame: standard ID, data frame */
    CAN->sTxMailBox[mbox].TIR  = (id << 21u);   /* STID[10:0], RTR=0, IDE=0 */
    CAN->sTxMailBox[mbox].TDTR = (uint32_t)len;
    CAN->sTxMailBox[mbox].TDLR = ((uint32_t)data[0])        |
                                  ((uint32_t)data[1] << 8u)  |
                                  ((uint32_t)(len > 2u ? data[2] : 0u) << 16u) |
                                  ((uint32_t)(len > 3u ? data[3] : 0u) << 24u);
    CAN->sTxMailBox[mbox].TDHR = ((uint32_t)(len > 4u ? data[4] : 0u))        |
                                  ((uint32_t)(len > 5u ? data[5] : 0u) << 8u)  |
                                  ((uint32_t)(len > 6u ? data[6] : 0u) << 16u) |
                                  ((uint32_t)(len > 7u ? data[7] : 0u) << 24u);

    /* Request transmission */
    CAN->sTxMailBox[mbox].TIR |= CAN_TI0R_TXRQ;
    return BMS_OK;
}
