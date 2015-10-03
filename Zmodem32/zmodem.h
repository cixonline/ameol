/* ZMODEM.H - ZMODEM definitions and interface
 *
 * Copyright 1993-2015 CIX Online Ltd, All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ZMODEM_H_

#include "transfer.h"

#define ZRQINIT         0       /* Request receive init */
#define ZRINIT          1       /* Receive init */
#define ZSINIT          2       /* Send init sequence (optional) */
#define ZACK            3       /* ACK to above */
#define ZFILE           4       /* File name from sender */
#define ZSKIP           5       /* To sender: skip this file */
#define ZNAK            6       /* Last packet was garbled */
#define ZABORT          7       /* Abort batch transfers */
#define ZFIN            8       /* Finish session */
#define ZRPOS           9       /* Resume data trans at this position */
#define ZDATA           10      /* Data packet(s) follow */
#define ZEOF            11      /* End of file */
#define ZFERR           12      /* Fatal Read or Write error Detected */
#define ZCRC            13      /* Request for file CRC and response */
#define ZCHALLENGE      14      /* Receiver's Challenge */
#define ZCOMPL          15      /* Request is complete */
#define ZCAN            16      /* Other end canned session with CAN*5 */
#define ZFREECNT        17
#define ZCOMMAND        18
#define ZSTDERR         19
#define ZCRCE           'h' /* CRC next, frame ends, header packet follows */
#define ZCRCG           'i' /* CRC next, frame continues nonstop */
#define ZCRCQ           'j' /* CRC next, frame continues, ZACK expected */
#define ZCRCW           'k' /* CRC next, ZACK expected, end of frame */
#define ZRUB0           'l' /* Translate to rubout 0177 */
#define ZRUB1           'm' /* Translate to rubout 0377 */
#define ZOK             0       /* No error */
#define ZERROR          -1
#define ZTIMEOUT        -2      /* Receiver timeout error */
#define RCDO            -3      /* Loss of carrier */
#define FUBAR           -4
#define GOTOR           256
#define GOTCRCE         360
#define GOTCRCG         361
#define GOTCRCQ         362
#define GOTCRCW         363
#define GOTCAN          272

#define ZF0             3
#define ZF1             2
#define ZF2             1
#define ZF3             0
#define ZP0             0
#define ZP1             1
#define ZP2             2
#define ZP3             3

#define TESCCTL         64
#define TESC8           128

#define DLE             16
#define XON             17
#define XOFF            19
#define CAN             24
#define CR              '\r'
#define LF              '\n'

#define ZCRESUM         3

#define ZPAD            '*'
#define ZDLE            CAN
#define ZDLEE           88
#define ZBIN            'A'
#define ZHEX            'B'
#define ZVBIN           'a'
#define ZVHEX           'b'
#define ZBIN32          'C'
#define ZMAXHLEN        16
#define ZMAXSPLEN       1024
#define ZMAXBUFLEN      ((ZMAXSPLEN*2)+12)
#define ZATTNLEN        32      /* Max length of attention string */

#define ASSUCCESS                       0
#define ASGENERALERROR                  -1
#define ASABORT                         -2
#define ASNOCARRIER                     -3
#define ASBUFREMPTY                     -8
#define ASOVERFLOW                      -31

#define XFER_RETURN_SUCCESS         0       /* Successful file transfer         */
#define XFER_RETURN_CANT_GET_BUFFER -601    /* Failed to allocate comm buffer   */
#define XFER_RETURN_CANT_OPEN_FILE  -602    /* Couldn't open transfer file      */
#define XFER_RETURN_CANT_SEND_NAK   -603    /* Buffer full, can't send NAK      */
#define XFER_RETURN_CANT_SEND_ACK   -604    /* Buffer full, can't send ACK      */
#define XFER_RETURN_KEYBOARD_ABORT  -605    /* User hit the abort key           */
#define XFER_RETURN_REMOTE_ABORT    -606    /* CAN-CAN abort from remote end    */
#define XFER_RETURN_FILE_ERROR      -607    /* Failure during file I/O          */
#define XFER_RETURN_PACKET_TIMEOUT  -608    /* Timed out waiting for packet     */
#define XFER_RETURN_BAD_PACKET_SIZE -609    /* Got an invalid packet size       */
#define XFER_RETURN_TOO_MANY_ERRORS -610    /* Got too many errors to go on     */
#define XFER_RETURN_LOGIC_ERROR     -611    /* Internal error, will never happen*/
#define XFER_RETURN_CANT_PUT_BUFFER -612    /* Error transmitting buffer        */
#define XFER_RETURN_CANT_SEND_EOT   -613    /* Buffer full, can't send EOT      */
#define XFER_RETURN_PROTOCOL_ERROR  -614    /* Usually a bad packet type in kerm*/
#define XFER_RETURN_CANT_PUT_CHAR   -615    /* Error transmitting a character   */
#define XFER_RETURN_CANT_GET_CHAR   -616    /* Error receiving a character      */
#define XFER_RETURN_ASCII_TIMEOUT   -617    /* Nothing happening during ASCII   */

#define ZMODEM_MAX_ERRORS           10      /* Maximum number of block retries  */

#define CANFDX          1       /* Can handle full-duplex (yes for PC's) */
#define CANOVIO         2       /* Can overlay disk and serial I/O (ditto) */
#define CANBRK          4       /* Can send a break - True but superfluous */
#define CANCRY          8       /* Can encrypt/decrypt - not defined yet */
#define CANLZW          16      /* Can LZ compress - not defined yet */
#define CANFC32         32      /* Can use 32 bit crc frame checks - true */
#define ESCALL          64      /* Escapes all control chars. NOT implemented */
#define ESC8            128     /* Escapes the 8th bit. NOT implemented */

BOOL FASTCALL ZModemSend( LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL );
BOOL FASTCALL ZModemReceive( LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL, WORD );
BOOL FASTCALL ZModemCancel( LPCOMMDEVICE );

#define _ZMODEM_H_
#endif
