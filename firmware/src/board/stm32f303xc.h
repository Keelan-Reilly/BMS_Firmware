/* stm32f303xc.h — minimal STM32F303xC peripheral register definitions.
 *
 * In a full build this would be replaced by the official STM32CubeF3 CMSIS
 * header (cmsis-device-f3 repo, stm32f303xc.h from ST).
 *
 * This file contains only the registers used by this skeleton. Add remaining
 * peripheral definitions as needed. All register addresses from RM0316 rev 8.
 */
#pragma once
#include <stdint.h>

/* ── Core types ───────────────────────────────────────────────────────────── */
typedef volatile uint32_t __IO;

/* ── Base addresses ───────────────────────────────────────────────────────── */
#define PERIPH_BASE         0x40000000u
#define APB1PERIPH_BASE     PERIPH_BASE
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000u)
#define AHB1PERIPH_BASE     (PERIPH_BASE + 0x00020000u)
#define AHB2PERIPH_BASE     0x48000000u

/* ── GPIO ─────────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t MODER;    /*!< 0x00 mode register */
    __IO uint32_t OTYPER;   /*!< 0x04 output type */
    __IO uint32_t OSPEEDR;  /*!< 0x08 output speed */
    __IO uint32_t PUPDR;    /*!< 0x0C pull-up/pull-down */
    __IO uint32_t IDR;      /*!< 0x10 input data */
    __IO uint32_t ODR;      /*!< 0x14 output data */
    __IO uint32_t BSRR;     /*!< 0x18 bit set/reset */
    __IO uint32_t LCKR;     /*!< 0x1C configuration lock */
    __IO uint32_t AFR[2];   /*!< 0x20 alternate function low/high */
    __IO uint32_t BRR;      /*!< 0x28 bit reset */
} GPIO_TypeDef;

#define GPIOA_BASE  (AHB2PERIPH_BASE + 0x0000u)
#define GPIOB_BASE  (AHB2PERIPH_BASE + 0x0400u)
#define GPIOC_BASE  (AHB2PERIPH_BASE + 0x0800u)
#define GPIOD_BASE  (AHB2PERIPH_BASE + 0x0C00u)
#define GPIOF_BASE  (AHB2PERIPH_BASE + 0x1400u)

#define GPIOA  ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB  ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC  ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD  ((GPIO_TypeDef *)GPIOD_BASE)
#define GPIOF  ((GPIO_TypeDef *)GPIOF_BASE)

/* GPIO MODER field values */
#define GPIO_MODER_INPUT    0x0u
#define GPIO_MODER_OUTPUT   0x1u
#define GPIO_MODER_AF       0x2u
#define GPIO_MODER_ANALOG   0x3u

/* ── RCC ──────────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CR;       /*!< 0x00 clock control */
    __IO uint32_t CFGR;     /*!< 0x04 clock configuration */
    __IO uint32_t CIR;      /*!< 0x08 clock interrupt */
    __IO uint32_t APB2RSTR; /*!< 0x0C APB2 peripheral reset */
    __IO uint32_t APB1RSTR; /*!< 0x10 APB1 peripheral reset */
    __IO uint32_t AHBENR;   /*!< 0x14 AHB peripheral clock enable */
    __IO uint32_t APB2ENR;  /*!< 0x18 APB2 peripheral clock enable */
    __IO uint32_t APB1ENR;  /*!< 0x1C APB1 peripheral clock enable */
    __IO uint32_t BDCR;     /*!< 0x20 backup domain control */
    __IO uint32_t CSR;      /*!< 0x24 control/status */
    __IO uint32_t AHBRSTR;  /*!< 0x28 AHB peripheral reset */
    __IO uint32_t CFGR2;    /*!< 0x2C clock configuration 2 */
    __IO uint32_t CFGR3;    /*!< 0x30 clock configuration 3 */
} RCC_TypeDef;

#define RCC_BASE  (AHB1PERIPH_BASE + 0x1000u)
#define RCC       ((RCC_TypeDef *)RCC_BASE)

/* RCC CR bits */
#define RCC_CR_HSEON        (1u << 16)
#define RCC_CR_HSERDY       (1u << 17)
#define RCC_CR_PLLON        (1u << 24)
#define RCC_CR_PLLRDY       (1u << 25)

/* RCC CFGR SW bits */
#define RCC_CFGR_SW_PLL     (0x2u)
#define RCC_CFGR_SWS_PLL    (0x8u)
#define RCC_CFGR_SWS_Msk    (0xCu)

/* RCC AHBENR bits (GPIO) */
#define RCC_AHBENR_GPIOAEN  (1u << 17)
#define RCC_AHBENR_GPIOBEN  (1u << 18)
#define RCC_AHBENR_GPIOCEN  (1u << 19)
#define RCC_AHBENR_GPIOFEN  (1u << 22)

/* RCC APB1ENR bits */
#define RCC_APB1ENR_USART2EN  (1u << 17)
#define RCC_APB1ENR_I2C2EN    (1u << 22)
#define RCC_APB1ENR_CANEN     (1u << 25)
#define RCC_APB1ENR_PWREN     (1u << 28)

/* RCC APB2ENR bits */
#define RCC_APB2ENR_SPI1EN    (1u << 12)
#define RCC_APB2ENR_ADC12EN   (1u << 9)

