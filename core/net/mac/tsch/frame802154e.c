/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         IEEE 802.15.4e-specific frame creation and parsing
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 */

#include <string.h>
#include "net/mac/tsch/frame802154e.h"

/* c.f. IEEE 802.15.4e Table 4b */
enum ieee802154e_header_ie_id {
  HEADER_IE_LE_CSL = 0x1a,
  HEADER_IE_LE_RIT,
  HEADER_IE_DSME_PAN_DESCRIPTOR,
  HEADER_IE_RZ_TIME,
  HEADER_IE_ACK_NACK_TIME_CORRECTION,
  HEADER_IE_GACK,
  HEADER_IE_LOW_LATENCY_NETWORK_INFO,
  HEADER_IE_LIST_TERMINATION_1 = 0x7e,
  HEADER_IE_LIST_TERMINATION_2 = 0x7f,
};

/* c.f. IEEE 802.15.4e Table 4c */
enum ieee802154e_payload_ie_id {
  PAYLOAD_IE_ESDU = 0,
  PAYLOAD_IE_MLME,
  PAYLOAD_IE_LIST_TERMINATION = 0xf,
};

/* c.f. IEEE 802.15.4e Table 4d */
enum ieee802154e_mlme_short_subie_id {
  PAYLOAD_IE_TSCH_SYNCHRONIZATION = 0x1a,
  PAYLOAD_IE_TSCH_SLOFTRAME_AND_LINK,
  PAYLOAD_IE_TSCH_TIMESLOT,
  PAYLOAD_IE_TSCH_HOPPING_TIMING,
  PAYLOAD_IE_TSCH_EB_FILTER,
  PAYLOAD_IE_TSCH_MAC_METRICS_1,
  PAYLOAD_IE_TSCH_MAC_METRICS_2,
};

/* c.f. IEEE 802.15.4e Table 4e */
enum ieee802154e_mlme_long_subie_id {
  PAYLOAD_IE_TSCH_CHANNEL_HOPPING_SEQUENCE = 0x9,
};

/* Create a header IE 2-byte descriptor */
static void
create_header_ie_descriptor(uint8_t *buf, uint8_t element_id, int ie_len)
{
  uint16_t ie_desc;
  /* Header IE descriptor: b0-6: len, b7-14: element id:, b15: type: 0 */
  ie_desc = (ie_len & 0x7f) + ((element_id & 0xff) << 7);
  buf[0] = ie_desc & 0xff;
  buf[1] = (ie_desc >> 8) & 0xff;
}

/* Create a payload IE 2-byte descriptor */
static void
create_payload_ie_descriptor(uint8_t *buf, uint8_t group_id, int ie_len)
{
  uint16_t ie_desc;
  /* MLME Long IE descriptor: b0-10: len, b11-14: group id:, b15: type: 1 */
  ie_desc = (ie_len & 0x07ff) + ((group_id & 0x0f) << 11) + (1 << 15);
  buf[0] = ie_desc & 0xff;
  buf[1] = (ie_desc >> 8) & 0xff;
}

/* Create a MLME short IE 2-byte descriptor */
static void
create_mlme_short_ie_descriptor(uint8_t *buf, uint8_t sub_id, int ie_len)
{
  uint16_t ie_desc;
  /* MLME Short IE descriptor: b0-7: len, b8-14: sub id:, b15: type: 0 */
  ie_desc = (ie_len & 0xff) + ((sub_id & 0x7f) << 8);
  buf[0] = ie_desc & 0xff;
  buf[1] = (ie_desc >> 8) & 0xff;
}

/* Create a MLME long IE 2-byte descriptor */
static void
create_mlme_long_ie_descriptor(uint8_t *buf, uint8_t sub_id, int ie_len)
{
  uint16_t ie_desc;
  /* MLME Long IE descriptor: b0-10: len, b11-14: sub id:, b15: type: 1 */
  ie_desc = (ie_len & 0x07ff) + ((sub_id & 0x0f) << 11) + (1 << 15);
  buf[0] = ie_desc & 0xff;
  buf[1] = (ie_desc >> 8) & 0xff;
}

