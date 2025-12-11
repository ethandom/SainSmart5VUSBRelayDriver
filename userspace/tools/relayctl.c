/* relayctl.c - User-space controller for SainSmart 4-Channel USB Relay
 *
 * High-level skeleton ONLY.
 * This file outlines the structure, functions, and responsibilities
 * without providing actual implementation code.
 *
 * Responsibilities:
 *   - Parse CLI arguments.
 *   - Open the relay device (e.g. /dev/usbrelay0).
 *   - Maintain a shadow 4-bit mask for relay state.
 *   - Perform operations like SET/GET/TOGGLE/WRITE MASK, etc.
 *   - Print ASCII protocol responses (OK / ERR ...).
 */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>  
#include <strings.h>  
#include <stdint.h>
#include <limits.h>
#define PATH_MAX    128


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
    enum relayctl_cmd  cmd;        /* which high-level command */
    int                channel;    /* channel number for channel-based commands (1..4 or 0) */
    enum relayctl_state state;     /* ON/OFF for set, if relevant */
    uint8_t            mask;       /* mask for write-mask, if relevant */
    char               dev_path[PATH_MAX]; /* device path override, if provided */
    int                interactive; /* nonzero if -i / interactive requested */
    int                verbose;     /* nonzero if verbose mode requested */
};

