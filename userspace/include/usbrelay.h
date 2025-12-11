#ifndef USBRELAY_H
#define USBRELAY_H

#include <stdint.h>

#define USBRELAY_PROTO_VERSION   "1.1"

#define USBRELAY_NUM_CHANNELS    4
#define USBRELAY_MIN_CHANNEL     1
#define USBRELAY_MAX_CHANNEL     USBRELAY_NUM_CHANNELS

#define USBRELAY_CH_TO_BIT(ch)   (1U << ((ch) - 1))
#define USBRELAY_MASK_ALL        ((uint8_t)((1U << USBRELAY_NUM_CHANNELS) - 1))

#define USBRELAY_MAX_LINE_LEN    128
#define USBRELAY_DEFAULT_DEVICE  "/dev/usbrelay0"

#endif /* USBRELAY_H */