/* Header IE. ACK/NACK time correction. Used in enhanced ACKs */
int
frame80215e_create_ie_ack_nack_time_correction(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  int ie_len = 2;
  if(len >= 2 + ie_len && ies != NULL) {
    int16_t drift_us;
    uint16_t time_sync_field;
    drift_us = ies->ie_time_correction;
    time_sync_field = drift_us & 0x0fff;
    if(ies->ie_is_nack) {
      time_sync_field |= 0x8000;
    }
    buf[2] = time_sync_field & 0xff;
    buf[3] = (time_sync_field >> 8) & 0xff;
    create_header_ie_descriptor(buf, HEADER_IE_ACK_NACK_TIME_CORRECTION, ie_len);
    return 2 + ie_len;
  } else {
    return -1;
  }
}

/* Payload IE. MLME. Used to nest sub-IEs */
int
frame80215e_create_ie_mlme(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  int ie_len = 0;
  if(len >= 2 + ie_len && ies != NULL) {
    /* The length of the outer MLME IE is the total length of sub-IEs */
    create_payload_ie_descriptor(buf, PAYLOAD_IE_MLME, ies->ie_mlme_len);
    return 2 + ie_len;
  } else {
    return -1;
  }
}


/* MLME sub-IE. TSCH synchronization. Used in EBs: ASN and join priority */
int
frame80215e_create_ie_tsch_synchronization(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  int ie_len = 6;
  if(len >= 2 + ie_len && ies != NULL) {
    buf[2] = ies->ie_asn.ls4b;
    buf[3] = ies->ie_asn.ls4b >> 8;
    buf[4] = ies->ie_asn.ls4b >> 16;
    buf[5] = ies->ie_asn.ls4b >> 24;
    buf[6] = ies->ie_asn.ms1b;
    buf[7] = ies->ie_join_priority;
    create_mlme_short_ie_descriptor(buf, PAYLOAD_IE_TSCH_SYNCHRONIZATION, ie_len);
    return 2 + ie_len;
  } else {
    return -1;
  }
}

/* MLME sub-IE. TSCH slotframe and link. Used in EBs: initial schedule */
int frame80215e_create_ie_tsch_slotframe_and_link(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  /* TODO */
  return -1;
}

/* MLME sub-IE. TSCH timeslot. Used in EBs: timeslot template (timing) */
int
frame80215e_create_ie_tsch_timeslot(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  int ie_len = 1;
  if(len >= 2 + ie_len && ies != NULL) {
    buf[2] = ies->ie_tsch_timeslot_id;
    create_mlme_short_ie_descriptor(buf, PAYLOAD_IE_TSCH_TIMESLOT, ie_len);
    return 2 + ie_len;
  } else {
    return -1;
  }
}

/* MLME sub-IE. TSCH channel hopping sequence. Used in EBs: hopping sequence */
int
frame80215e_create_ie_tsch_channel_hopping_sequence(uint8_t *buf, int len,
    struct ieee802154_ies *ies)
{
  int ie_len = 1;
  if(len >= 2 + ie_len && ies != NULL) {
    buf[2] = ies->ie_channel_hopping_sequence_id;
    create_mlme_long_ie_descriptor(buf, PAYLOAD_IE_TSCH_CHANNEL_HOPPING_SEQUENCE, ie_len);
    return 2 + ie_len;
  } else {
    return -1;
  }
}

/* Parse a header IE */
static int
frame802154e_parse_header_ie(uint8_t *buf, int len,
    uint8_t element_id, struct ieee802154_ies *ies)
{
  switch(element_id) {
    case HEADER_IE_ACK_NACK_TIME_CORRECTION:
      if(len == 2) {
        if(ies != NULL) {
          /* If the originator was a time source neighbor, the receiver adjust
           * its own clock by incorporating the received drift correction */
          uint16_t time_sync_field = 0;
          int16_t drift_us = 0;
          /* Extract drift correction from Sync-IE, cast from 12 to 16-bit,
           * and convert it to RTIMER ticks.
           * See page 88 in IEEE Std 802.15.4e-2012. */
          time_sync_field = buf[0] | buf[1] << 8;
          /* First extract NACK */
          ies->ie_is_nack = (time_sync_field & (uint16_t)0x8000) ? 1 : 0;
          /* Then cast from 12 to 16 bit signed */
          if(time_sync_field & 0x0800) { /* Negative integer */
            drift_us = time_sync_field | 0xf000;
          } else { /* Positive integer */
            drift_us = time_sync_field & 0x0fff;
          }
          /* Convert to RTIMER ticks */
          ies->ie_time_correction = drift_us;
        }
      }
      break;
  }
  return len;
}

