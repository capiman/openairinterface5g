/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \brief PWS (ETWS/CMAS) SIB generation for the gNB, see nr_pws.h */

#include <string.h>

#include "nr_pws.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"
#include "assertions.h"
#include "utils.h"

#define CONFIG_STRING_PWS "pws"

#define PWS_CONFIG_STRING_MESSAGE_IDENTIFIER "message_identifier"
#define PWS_CONFIG_STRING_SERIAL_NUMBER "serial_number"
#define PWS_CONFIG_STRING_WARNING_TYPE "warning_type"
#define PWS_CONFIG_STRING_WARNING_MESSAGE "warning_message"
#define PWS_CONFIG_STRING_DATA_CODING_SCHEME "data_coding_scheme"
#define PWS_CONFIG_STRING_SHORT_MESSAGE_PERIOD "short_message_period_rf"

#define PWS_CONFIG_HLP_MESSAGE_IDENTIFIER "Message Identifier (TS 23.041), e.g. 4370 for the highest CMAS severity\n"
#define PWS_CONFIG_HLP_SERIAL_NUMBER "Serial Number (TS 23.041); change it to make the UE treat the warning as new\n"
#define PWS_CONFIG_HLP_WARNING_TYPE "SIB6 Warning Type, 2 octets (TS 23.041 9.3.24)\n"
#define PWS_CONFIG_HLP_WARNING_MESSAGE "SIB7/SIB8 warning text (max 93 chars, GSM 7 bit default alphabet)\n"
#define PWS_CONFIG_HLP_DATA_CODING_SCHEME "Data Coding Scheme (TS 23.038), 0x00 German, 0x01 English\n"
#define PWS_CONFIG_HLP_SHORT_MESSAGE_PERIOD "Period in radio frames of the paging Short Message, 0 to disable\n"

/* clang-format off */
#define PWS_PARAMS_DESC {                                                                                                     \
  {PWS_CONFIG_STRING_MESSAGE_IDENTIFIER,   PWS_CONFIG_HLP_MESSAGE_IDENTIFIER,   0, .iptr = NULL, .defintval = 4370,  TYPE_INT,    0}, \
  {PWS_CONFIG_STRING_SERIAL_NUMBER,        PWS_CONFIG_HLP_SERIAL_NUMBER,        0, .iptr = NULL, .defintval = 0,     TYPE_INT,    0}, \
  {PWS_CONFIG_STRING_WARNING_TYPE,         PWS_CONFIG_HLP_WARNING_TYPE,         0, .iptr = NULL, .defintval = 0x0080, TYPE_INT,   0}, \
  {PWS_CONFIG_STRING_WARNING_MESSAGE,      PWS_CONFIG_HLP_WARNING_MESSAGE,      0, .strptr = NULL, .defstrval = "OAI PWS test message", TYPE_STRING, 0}, \
  {PWS_CONFIG_STRING_DATA_CODING_SCHEME,   PWS_CONFIG_HLP_DATA_CODING_SCHEME,   0, .iptr = NULL, .defintval = 0x01,  TYPE_INT,    0}, \
  {PWS_CONFIG_STRING_SHORT_MESSAGE_PERIOD, PWS_CONFIG_HLP_SHORT_MESSAGE_PERIOD, 0, .iptr = NULL, .defintval = 8,     TYPE_INT,    0}, \
}
/* clang-format on */

#define PWS_MESSAGE_IDENTIFIER_IDX 0
#define PWS_SERIAL_NUMBER_IDX 1
#define PWS_WARNING_TYPE_IDX 2
#define PWS_WARNING_MESSAGE_IDX 3
#define PWS_DATA_CODING_SCHEME_IDX 4
#define PWS_SHORT_MESSAGE_PERIOD_IDX 5

static nr_pws_config_t pws_config;
static bool pws_config_read = false;

