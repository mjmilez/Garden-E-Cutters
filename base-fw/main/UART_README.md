# UART Protocol (Base ESP32 → Raspberry Pi)

Baud: 115200\

Wiring: ESP32 TX → Pi RX\
ESP32 RX ← Pi TX\
GND ↔ GND

------------------------------------------------------------------------

## Packet Layout

Every packet:

\[0xAA\]\[TYPE\]\[LEN\]\[PAYLOAD...\]\[CHECKSUM\]

START byte = 0xAA

LEN = 1 byte (0--255)

CHECKSUM = XOR of: TYPE \^ LEN \^ all payload bytes

START is NOT included in checksum.

------------------------------------------------------------------------

## Packet Types

0x01 = START\
0x02 = DATA\
0x03 = END\
0x04 = ACK\
0x05 = COMMIT

------------------------------------------------------------------------

## START

Payload: \[fileSize uint32 little-endian\]

Pi must ACK.

------------------------------------------------------------------------

## DATA

Payload: \[file bytes\]

Max 255 bytes per packet.

Each DATA must be ACKed before next is sent.

------------------------------------------------------------------------

## END

No payload.

Pi ACKs it.

------------------------------------------------------------------------

## COMMIT

Payload: \[status\]

0x00 = success\
Anything else = failure

ESP32 only clears the file after COMMIT(0x00).

------------------------------------------------------------------------

Flow:

ESP → START\
Pi → ACK\
ESP → DATA\
Pi → ACK\
(repeat)\
ESP → END\
Pi → ACK\
Pi verifies file\
Pi → COMMIT\
ESP clears file