/* ── USART ────────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t CR3;
    __IO uint32_t BRR;
    __IO uint32_t GTPR;
    __IO uint32_t RTOR;
    __IO uint32_t RQR;
    __IO uint32_t ISR;
    __IO uint32_t ICR;
    __IO uint32_t RDR;
    __IO uint32_t TDR;
} USART_TypeDef;

#define USART2_BASE  (APB1PERIPH_BASE + 0x4400u)
#define USART2       ((USART_TypeDef *)USART2_BASE)

#define USART_CR1_UE        (1u << 0)
#define USART_CR1_RE        (1u << 2)
#define USART_CR1_TE        (1u << 3)
#define USART_CR1_RXNEIE    (1u << 5)
#define USART_ISR_RXNE      (1u << 5)
#define USART_ISR_TXE       (1u << 7)
#define USART_ISR_TC        (1u << 6)

/* ── SPI ──────────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t CRCPR;
    __IO uint32_t RXCRCR;
    __IO uint32_t TXCRCR;
    __IO uint32_t I2SCFGR;
    __IO uint32_t I2SPR;
} SPI_TypeDef;

#define SPI1_BASE  (APB2PERIPH_BASE + 0x3000u)
#define SPI1       ((SPI_TypeDef *)SPI1_BASE)

#define SPI_CR1_CPHA    (1u << 0)
#define SPI_CR1_CPOL    (1u << 1)
#define SPI_CR1_MSTR    (1u << 2)
#define SPI_CR1_BR_Pos  3u
#define SPI_CR1_SPE     (1u << 6)
#define SPI_CR1_SSM     (1u << 9)
#define SPI_CR1_SSI     (1u << 8)
#define SPI_CR1_BIDIMODE (1u << 15)
#define SPI_SR_RXNE     (1u << 0)
#define SPI_SR_TXE      (1u << 1)
#define SPI_SR_BSY      (1u << 7)

/* ── IWDG ─────────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t KR;
    __IO uint32_t PR;
    __IO uint32_t RLR;
    __IO uint32_t SR;
    __IO uint32_t WINR;
} IWDG_TypeDef;

#define IWDG_BASE  0x40003000u
#define IWDG       ((IWDG_TypeDef *)IWDG_BASE)
#define IWDG_KR_RELOAD  0xAAAAu
#define IWDG_KR_ENABLE  0xCCCCu
#define IWDG_KR_UNLOCK  0x5555u

/* ── RTC (for boot flag in BKP0R) ────────────────────────────────────────── */
typedef struct {
    __IO uint32_t TR;
    __IO uint32_t DR;
    __IO uint32_t CR;
    __IO uint32_t ISR;
    __IO uint32_t PRER;
    __IO uint32_t WUTR;
    uint32_t      RESERVED0;
    __IO uint32_t ALRMAR;
    __IO uint32_t ALRMBR;
    __IO uint32_t WPR;
    __IO uint32_t SSR;
    __IO uint32_t SHIFTR;
    __IO uint32_t TSTR;
    __IO uint32_t TSDR;
    __IO uint32_t TSSSR;
    __IO uint32_t CALR;
    __IO uint32_t TAFCR;
    __IO uint32_t ALRMASSR;
    __IO uint32_t ALRMBSSR;
    uint32_t      RESERVED1;
    __IO uint32_t BKP0R;   /* Backup register 0 — used for boot flag */
} RTC_TypeDef;

#define RTC_BASE  (APB1PERIPH_BASE + 0x2800u)
#define RTC       ((RTC_TypeDef *)RTC_BASE)

/* ── Flash registers ─────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t ACR;
    __IO uint32_t KEYR;
    __IO uint32_t OPTKEYR;
    __IO uint32_t SR;
    __IO uint32_t CR;
    __IO uint32_t AR;
    uint32_t      RESERVED;
    __IO uint32_t OBR;
    __IO uint32_t WRPR;
} FLASH_TypeDef;

#define FLASH_R_BASE   (AHB1PERIPH_BASE + 0x2000u)
#define FLASH_REGS     ((FLASH_TypeDef *)FLASH_R_BASE)

#define FLASH_ACR_LATENCY_Msk  0x7u
#define FLASH_CR_PG            (1u << 0)
#define FLASH_CR_PER           (1u << 1)
#define FLASH_CR_MER           (1u << 2)
#define FLASH_CR_STRT          (1u << 6)
#define FLASH_CR_LOCK          (1u << 7)
#define FLASH_SR_BSY           (1u << 0)
#define FLASH_SR_EOP           (1u << 5)
#define FLASH_KEYR_KEY1        0x45670123u
#define FLASH_KEYR_KEY2        0xCDEF89ABu

/* ── SysTick ─────────────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t LOAD;
    __IO uint32_t VAL;
    __IO uint32_t CALIB;
} SysTick_Type;

#define SysTick_BASE  0xE000E010u
#define SysTick       ((SysTick_Type *)SysTick_BASE)
#define SysTick_CTRL_ENABLE     (1u << 0)
#define SysTick_CTRL_TICKINT    (1u << 1)
#define SysTick_CTRL_CLKSOURCE  (1u << 2)

/* ── SCB (for VTOR) ──────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CPUID;
    __IO uint32_t ICSR;
    __IO uint32_t VTOR;
    __IO uint32_t AIRCR;
    __IO uint32_t SCR;
    __IO uint32_t CCR;
    __IO uint32_t SHPR[3];
    __IO uint32_t SHCSR;
    __IO uint32_t CFSR;
    __IO uint32_t HFSR;
    __IO uint32_t DFSR;
    __IO uint32_t MMFAR;
    __IO uint32_t BFAR;
} SCB_Type;

#define SCB_BASE  0xE000ED00u
#define SCB       ((SCB_Type *)SCB_BASE)
#define SCB_AIRCR_SYSRESETREQ  (1u << 2)
#define SCB_AIRCR_VECTKEY      (0x05FAu << 16)