const nr_pws_config_t *nr_pws_get_config(void)
{
  if (pws_config_read)
    return &pws_config;

  paramdef_t pws_params[] = PWS_PARAMS_DESC;
  /* The section is optional: without it every parameter falls back to its default. */
  config_get(config_get_if(), pws_params, sizeofArray(pws_params), CONFIG_STRING_PWS);

  pws_config.message_identifier = *pws_params[PWS_MESSAGE_IDENTIFIER_IDX].iptr & 0xffff;
  pws_config.serial_number = *pws_params[PWS_SERIAL_NUMBER_IDX].iptr & 0xffff;
  pws_config.warning_type = *pws_params[PWS_WARNING_TYPE_IDX].iptr & 0xffff;
  pws_config.warning_message = *pws_params[PWS_WARNING_MESSAGE_IDX].strptr;
  pws_config.data_coding_scheme = *pws_params[PWS_DATA_CODING_SCHEME_IDX].iptr & 0xff;
  pws_config.short_message_period_rf = *pws_params[PWS_SHORT_MESSAGE_PERIOD_IDX].iptr & 0xffff;

  LOG_I(NR_MAC,
        "PWS: message_identifier=%u serial_number=%u warning_type=0x%04x dcs=0x%02x short_message_period_rf=%u message=\"%s\"\n",
        pws_config.message_identifier,
        pws_config.serial_number,
        pws_config.warning_type,
        pws_config.data_coding_scheme,
        pws_config.short_message_period_rf,
        pws_config.warning_message ? pws_config.warning_message : "");

  pws_config_read = true;
  return &pws_config;
}

/** @brief Map one ASCII character to the GSM 7 bit default alphabet (TS 23.038 6.2.1).
 *
 * Only the characters that differ from ASCII are listed; everything else in the
 * printable ASCII range maps onto itself. Unmappable input becomes '?'. */
static uint8_t ascii_to_gsm7(char c)
{
  switch (c) {
    case '@': return 0x00;
    case '$': return 0x02;
    case '\n': return 0x0a;
    case '\r': return 0x0d;
    case '_': return 0x11;
    case '[': return 0x3c; /* actually an escape sequence, approximated */
    case ']': return 0x3e;
    case '{': return 0x28;
    case '}': return 0x29;
    case '\\': return 0x2f;
    case '~': return 0x3d;
    case '|': return 0x40;
    case '^': return 0x14;
    default: break;
  }
  if ((c >= 0x20 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a))
    return (uint8_t)c;
  return 0x3f; /* '?' */
}

int nr_pws_encode_cb_page(const char *text, uint8_t out[NR_PWS_CB_PAGE_SIZE])
{
  const int max_septets = 93; /* 82 octets * 8 / 7 */
  uint8_t septets[93];
  int n = 0;

  for (const char *p = text; p && *p && n < max_septets; p++)
    septets[n++] = ascii_to_gsm7(*p);

  if (text && strlen(text) > (size_t)max_septets)
    LOG_W(NR_MAC, "PWS: warning message truncated to %d characters (one CBS information page)\n", max_septets);

  /* TS 23.041 9.4.2.2.5: pad the remainder of the page with CR. */
  const int used_octets = (n * 7 + 7) / 8;
  while (n < max_septets)
    septets[n++] = 0x0d;

  memset(out, 0, NR_PWS_CB_PAGE_SIZE);
  /* 7 bit packing, LSB first (TS 23.038 6.1.2.1.1). */
  for (int i = 0; i < max_septets; i++) {
    const int bit = i * 7;
    const int octet = bit / 8;
    const int shift = bit % 8;
    out[octet] |= (uint8_t)(septets[i] << shift);
    if (shift > 1)
      out[octet + 1] |= (uint8_t)(septets[i] >> (8 - shift));
  }
  /* Last octet of the page carries the number of used octets. */
  out[NR_PWS_CB_PAGE_SIZE - 1] = (uint8_t)used_octets;
  return NR_PWS_CB_PAGE_SIZE;
}

