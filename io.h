/*
 * Network and IO functions for tempd. Mostly stolen from fieldd.
 */

/*
 * Maximum number of network clients. We don't want to have lots of
 * clients connected to an instrument. Use a CGI if you want the
 * Internet to know about the temperature of the cryostat.
 */
#define MAX_CLIENTS 5

/* Description of a client. */
typedef struct {
    int active;                 /* 1 if this slot is used */
    int in;                     /* fd for reading */
    int out;                    /* fd for writing */
    size_t output_pending;      /* number of bytes pending */
    char output_buffer[4096];
    int autoflush;              /* for tty clients only */
} client_t;

/* Array of clients. */
extern client_t client[MAX_CLIENTS];

/* Get a free slot from the above array. */
client_t *get_client_slot(void);

/* Get a listening socket. */
int get_socket(int domain, int port, const char *name);

/* Queue message in the client output buffer. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
void queue_output(client_t *cl, const char *fmt, ...);

/*
 * The following functions may block. Use them only when select() says
 * cl-in (resp. cl->out) is ready for input (resp. output).
 */

/* Get input from the client. */
char *process_input(client_t *cl);

/* Send pending output to the client. */
void process_output(client_t *cl);
