#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>  
#include <strings.h>  
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "usbrelay.h"

#ifndef PATH_MAX
#define PATH_MAX    128
#endif

#define RELAYCTL_TOOL_VERSION "0.1"

/* Holds state for a single use of relayctl. */
struct relay_context {
    int fd;
    uint8_t mask;
    char dev_path[PATH_MAX];
    int verbose;
    int interactive;
};

enum relayctl_cmd {
    RELAYCTL_CMD_NONE = 0,
    RELAYCTL_CMD_SET,
    RELAYCTL_CMD_GET,
    RELAYCTL_CMD_GETALL,
    RELAYCTL_CMD_TOGGLE,
    RELAYCTL_CMD_WRITE_MASK,
    RELAYCTL_CMD_READ_MASK,
    RELAYCTL_CMD_RESET,
    RELAYCTL_CMD_PING,
    RELAYCTL_CMD_VERSION,
    RELAYCTL_CMD_HELP
};

/* ON/OFF state used when parsing "set" commands. */
enum relayctl_state {
    RELAYCTL_STATE_OFF = 0,
    RELAYCTL_STATE_ON  = 1
};


/* Holds the result of parsing argv. */
struct relayctl_args {
    enum relayctl_cmd   cmd;        /* which high-level command */
    int                 channel;    /* channel number for channel-based commands (1..4 or 0) */
    enum relayctl_state state;     /* ON/OFF for set, if relevant */
    uint8_t             mask;       /* mask for write-mask, if relevant */
    char                dev_path[PATH_MAX]; /* device path override, if provided */
    int                 interactive; /* nonzero if -i / interactive requested */
    int                 verbose;     /* nonzero if verbose mode requested */
};

/* forward declarations used by parse_args */
static int parse_channel_arg(const char *arg, int *out_channel);
static int parse_mask_arg(const char *arg, uint8_t *out_mask);

/* Help messages on failure */
static void print_usage(void) {
    fprintf(stderr,
        "Usage:\n"
        "  relayctl set <ch> <on|off>        Set channel on or off\n"
        "  relayctl get <ch>                 Get state of a single channel\n"
        "  relayctl getall                   Get state of all channels (mask)\n"
        "  relayctl toggle <ch>              Toggle a single channel\n"
        "  relayctl write-mask 0xHH          Write full 4-bit mask (0x00-0x0F)\n"
        "  relayctl read-mask                Read current mask from device\n"
        "  relayctl reset                    Turn all channels off\n"
        "  relayctl ping                     Check device responsiveness\n"
        "  relayctl version                  Show tool/protocol version\n"
        "  relayctl help                     Show detailed help\n"
        "\n"
        "Options:\n"
        "  -d <device>                        Device path (default: /dev/usbrelay0)\n"
        "  -v                                 Verbose output (debug logging)\n"
        "  -i                                 Interactive mode (REPL)\n"
    );
}
static void print_help(void) {
    printf(
        "relayctl - SainSmart 4-Channel 5V USB Relay Controller\n"
        "\n"
        "This tool controls a 4-channel USB relay board using a simple ASCII\n"
        "protocol on top of a 1-byte mask ABI exposed by the kernel driver.\n"
        "\n"
        "Commands:\n"
        "  set <ch> <on|off>\n"
        "      Set channel <ch> (1-4) ON or OFF.\n"
        "\n"
        "  get <ch>\n"
        "      Print the current state of channel <ch> as:\n"
        "          OK CH=<ch> STATE=<ON|OFF>\n"
        "\n"
        "  getall\n"
        "      Print the full 4-bit mask for all channels as:\n"
        "          OK MASK=0xHH\n"
        "\n"
        "  toggle <ch>\n"
        "      Flip the state of channel <ch> and report the new state.\n"
        "\n"
        "  write-mask 0xHH\n"
        "      Write the low 4 bits of 0xHH directly to the relay mask\n"
        "      (bit 0 -> CH1, bit 1 -> CH2, bit 2 -> CH3, bit 3 -> CH4).\n"
        "\n"
        "  read-mask\n"
        "      Read the current mask from the device and print it as:\n"
        "          OK MASK=0xHH\n"
        "\n"
        "  reset\n"
        "      Turn all channels OFF (mask 0x00) and print the new mask.\n"
        "\n"
        "  ping\n"
        "      Check if the device is available; prints OK or an error.\n"
        "\n"
        "  version\n"
        "      Print the tool and protocol version string.\n"
        "\n"
        "  help\n"
        "      Print this help text.\n"
        "\n"
        "Options:\n"
        "  -d <device>\n"
        "      Override the device path (default: /dev/usbrelay0).\n"
        "\n"
        "  -v\n"
        "      Enable verbose logging to stderr (debug information) while\n"
        "      keeping protocol responses on stdout.\n"
        "\n"
        "  -i\n"
        "      Interactive mode (REPL): read commands from stdin repeatedly\n"
        "      and print a response line for each.\n"
        "\n"
        "Examples:\n"
        "  relayctl set 1 on\n"
        "  relayctl toggle 3\n"
        "  relayctl write-mask 0x05\n"
        "  relayctl getall\n"
        "\n"
    );
}


