/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \brief Public Warning System (ETWS/CMAS) support for the gNB: SIB6/SIB7/SIB8
 *         generation and the paging Short Message that triggers their acquisition.
 *
 * TS 38.331: SIB6 carries the ETWS primary notification, SIB7 the ETWS secondary
 * notification and SIB8 the CMAS notification (also used by EU-Alert / DE-Alert
 * and other regional schemes). TS 23.041 defines the warning message contents.
 *
 * This is a static, locally configured broadcast intended for lab testing of UE
 * and modem side PWS handling. There is no CBE/CBCF/AMF interaction: the warning
 * is taken from the configuration file and broadcast for as long as the gNB runs.
 */

#ifndef NR_PWS_H
#define NR_PWS_H

#include <stdbool.h>
#include <stdint.h>

#include "NR_SIB6.h"
#include "NR_SIB7.h"
#include "NR_SIB8.h"

/** @brief Locally configured PWS warning, read once from the "pws" section. */
typedef struct {
  /* Message Identifier (TS 23.041 9.4.1.2.2), e.g. 4370 for the highest CMAS
   * severity level used by WEA and by EU-Alert / DE-Alert. */
  uint16_t message_identifier;
  /* Serial Number (TS 23.041 9.4.1.2.1). A UE treats a message with an already
   * seen identifier/serial pair as a duplicate and does not alert again. */
  uint16_t serial_number;
  /* SIB6 only: Warning Type, 2 octets (TS 23.041 9.3.24). Bits 15..9 carry the
   * warning type value (0 earthquake, 1 tsunami, 2 both, 3 test, 4 other),
   * bit 8 is the Emergency User Alert flag, bit 7 the Popup flag. */
  uint16_t warning_type;
  /* SIB7/SIB8: warning text, encoded into one CBS information page. */
  const char *warning_message;
  /* Data Coding Scheme (TS 23.038 5). Only the GSM 7 bit default alphabet is
   * produced here, so this selects the language indication, e.g. 0x00 German,
   * 0x01 English. */
  uint8_t data_coding_scheme;
  /* Period in radio frames at which the paging Short Message carrying
   * etwsAndCmasIndication is transmitted, 0 disables it. */
  uint16_t short_message_period_rf;
} nr_pws_config_t;

/** @brief Read (once) and return the PWS configuration. */
const nr_pws_config_t *nr_pws_get_config(void);

/** @brief Build SIB6 (ETWS primary notification) from the configuration. */
NR_SIB6_t *get_SIB6_NR(void);
/** @brief Build SIB7 (ETWS secondary notification) from the configuration. */
NR_SIB7_t *get_SIB7_NR(void);
/** @brief Build SIB8 (CMAS notification) from the configuration. */
NR_SIB8_t *get_SIB8_NR(void);

/** @brief Pack an ASCII string into one CBS information page (TS 23.041 9.4.2.2.5).
 *
 * The warning message contents are a sequence of 82 octet pages, each followed by
 * one octet giving the number of used octets in that page. One page holds at most
 * 93 characters in the GSM 7 bit default alphabet; longer input is truncated.
 *
 * @param text  NUL terminated ASCII input
 * @param out   output buffer, must hold NR_PWS_CB_PAGE_SIZE octets
 * @return number of octets written (always NR_PWS_CB_PAGE_SIZE) */
#define NR_PWS_CB_PAGE_SIZE 83
int nr_pws_encode_cb_page(const char *text, uint8_t out[NR_PWS_CB_PAGE_SIZE]);

#endif /* NR_PWS_H */