/* Help messages on failure */
static void print_usage(void)
{
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
static void print_help(void)
{
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

static int parse_args(int argc, char **argv, struct relayctl_args *out_args)
{
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
    strncpy(out_args->dev_path, "/dev/usbrelay0", PATH_MAX - 1);
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
static int parse_channel_arg(const char *arg, int *out_channel)
{
    char *endp = NULL;
    long ch = strtol(arg, &endp, 10);
    if (*arg == '\0' || *endp != '\0' || ch < 1 || ch > 4) {
        fprintf(stderr, "ERR BAD_CHANNEL Channel must be 1..4\n");
        return 1;
    }
    *out_channel = (int)ch;
    return 0;
}

/* Helper 2 parse a mask argument "0xHH" (0x00-0x0F) */
static int parse_mask_arg(const char *arg, uint8_t *out_mask)
{
    const char *mask_str = arg;
    char *endp = NULL;
    long val = strtol(mask_str, &endp, 0); /* base 0: allows 0x prefix */
    if (*mask_str == '\0' || *endp != '\0' || val < 0 || val > 0xFF) {
        fprintf(stderr, "ERR BAD_MASK Mask must be 0xHH\n");
        return 1;
    }
    if (val & ~0x0F) {
        fprintf(stderr, "ERR BAD_MASK Mask must be in range 0x00-0x0F\n");
        return 1;
    }
    *out_mask = (uint8_t)val;
    return 0;
}

/* Get the context initialized using the parsed args */
static void relay_context_init(struct relay_context *ctx, const struct relayctl_args *args)
{
    ctx->fd  = -1;
    ctx->mask = args->mask;
    strncpy(ctx->dev_path, args->dev_path, PATH_MAX - 1);
    ctx->dev_path[PATH_MAX - 1] = '\0';
    ctx->verbose = args->verbose;
    ctx->interactive = args->interactive;
}


/* Open the device described in ctx->dev_path.
 *
 * Responsibilities:
 *   - Open the device node with the desired flags.
 *   - On success, store a valid descriptor into ctx->fd.
 *   - On failure, print an error (optionally in the ASCII error format) and
 *     return a nonzero status.
 */
static int relay_open_device(struct relay_context *ctx);

/* Close the device if it is open.
 *
 * Responsibilities:
 *   - If ctx->fd is valid, close it.
 *   - Clear or reset ctx->fd.
 */
static void relay_close_device(struct relay_context *ctx);

/* ============================================================
 * 7. Low-level mask helpers (kernel ABI)
 * ============================================================ */

/* Read current mask from the kernel driver.
 *
 * Responsibilities:
 *   - Issue a 1-byte read on ctx->fd.
 *   - Update ctx->mask with the returned value (0x00-0xFF).
 *   - Optionally mask to 4 bits if desired.
 *   - Return 0 on success, nonzero on failure.
 */
static int relay_read_mask(struct relay_context *ctx);

/* Write current mask to the kernel driver.
 *
 * Responsibilities:
 *   - Ensure ctx->mask is constrained to lower 4 bits (0x0F).
 *   - Issue a 1-byte write on ctx->fd.
 *   - Return 0 on success, nonzero on failure.
 */
static int relay_write_mask(struct relay_context *ctx);

/* Constrain ctx->mask to valid 4-bit mask (0x0F).
 *
 * Responsibilities:
 *   - Apply bitwise AND so that only bits 0-3 remain.
 */
static void relay_sanitize_mask(struct relay_context *ctx);

/* ============================================================
 * 8. High-level command handlers
 * ============================================================ */

/* Set a single channel ON or OFF.
 *
 * Responsibilities:
 *   - Validate channel is in range 1..4.
 *   - Optionally call relay_read_mask to refresh ctx->mask.
 *   - Set or clear the corresponding bit in ctx->mask based on
 *     requested state.
 *   - Call relay_write_mask to apply new mask to device.
 *   - Print an ASCII protocol response ("OK CH=<n> STATE=<ON|OFF>" or
 *     "ERR BAD_CHANNEL ..." etc.).
 *   - Return an appropriate exit code.
 */
static int handle_set(struct relay_context *ctx,
                      const struct relayctl_args *args);

/* Get the state of a single channel.
 *
 * Responsibilities:
 *   - Validate channel.
 *   - Call relay_read_mask to refresh ctx->mask.
 *   - Compute state from ctx->mask.
 *   - Print "OK CH=<n> STATE=<ON|OFF>" or an error.
 */
static int handle_get(struct relay_context *ctx,
                      const struct relayctl_args *args);

/* Get the state of all channels (mask).
 *
 * Responsibilities:
 *   - Call relay_read_mask.
 *   - Print "OK MASK=0xHH".
 */
static int handle_getall(struct relay_context *ctx);

/* Toggle a single channel.
 *
 * Responsibilities:
 *   - Validate channel.
 *   - Call relay_read_mask.
 *   - Flip the relevant bit in ctx->mask.
 *   - Call relay_write_mask.
 *   - Print "OK CH=<n> STATE=<ON|OFF>" using the new state.
 */
static int handle_toggle(struct relay_context *ctx,
                         const struct relayctl_args *args);

/* Write a full mask provided by the user.
 *
 * Responsibilities:
 *   - Take mask from parsed args.
 *   - Ensure it is in the valid range (0x00-0x0F).
 *   - Set ctx->mask accordingly and call relay_write_mask.
 *   - Print "OK MASK=0xHH" or appropriate error.
 */
static int handle_write_mask(struct relay_context *ctx,
                             const struct relayctl_args *args);

/* Read the current mask from the device.
 *
 * Responsibilities:
 *   - Call relay_read_mask.
 *   - Print "OK MASK=0xHH" or error.
 */
static int handle_read_mask(struct relay_context *ctx);

/* Reset all channels to OFF.
 *
 * Responsibilities:
 *   - Set ctx->mask to 0.
 *   - Call relay_write_mask.
 *   - Print "OK MASK=0x00" or error.
 */
static int handle_reset(struct relay_context *ctx);

/* Ping the device.
 *
 * Responsibilities:
 *   - Perform a minimal, non-destructive check that device is alive.
 *   - For example, attempt a read or use an existing mask.
 *   - If device is responsive: print "OK".
 *   - On failure: print "ERR DEVICE_UNAVAILABLE ..." and return nonzero.
 */
static int handle_ping(struct relay_context *ctx);

/* Print version information.
 *
 * Responsibilities:
 *   - Print something like "OK VERSION=1.1 TOOL=relayctl/x.y".
 *   - No device access required.
 */
static int handle_version(void);

/* Print help.
 *
 * Responsibilities:
 *   - Print the same or more detailed information as print_usage.
 *   - Can print "OK" or just human-readable help text.
 */
static int handle_help(void);

/* ============================================================
 * 9. Optional: Interactive / REPL mode
 * ============================================================ */

/* Run a read-eval-print loop on stdin/stdout.
 *
 * Responsibilities:
 *   - Continuously read lines from stdin.
 *   - For each line:
 *       - Parse it as an ASCII protocol command.
 *       - Map to the same handlers (set/get/etc.) using the
 *         internal logic.
 *       - Print a single response line for each input line.
 *   - Exit loop on EOF or explicit quit command (if you choose to support).
 */
static int run_interactive(struct relay_context *ctx);

/* ============================================================
 * 10. Main entry point
 * ============================================================ */

/* main()
 *
 * Responsibilities:
 *   - Create relayctl_args and relay_context structures.
 *   - Call parse_args to interpret command-line arguments.
 *   - If parsing fails or help/version is requested, call the appropriate
 *     handler and exit.
 *   - Initialize relay_context from parsed arguments.
 *   - Open device where required (most commands except version/help).
 *   - Dispatch to the appropriate handler based on relayctl_args.cmd:
 *       - handle_set / handle_get / handle_getall / handle_toggle /
 *         handle_write_mask / handle_read_mask / handle_reset /
 *         handle_ping / handle_version / handle_help
 *   - For interactive mode, call run_interactive instead of a single handler.
 *   - Close device before exiting.
 *   - Return an exit status code:
 *       0 on success,
 *       nonzero on errors (argument errors, device errors, etc.).
 */
int main(int argc, char **argv);