/* Parse our arguments to determine if user made a valid call */

static int parse_args(int argc, char **argv, struct relayctl_args *out_args) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    memset(out_args, 0, sizeof(*out_args));
    out_args->cmd        = RELAYCTL_CMD_NONE;
    out_args->channel    = 0;
    out_args->state      = RELAYCTL_STATE_OFF;
    out_args->mask       = 0;
    out_args->interactive = 0;
    out_args->verbose     = 0;
    strncpy(out_args->dev_path, USBRELAY_DEFAULT_DEVICE, PATH_MAX - 1);
    out_args->dev_path[PATH_MAX - 1] = '\0';

    // Flag handling for verbose and interactive mode
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-v") == 0) {
            out_args->verbose = 1;
            i++;
        } else if (strcmp(argv[i], "-i") == 0) {
            out_args->interactive = 1;
            i++;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERR BAD_COMMAND -d requires a device path\n");
                return 1;
            }
            strncpy(out_args->dev_path, argv[i + 1], PATH_MAX - 1);
            out_args->dev_path[PATH_MAX - 1] = '\0';
            i += 2;
        } else {
            fprintf(stderr, "ERR BAD_COMMAND Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    /* Now argv[i] should be protocol command */
    if (i >= argc) {
        print_usage();
        return 1;
    }
    const char *cmd = argv[i];
    i++;

    /* Case-insensitive match on command name */
    if (strcasecmp(cmd, "set") == 0) {
        if (i + 1 >= argc) {
            fprintf(stderr, "ERR BAD_COMMAND set requires: set <ch> <on|off>\n");
            return 1;
        }
        out_args->cmd = RELAYCTL_CMD_SET;

        /* Parse channel (argv[i]) */
        if (parse_channel_arg(argv[i], &out_args->channel) != 0) {
            return 1;
        }
        i++;

        /* Parse state (argv[i]) */
        const char *state_str = argv[i];
        if (strcasecmp(state_str, "on") == 0) {
            out_args->state = RELAYCTL_STATE_ON;
        } else if (strcasecmp(state_str, "off") == 0) {
            out_args->state = RELAYCTL_STATE_OFF;
        } else {
            fprintf(stderr, "ERR BAD_STATE State must be ON or OFF\n");
            return 1;
        }
        i++;

    } else if (strcasecmp(cmd, "get") == 0) {
        if (i >= argc) {
            fprintf(stderr, "ERR BAD_COMMAND get requires: get <ch>\n");
            return 1;
        }
        out_args->cmd = RELAYCTL_CMD_GET;

        if (parse_channel_arg(argv[i], &out_args->channel) != 0) {
            return 1;
        }
        i++;

    } else if (strcasecmp(cmd, "getall") == 0) {
        out_args->cmd = RELAYCTL_CMD_GETALL;

    } else if (strcasecmp(cmd, "toggle") == 0) {
        if (i >= argc) {
            fprintf(stderr, "ERR BAD_COMMAND toggle requires: toggle <ch>\n");
            return 1;
        }
        out_args->cmd = RELAYCTL_CMD_TOGGLE;

        if (parse_channel_arg(argv[i], &out_args->channel) != 0) {
            return 1;
        }
        i++;

    } else if (strcasecmp(cmd, "write-mask") == 0) {
        if (i >= argc) {
            fprintf(stderr, "ERR BAD_COMMAND write-mask requires: write-mask 0xHH\n");
            return 1;
        }
        out_args->cmd = RELAYCTL_CMD_WRITE_MASK;

        if (parse_mask_arg(argv[i], &out_args->mask) != 0) {
            return 1;
        }
        i++;

    } else if (strcasecmp(cmd, "read-mask") == 0) {
        out_args->cmd = RELAYCTL_CMD_READ_MASK;

    } else if (strcasecmp(cmd, "reset") == 0) {
        out_args->cmd = RELAYCTL_CMD_RESET;

    } else if (strcasecmp(cmd, "ping") == 0) {
        out_args->cmd = RELAYCTL_CMD_PING;

    } else if (strcasecmp(cmd, "version") == 0) {
        out_args->cmd = RELAYCTL_CMD_VERSION;

    } else if (strcasecmp(cmd, "help") == 0) {
        out_args->cmd = RELAYCTL_CMD_HELP;

    } else {
        fprintf(stderr, "ERR BAD_COMMAND Unknown command: %s\n", cmd);
        return 1;
    }

    if (i < argc) {
        fprintf(stderr, "ERR BAD_COMMAND Unexpected extra arguments\n");
        return 1;
    }

    return 0;
}

/* Helper 1 parse a channel argument "1".."4" */
static int parse_channel_arg(const char *arg, int *out_channel) {
    char *endp = NULL;
    long ch = strtol(arg, &endp, 10);
    if (*arg == '\0' || *endp != '\0' ||
        ch < USBRELAY_MIN_CHANNEL || ch > USBRELAY_MAX_CHANNEL) {
        fprintf(stderr, "ERR BAD_CHANNEL Channel must be 1..4\n");
        return 1;
    }
    *out_channel = (int)ch;
    return 0;
}

/* Helper 2 parse a mask argument "0xHH" (0x00-0x0F) */
static int parse_mask_arg(const char *arg, uint8_t *out_mask) {
    const char *mask_str = arg;
    char *endp = NULL;
    long val = strtol(mask_str, &endp, 0); /* base 0: allows 0x prefix */
    if (*mask_str == '\0' || *endp != '\0' || val < 0 || val > 0xFF) {
        fprintf(stderr, "ERR BAD_MASK Mask must be 0xHH\n");
        return 1;
    }
    if ((uint8_t)val & ~USBRELAY_MASK_ALL) {
        fprintf(stderr, "ERR BAD_MASK Mask must be in range 0x00-0x0F\n");
        return 1;
    }
    *out_mask = (uint8_t)val;
    return 0;
}

/* Helper 3 keep lower 3 bits of relay mask */
static void relay_sanitize_mask(struct relay_context *ctx) {
    ctx->mask &= USBRELAY_MASK_ALL;
}

/* Get the context initialized using the parsed args */
static void relay_context_init(struct relay_context *ctx, const struct relayctl_args *args) {
    ctx->fd  = -1;
    ctx->mask = args->mask;
    strncpy(ctx->dev_path, args->dev_path, PATH_MAX - 1);
    ctx->dev_path[PATH_MAX - 1] = '\0';
    ctx->verbose = args->verbose;
    ctx->interactive = args->interactive;
}

static int relay_open_device(struct relay_context *ctx) {
    int fd = open(ctx->dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "ERR DEVICE_UNAVAILABLE Failed to open device %s (errno=%d)\n", ctx->dev_path, errno);
        return 1;
    }
    ctx->fd = fd;
    return 0;
}

