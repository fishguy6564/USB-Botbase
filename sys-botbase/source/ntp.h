/* This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
For more information, please refer to <https://unlicense.org>
Additionally, the ntp_packet struct uses code licensed under the BSD 3-clause. See LICENSE-THIRD-PARTY for more
information. */

#pragma once

#include <time.h>

#define NTP_TIMESTAMP_DELTA 2208988800ull

/* ------------- BEGIN BSD 3-CLAUSE LICENSED CODE-------------------- */
// https://www.cisco.com/c/en/us/about/press/internet-protocol-journal/back-issues/table-contents-58/154-ntp.html
// Struct adapted from https://github.com/lettier/ntpclient , see LICENSE-THIRD-PARTY for more information.
typedef struct {
    uint8_t li_vn_mode;  // li: two bits, leap indicator. vn: three bits, protocol version number. mode: three bits,
                         // client mode.

    uint8_t stratum;    // Stratum level of the local clock.
    uint8_t poll;       // Maximum interval between successive messages.
    uint8_t precision;  // Precision of the local clock.

    uint32_t rootDelay;       // Total round trip delay time.
    uint32_t rootDispersion;  // Max error allowed from primary clock source.
    uint32_t refId;           // Reference clock identifier.

    uint32_t refTm_s;  // Reference time-stamp seconds.
    uint32_t refTm_f;  // Reference time-stamp fraction of a second.

    uint32_t origTm_s;  // Originate time-stamp seconds.
    uint32_t origTm_f;  // Originate time-stamp fraction of a second.

    uint32_t rxTm_s;  // Received time-stamp seconds.
    uint32_t rxTm_f;  // Received time-stamp fraction of a second.

    uint32_t txTm_s;  // Transmit time-stamp seconds.
    uint32_t txTm_f;  // Transmit time-stamp fraction of a second.
} ntp_packet;
/* ------------- END BSD 3-CLAUSE LICENSED CODE-------------------- */

time_t ntpGetTime();