/* Parse a MLME short IE */
static int
frame802154e_parse_mlme_short_ie(uint8_t *buf, int len,
    uint8_t sub_id, struct ieee802154_ies *ies)
{
  switch(sub_id) {
    case PAYLOAD_IE_TSCH_SYNCHRONIZATION:
      if(len == 6) {
        if(ies != NULL) {
          ies->ie_asn.ls4b = (uint32_t)buf[0];
          ies->ie_asn.ls4b |= (uint32_t)buf[1] << 8;
          ies->ie_asn.ls4b |= (uint32_t)buf[2] << 16;
          ies->ie_asn.ls4b |= (uint32_t)buf[3] << 24;
          ies->ie_asn.ms1b = (uint8_t)buf[4];
          ies->ie_join_priority = (uint8_t)buf[5];
        }
      }
      break;
    case PAYLOAD_IE_TSCH_TIMESLOT:
      if(len == 1) {
        if(ies != NULL) {
          ies->ie_tsch_timeslot_id = buf[0];
        }
      }
      break;
  }
  return len;
}

/* Parse a MLME long IE */
static int
frame802154e_parse_mlme_long_ie(uint8_t *buf, int len,
    uint8_t sub_id, struct ieee802154_ies *ies)
{
  switch(sub_id) {
  case PAYLOAD_IE_TSCH_CHANNEL_HOPPING_SEQUENCE:
    if(len == 1) {
      if(ies != NULL) {
        ies->ie_channel_hopping_sequence_id = buf[0];
      }
    }
    break;
  }
  return len;
}

/* Parse all IEEE 802.15.4e Information Elements (IE) from a frame */
int
frame802154e_parse_information_elements(uint8_t *buf, uint8_t buf_size,
    struct ieee802154_ies *ies)
{
  uint8_t *start = buf;
  uint16_t ie_desc;
  uint8_t type;
  uint16_t len;
  uint8_t id;
  int in_nested_mlme = 0;
  /* Loop over all IEs */
  while(buf_size > 0) {
    if(buf_size < 2) { /* Not enough space for IE descriptor */
      return -1;
    }
    ie_desc = (buf[1] << 8) + buf[0];
    buf_size -= 2;
    buf += 2;
    type = ie_desc & 0x8000 ? 1 : 0; /* b15 */
    if(in_nested_mlme == 0) {
      /* Header IE: type == 0 means header IE, type == 1 means payload IE */
      if(type == 0) {
        /* Header IE: 2 bytes descriptor, c.f. fig 48n in IEEE 802.15.4e */
        len = ie_desc & 0x007f; /* b0-b6 */
        id = (ie_desc & 0x7f80) >> 7; /* b7-b14 */
        if(len > buf_size || !frame802154e_parse_header_ie(buf, len, id, ies)) {
          return -1;
        }
        if(id == HEADER_IE_LIST_TERMINATION_2) {
          /* End of IE parsing */
          return buf + len - start;
        }
      } else {
        /* Payload IE: 2 bytes descriptor, c.f. fig 48o in IEEE 802.15.4e */
        len = ie_desc & 0x7ff; /* b0-b10 */
        id = (ie_desc & 0x7800) >> 11; /* b11-b14 */
        if(id == 1) {
          in_nested_mlme = len;
          len = 0; /* Reset len as we want to read subIEs and not jump over them */
        }
      }
    } else {
      /* MLME sub-IE: 2 bytes descriptor, c.f. fig 48q in IEEE 802.15.4e */
      /* type == 0 means short sub-IE, type == 1 means long sub-IE */
      if(type == 0) {
        /* Short sub-IE, c.f. fig 48r in IEEE 802.15.4e */
        len = ie_desc & 0x00ff; /* b0-b7 */
        id = (ie_desc & 0x7f00) >> 8; /* b8-b14 */
        if(len > buf_size || !frame802154e_parse_mlme_short_ie(buf, len, id, ies)) {
          return -1;
        }
      } else {
        /* Long sub-IE, c.f. fig 48s in IEEE 802.15.4e */
        len = ie_desc & 0x7ff; /* b0-b10 */
        id = (ie_desc & 0x7800) >> 11; /* b11-b14 */
        if(len > buf_size || !frame802154e_parse_mlme_long_ie(buf, len, id, ies)) {
          return -1;
        }
      }
      /* Update remaining nested MLME len */
      in_nested_mlme -= 2 + len;
      if(in_nested_mlme < 0) {
        /* We found more sub-IEs than initially advertised */
        return -1;
      }
      if(in_nested_mlme == 0) {
        /* End of IE parsing */
        return buf + len - start;
      }
    }
    buf += len;
    buf_size -= len;
  }
  return buf - start;
}