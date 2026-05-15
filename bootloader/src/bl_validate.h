/* bl_validate.h — firmware package header validation. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ── Firmware package header (64 bytes, packed, little-endian) ────────────── */
/* Must match protocol/firmware_package.yaml exactly. */
#pragma pack(push, 1)
typedef struct {
    uint32_t pkg_magic;             /* 0x00: 0xBF00BF00 */
    uint16_t pkg_version;           /* 0x04: package format version */
    uint16_t hw_profile_id;         /* 0x06: must match HW_PROFILE_ID */
    uint32_t target_mcu_id;         /* 0x08: DBGMCU_IDCODE (lower 12 bits = dev_id) */
    uint8_t  image_type;            /* 0x0C: 0x01 = application */
    uint8_t  _pad[3];               /* 0x0D: alignment */
    uint32_t app_start_addr;        /* 0x10: must == APP_START_ADDR */
    uint32_t app_size;              /* 0x14: bytes of application image */
    uint32_t app_crc32;             /* 0x18: CRC-32/ISO-HDLC of payload */
    uint8_t  fw_version[3];         /* 0x1C: semantic version of the firmware */
    uint8_t  min_bootloader_version[3]; /* 0x1F: minimum BL version required */
    uint16_t required_config_schema;/* 0x22: config schema version required */
    uint16_t _reserved;             /* 0x24: zero */
    uint32_t pkg_header_crc32;      /* 0x26: CRC-32 of header bytes [0x00..0x25] */
    uint8_t  _pad2[22];             /* 0x2A: padding to 64 bytes */
} FirmwarePackageHeader;
#pragma pack(pop)

_Static_assert(sizeof(FirmwarePackageHeader) == 64,
               "FirmwarePackageHeader must be exactly 64 bytes");

/* ── Validation result codes ──────────────────────────────────────────────── */
typedef enum {
    BL_VALIDATE_OK              = 0,
    BL_ERR_BAD_MAGIC            = 1,
    BL_ERR_PKG_VERSION          = 2,
    BL_ERR_HW_PROFILE           = 3,
    BL_ERR_MCU_ID               = 4,
    BL_ERR_IMAGE_TYPE           = 5,
    BL_ERR_APP_ADDR             = 6,
    BL_ERR_APP_SIZE             = 7,
    BL_ERR_HEADER_CRC           = 8,
    BL_ERR_BL_VERSION           = 9,
    BL_ERR_CONFIG_SCHEMA        = 10,
} BlValidateResult;

/* Validate a firmware package header in the order specified in docs/06.
 * mcu_dev_id: lower 12 bits from DBGMCU_IDCODE (read at runtime). */
BlValidateResult bl_validate_package_header(const FirmwarePackageHeader *hdr,
                                             uint32_t mcu_dev_id);

/* CRC-32/ISO-HDLC: poly 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF. */
uint32_t bl_crc32(const uint8_t *data, uint32_t len);