static void relay_close_device(struct relay_context *ctx) {
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

static int relay_read_mask(struct relay_context *ctx)
{
    char buf[1];
    ssize_t ret = read(ctx->fd, buf, 1);
    if (ret == 1) {
        ctx->mask = (unsigned char)buf[0];
        relay_sanitize_mask(ctx);
        return 0;
    }

    fprintf(stderr, "ERR READ_FAILURE Failed to read character driver for device. (errno=%d)\n", errno);
    return 1;
}

static int relay_write_mask(struct relay_context *ctx) {
    char buf[1];

    relay_sanitize_mask(ctx);
    buf[0] = (char)ctx->mask;

    ssize_t ret = write(ctx->fd, buf, 1);
    if (ret == 1) {
        return 0;
    }

    fprintf(stderr,
            "ERR WRITE_FAILURE Failed to write mask to device. (errno=%d)\n",
            errno);
    return 1;
}

static int handle_set(struct relay_context *ctx, const struct relayctl_args *args) {
    int ch = args->channel;
    unsigned int bit;
    const char *state_str;

    if (ch < USBRELAY_MIN_CHANNEL || ch > USBRELAY_MAX_CHANNEL) {
        fprintf(stderr, "ERR BAD_CHANNEL Channel must be 1..4\n");
        return 1;
    }
    if (relay_read_mask(ctx) != 0) {return 1;}

    /* Compute bit for this channel (bit 0 -> CH1, etc.) */
#ifdef USBRELAY_CH_TO_BIT
    bit = USBRELAY_CH_TO_BIT(ch);
#else
    bit = 1U << (ch - 1);
#endif

    if (args->state == RELAYCTL_STATE_ON) {
        ctx->mask |= bit;
        state_str = "ON";
    } else {
        ctx->mask &= ~bit;
        state_str = "OFF";
    }

    if (relay_write_mask(ctx) != 0) {return 1;}

    printf("OK CH=%d STATE=%s\n", ch, state_str);
    return 0;
}



static int handle_get(struct relay_context *ctx, const struct relayctl_args *args) {
    int ch = args->channel;
    unsigned int bit;
    const char *state_str;

    if (ch < USBRELAY_MIN_CHANNEL || ch > USBRELAY_MAX_CHANNEL) {
        fprintf(stderr, "ERR BAD_CHANNEL Channel must be 1..4\n");
        return 1;
    }

    if (relay_read_mask(ctx) != 0) {return 1;}

#ifdef USBRELAY_CH_TO_BIT
    bit = USBRELAY_CH_TO_BIT(ch);
#else
    bit = 1U << (ch - 1);
#endif

    if (ctx->mask & bit) {
        state_str = "ON";
    } else {
        state_str = "OFF";
    }

    printf("OK CH=%d STATE=%s\n", ch, state_str);
    return 0;
}


static int handle_getall(struct relay_context *ctx) {
    if (relay_read_mask(ctx) != 0) {return 1;}
    printf("OK MASK=0x%02X\n", (unsigned int)ctx->mask);
    return 0;
}

static int handle_toggle(struct relay_context *ctx, const struct relayctl_args *args) {
    int ch = args->channel;
    unsigned int bit;
    const char *state_str;

    if (ch < USBRELAY_MIN_CHANNEL || ch > USBRELAY_MAX_CHANNEL) {
        fprintf(stderr, "ERR BAD_CHANNEL Channel must be 1..4\n");
        return 1;
    }
    if (relay_read_mask(ctx) != 0) {return 1;}

#ifdef USBRELAY_CH_TO_BIT
    bit = USBRELAY_CH_TO_BIT(ch);
#else
    bit = 1U << (ch - 1);
#endif
    ctx->mask ^= bit;
    if (relay_write_mask(ctx) != 0) {return 1;}

    if (ctx->mask & bit) {
        state_str = "ON";
    } else {
        state_str = "OFF";
    }
    printf("OK CH=%d STATE=%s\n", ch, state_str);
    return 0;
}

static int handle_write_mask(struct relay_context *ctx, const struct relayctl_args *args) {
    uint8_t m = args->mask;
    if (m & ~USBRELAY_MASK_ALL) {
        fprintf(stderr, "ERR BAD_MASK Mask must be in range 0x00-0x0F\n");
        return 1;
    }

    ctx->mask = m;
    relay_sanitize_mask(ctx);
    if (relay_write_mask(ctx) != 0) {return 1;}

    printf("OK MASK=0x%02X\n", (unsigned int)ctx->mask);
    return 0;
}

static int handle_read_mask(struct relay_context *ctx)
{
    if (relay_read_mask(ctx) != 0) {return 1;}

    printf("OK MASK=0x%02X\n", (unsigned int)ctx->mask);
    return 0;
}

static int handle_reset(struct relay_context *ctx) {
    ctx->mask = 0x00;
    if (relay_write_mask(ctx) != 0) {
        return 1;
    }
    printf("OK MASK=0x00\n");
    return 0;
}

static int handle_ping(struct relay_context *ctx) {
    if (relay_read_mask(ctx) == 0) {
        printf("OK\n");
        return 0;
    }

    fprintf(stderr, "ERR DEVICE_UNAVAILABLE Unable to communicate with device\n");
    return 1;
}


static int handle_version(void) {
    printf("OK VERSION=%s TOOL=relayctl/%s\n",
           USBRELAY_PROTO_VERSION,
           RELAYCTL_TOOL_VERSION);
    return 0;
}

static int handle_help(void) {
    print_help();
    /* You could also add: printf("OK\n"); if you want strict protocol lines */
    return 0;
}

static int run_interactive(struct relay_context *ctx) {
    char line[USBRELAY_MAX_LINE_LEN];
    int exit_status = 0;

    for (;;) {
        if (ctx->verbose) {
            fputs("> ", stdout);
            fflush(stdout);
        }

        /* Read one line from stdin; EOF -> exit loop */
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        /* Trim leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            continue;
        }

        if (strcasecmp(p, "quit\n") == 0 || strcasecmp(p, "quit") == 0 ||
            strcasecmp(p, "exit\n") == 0 || strcasecmp(p, "exit") == 0) {
            break;
        }

        /* Tokenize the line */
        char *argv_buf[16];
        int argc_buf = 0;
        char *token = strtok(p, " \t\r\n");
        while (token && argc_buf < (int)(sizeof(argv_buf) / sizeof(argv_buf[0]))) {
            argv_buf[argc_buf++] = token;
            token = strtok(NULL, " \t\r\n");
        }
        if (argc_buf == 0) {
            continue;
        }

        char *fake_argv[1 + 16];
        int fake_argc = argc_buf + 1;
        fake_argv[0] = (char *)"relayctl";
        for (int i = 0; i < argc_buf; i++) {
            fake_argv[i + 1] = argv_buf[i];
        }

        struct relayctl_args args;
        int rc = parse_args(fake_argc, fake_argv, &args);
        if (rc != 0) {
            exit_status = rc;
            continue;
        }

        // Same usual handlers
        switch (args.cmd) {
        case RELAYCTL_CMD_SET:
            rc = handle_set(ctx, &args);
            break;
        case RELAYCTL_CMD_GET:
            rc = handle_get(ctx, &args);
            break;
        case RELAYCTL_CMD_GETALL:
            rc = handle_getall(ctx);
            break;
        case RELAYCTL_CMD_TOGGLE:
            rc = handle_toggle(ctx, &args);
            break;
        case RELAYCTL_CMD_WRITE_MASK:
            rc = handle_write_mask(ctx, &args);
            break;
        case RELAYCTL_CMD_READ_MASK:
            rc = handle_read_mask(ctx);
            break;
        case RELAYCTL_CMD_RESET:
            rc = handle_reset(ctx);
            break;
        case RELAYCTL_CMD_PING:
            rc = handle_ping(ctx);
            break;
        case RELAYCTL_CMD_VERSION:
            rc = handle_version();
            break;
        case RELAYCTL_CMD_HELP:
            rc = handle_help();
            break;
        default:
            fprintf(stderr,
                    "ERR INTERNAL_ERROR Unknown command in interactive mode\n");
            rc = 1;
            break;
        }
        if (rc != 0) {
            exit_status = rc;
        }
    }
    return exit_status;
}

int main(int argc, char **argv) {
    struct relayctl_args args;
    struct relay_context ctx;
    int ret = 0;

    /* 1. Parse command-line arguments */
    ret = parse_args(argc, argv, &args);
    if (ret != 0) {
        return ret;
    }

    /* 2. Handle commands that do not require device access */
    if (args.cmd == RELAYCTL_CMD_HELP) {
        return handle_help();
    }

    if (args.cmd == RELAYCTL_CMD_VERSION) {
        return handle_version();
    }

    /* 3. Initialize context from parsed arguments */
    relay_context_init(&ctx, &args);

    /* 4. Open the relay device for all other commands */
    if (relay_open_device(&ctx) != 0) {
        return 2; /* device-related error code */
    }

    /* 5. Interactive (REPL) mode, if requested */
    if (args.interactive) {
        ret = run_interactive(&ctx);
        relay_close_device(&ctx);
        return ret;
    }

    /* 6. One-shot command dispatch */
    switch (args.cmd) {
    case RELAYCTL_CMD_SET:
        ret = handle_set(&ctx, &args);
        break;
    case RELAYCTL_CMD_GET:
        ret = handle_get(&ctx, &args);
        break;
    case RELAYCTL_CMD_GETALL:
        ret = handle_getall(&ctx);
        break;
    case RELAYCTL_CMD_TOGGLE:
        ret = handle_toggle(&ctx, &args);
        break;
    case RELAYCTL_CMD_WRITE_MASK:
        ret = handle_write_mask(&ctx, &args);
        break;
    case RELAYCTL_CMD_READ_MASK:
        ret = handle_read_mask(&ctx);
        break;
    case RELAYCTL_CMD_RESET:
        ret = handle_reset(&ctx);
        break;
    case RELAYCTL_CMD_PING:
        ret = handle_ping(&ctx);
        break;

    /* HELP / VERSION already handled above; NONE is a bug */
    case RELAYCTL_CMD_HELP:
    case RELAYCTL_CMD_VERSION:
    case RELAYCTL_CMD_NONE:
    default:
        fprintf(stderr,
                "ERR INTERNAL_ERROR Unknown or unsupported command in main\n");
        ret = 3;
        break;
    }

    /* 7. Clean up and exit */
    relay_close_device(&ctx);
    return ret;
}