/** @brief Fill a 16 bit BIT STRING (messageIdentifier, serialNumber). */
static void set_bitstring16(BIT_STRING_t *bs, uint16_t value)
{
  bs->buf = calloc_or_fail(2, sizeof(*bs->buf));
  bs->size = 2;
  bs->bits_unused = 0;
  bs->buf[0] = (uint8_t)(value >> 8);
  bs->buf[1] = (uint8_t)(value & 0xff);
}

NR_SIB6_t *get_SIB6_NR(void)
{
  const nr_pws_config_t *cfg = nr_pws_get_config();
  NR_SIB6_t *sib6 = calloc_or_fail(1, sizeof(*sib6));

  set_bitstring16(&sib6->messageIdentifier, cfg->message_identifier);
  set_bitstring16(&sib6->serialNumber, cfg->serial_number);

  const uint8_t wt[2] = {(uint8_t)(cfg->warning_type >> 8), (uint8_t)(cfg->warning_type & 0xff)};
  OCTET_STRING_fromBuf(&sib6->warningType, (const char *)wt, sizeof(wt));

  LOG_I(NR_MAC, "PWS: SIB6 (ETWS primary) warningType=0x%04x\n", cfg->warning_type);
  return sib6;
}

/** @brief Common body of SIB7 and SIB8: one self contained warning segment. */
static void fill_warning_segment(BIT_STRING_t *message_identifier,
                                 BIT_STRING_t *serial_number,
                                 long *segment_type,
                                 long *segment_number,
                                 OCTET_STRING_t *segment,
                                 OCTET_STRING_t **dcs,
                                 const nr_pws_config_t *cfg,
                                 long last_segment_value)
{
  set_bitstring16(message_identifier, cfg->message_identifier);
  set_bitstring16(serial_number, cfg->serial_number);

  /* The whole message fits into one CBS information page, so this is both the
   * first and the last segment. */
  *segment_type = last_segment_value;
  *segment_number = 0;

  uint8_t page[NR_PWS_CB_PAGE_SIZE];
  const int len = nr_pws_encode_cb_page(cfg->warning_message, page);
  OCTET_STRING_fromBuf(segment, (const char *)page, len);

  /* Cond Segment1: dataCodingScheme is present in the first segment only. */
  *dcs = calloc_or_fail(1, sizeof(**dcs));
  const uint8_t dcs_octet = cfg->data_coding_scheme;
  OCTET_STRING_fromBuf(*dcs, (const char *)&dcs_octet, 1);
}

NR_SIB7_t *get_SIB7_NR(void)
{
  const nr_pws_config_t *cfg = nr_pws_get_config();
  NR_SIB7_t *sib7 = calloc_or_fail(1, sizeof(*sib7));

  fill_warning_segment(&sib7->messageIdentifier,
                       &sib7->serialNumber,
                       &sib7->warningMessageSegmentType,
                       &sib7->warningMessageSegmentNumber,
                       &sib7->warningMessageSegment,
                       &sib7->dataCodingScheme,
                       cfg,
                       NR_SIB7__warningMessageSegmentType_lastSegment);

  LOG_I(NR_MAC, "PWS: SIB7 (ETWS secondary) message_identifier=%u\n", cfg->message_identifier);
  return sib7;
}

NR_SIB8_t *get_SIB8_NR(void)
{
  const nr_pws_config_t *cfg = nr_pws_get_config();
  NR_SIB8_t *sib8 = calloc_or_fail(1, sizeof(*sib8));

  fill_warning_segment(&sib8->messageIdentifier,
                       &sib8->serialNumber,
                       &sib8->warningMessageSegmentType,
                       &sib8->warningMessageSegmentNumber,
                       &sib8->warningMessageSegment,
                       &sib8->dataCodingScheme,
                       cfg,
                       NR_SIB8__warningMessageSegmentType_lastSegment);

  LOG_I(NR_MAC, "PWS: SIB8 (CMAS) message_identifier=%u\n", cfg->message_identifier);
  return sib8;
}
