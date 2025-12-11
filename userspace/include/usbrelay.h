#ifndef USBRELAY_PROTO_H
#define USBRELAY_PROTO_H

#define USBRELAY_PROTO_VERSION       "1.1"


#define USBRELAY_NUM_CHANNELS        4
#define USBRELAY_CH_MIN              1


#define USBRELAY_CH_TO_BIT(ch)       (1u << ((ch) - 1))
#define USBRELAY_MASK_ALL            ((unsigned int)((1u << USBRELAY_NUM_CHANNELS) - 1))

enum usbrelay_cmd_type {
    USBRELAY_CMD_SET = 0,
    USBRELAY_CMD_GET,
    USBRELAY_CMD_GETALL,
    USBRELAY_CMD_TOGGLE,
    USBRELAY_CMD_WRITE_MASK,
    USBRELAY_CMD_READ_MASK,
    USBRELAY_CMD_RESET,
    USBRELAY_CMD_PING,
    USBRELAY_CMD_VERSION,
    USBRELAY_CMD_HELP,

    USBRELAY_CMD_INVALID
};

enum usbrelay_channel_state {
    USBRELAY_CH_STATE_OFF = 0,
    USBRELAY_CH_STATE_ON  = 1,
};

/*
 * Protocol-level error codes for ASCII "ERR <CODE> <MESSAGE>" responses.
 *
 * Recommended mapping:
 *   USBRELAY_PROTO_ERR_BAD_COMMAND -> "BAD_COMMAND"
 *   USBRELAY_PROTO_ERR_BAD_CHANNEL -> "BAD_CHANNEL"
 *   USBRELAY_PROTO_ERR_BAD_STATE   -> "BAD_STATE"
 *   USBRELAY_PROTO_ERR_BAD_MASK    -> "BAD_MASK"
 *   USBRELAY_PROTO_ERR_DEVICE      -> "DEVICE_UNAVAILABLE"
 *   USBRELAY_PROTO_ERR_INTERNAL    -> "INTERNAL_ERROR"
 */
enum usbrelay_proto_err {
    USBRELAY_PROTO_OK = 0,           /* success */

    USBRELAY_PROTO_ERR_BAD_COMMAND,  /* unknown command or syntax error */
    USBRELAY_PROTO_ERR_BAD_CHANNEL,  /* channel not in 1..4 */
    USBRELAY_PROTO_ERR_BAD_STATE,    /* state not ON/OFF */
    USBRELAY_PROTO_ERR_BAD_MASK,     /* mask invalid or out of range */
    USBRELAY_PROTO_ERR_DEVICE,       /* device unavailable / I/O error */
    USBRELAY_PROTO_ERR_INTERNAL,     /* internal error */

    USBRELAY_PROTO_ERR_MAX
};

/*
 * Parsed representation of a single ASCII command line.
 *
 * This is a protocol-level structure and does not contain any device handles.
 *
 * Fields:
 *   type    - which command (SET/GET/etc).
 *   channel - channel number for SET/GET/TOGGLE (1..4), or 0 if not used.
 *   state   - desired state for SET (ON/OFF).
 *   mask    - mask value for WRITE MASK / READ MASK (lower 4 bits used).
 */
struct usbrelay_cmd {
    enum usbrelay_cmd_type       type;
    unsigned int                 channel;  /* 0 if not applicable */
    enum usbrelay_channel_state  state;    /* only used for SET */
    unsigned int                 mask;     /* for mask operations (0x00-0x0F) */
};

#endif /* USBRELAY_PROTO_H */
