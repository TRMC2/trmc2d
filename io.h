// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Network and IO functions for trmc2d. Mostly stolen from fieldd.
 */

/*
 * Maximum number of network clients. We don't want to have lots of
 * clients connected to an instrument. Use a CGI if you want the
 * Internet to know about the temperature of the cryostat.
 */
#define MAX_CLIENTS 5

#define COMMAND_LENGTH 1024  /* max length of an accepted command */

/* Description of a client. */
typedef struct {
    unsigned int active: 1;     /* 1 if this slot is used */
    unsigned int autoflush: 1;  /* for tty clients only */
    unsigned int verbose: 1;    /* opted-in for verbose mode */
    unsigned int quitting: 1;   /* wants to quit */
    int in;                     /* fd for reading */
    int out;                    /* fd for writing */
    size_t input_pending;       /* number of read bytes not processed */
    char input_buffer[COMMAND_LENGTH];  /* NUL-terminated */
    size_t output_pending;      /* number of bytes pending */
    char output_buffer[4096];
} client_t;

/* Array of clients. */
extern client_t client[MAX_CLIENTS];

/* Get a free slot from the above array. */
client_t *get_client_slot(void);

/* Get a listening socket. */
int get_socket(int domain, int port, const char *name);

/*
 * Get a command from the input buffer. The caller should allocate
 * COMMAND_LENGTH bytes for `command'. Returns `command' or NULL if
 * there is no complete buffered command.
 */
char *get_command(client_t *cl, char *command);

/* Queue message in the client output buffer. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
void queue_output(client_t *cl, const char *fmt, ...);

/*
 * The following functions may block. Use them only when select() says
 * cl-in (resp. cl->out) is ready for input (resp. output).
 */

/*
 * Read bytes from the client.
 * Returns the number of bytes read, 0 on disconnect.
 */
int process_input(client_t *cl);

/* Send pending output to the client. */
void process_output(client_t *cl);
