#include "https.h"
#include <string.h>
#include <time.h>

#include "hardware/watchdog.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "multi_printf.h"

typedef struct TLS_CLIENT_T_ {
    struct altcp_pcb *pcb;
    bool complete;
    int error;
    const char *http_request;
    size_t http_request_len;
    int timeout;
} TLS_CLIENT_T;

static struct altcp_tls_config *tls_config = NULL;

static err_t tls_client_close(void *arg) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;
    err_t err = ERR_OK;

    state->complete = true;
    if (state->pcb != NULL) {
        altcp_arg(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);
        err = altcp_close(state->pcb);
        if (err != ERR_OK) {
            multi_printf("close failed %d, calling abort\n", err);
            altcp_abort(state->pcb);
            err = ERR_ABRT;
        }
        state->pcb = NULL;
    }
    return err;
}

static err_t tls_client_connected(void *arg, struct altcp_pcb *pcb, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;
    if (err != ERR_OK) {
        multi_printf("connect failed %d\n", err);
        state->error = (int) err;
        return tls_client_close(state);
    }

    multi_printf("connected to server, sending request\n");
    err = altcp_write(state->pcb, state->http_request, state->http_request_len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        multi_printf("error writing data, err=%d", err);
        state->error = (int) err;
        return tls_client_close(state);
    }

    return ERR_OK;
}

static err_t tls_client_poll(void *arg, struct altcp_pcb *pcb) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;
    multi_printf("timed out\n");
    state->error = PICO_ERROR_TIMEOUT;
    return tls_client_close(arg);
}

static void tls_client_err(void *arg, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;
    multi_printf("tls_client_err %d\n", err);
    tls_client_close(state);
    state->error = PICO_ERROR_GENERIC;
}

static err_t tls_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;
    if (!p) {
        multi_printf("connection closed\n");
        return tls_client_close(state);
    }

    if (p->tot_len > 0) {
        /* For simplicity this examples creates a buffer on stack the size of the data pending here,
           and copies all the data to it in one go.
           Do be aware that the amount of data can potentially be a bit large (TLS record size can be 16 KB),
           so you may want to use a smaller fixed size buffer and copy the data to it using a loop, if memory is a concern */
        char buf[p->tot_len + 1];

        pbuf_copy_partial(p, buf, p->tot_len, 0);
        buf[p->tot_len] = 0;

        printf("***\nnew data received from server:\n***\n\n%s\n", buf);

        altcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);

    return ERR_OK;
}

static void tls_client_connect_to_server_ip(const ip_addr_t *ipaddr, TLS_CLIENT_T *state) {
    err_t err;
    u16_t port = 443;

    multi_printf("connecting to server IP %s port %d\n", ipaddr_ntoa(ipaddr), port);
    err = altcp_connect(state->pcb, ipaddr, port, tls_client_connected);
    if (err != ERR_OK) {
        multi_printf("error initiating connect, err=%d\n", err);
        state->error = (int) err;
        tls_client_close(state);
    }
}

static void tls_client_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr) {
        multi_printf("DNS resolving complete\n");
        tls_client_connect_to_server_ip(ipaddr, (TLS_CLIENT_T *) arg);
    } else {
        multi_printf("error resolving hostname %s\n", hostname);
        ((TLS_CLIENT_T *) arg)->error = PICO_ERROR_NO_DATA;
        tls_client_close(arg);
    }
}


static bool tls_client_open(const char *hostname, void *arg) {
    err_t err;
    ip_addr_t server_ip;
    TLS_CLIENT_T *state = (TLS_CLIENT_T *) arg;

    state->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
    if (!state->pcb) {
        multi_printf("failed to create pcb\n");
        return false;
    }

    altcp_arg(state->pcb, state);
    altcp_poll(state->pcb, tls_client_poll, state->timeout * 2);
    altcp_recv(state->pcb, tls_client_recv);
    altcp_err(state->pcb, tls_client_err);

    /* Set SNI */
    mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), hostname);

    multi_printf("resolving %s\n", hostname);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();

    err = dns_gethostbyname(hostname, &server_ip, tls_client_dns_found, state);
    if (err == ERR_OK) {
        /* host is in DNS cache */
        tls_client_connect_to_server_ip(&server_ip, state);
    } else if (err != ERR_INPROGRESS) {
        multi_printf("error initiating DNS resolving, err=%d\n", err);
        tls_client_close(state->pcb);
    }

    cyw43_arch_lwip_end();

    return err == ERR_OK || err == ERR_INPROGRESS;
}

// Perform initialisation
static TLS_CLIENT_T *tls_client_init(void) {
    TLS_CLIENT_T *state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!state) {
        multi_printf("failed to allocate state\n");
        return NULL;
    }

    return state;
}

bool send_https_request(const uint8_t *cert, size_t cert_len, const char *server, const char *request,
                        size_t request_len, int timeout) {

    tls_config = altcp_tls_create_config_client(cert, cert_len);
    assert(tls_config);

    /* No CA certificate checking */
    //mbedtls_ssl_conf_authmode(&tls_config->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

    TLS_CLIENT_T *state = tls_client_init();
    if (!state) {
        return false;
    }
    state->http_request = request;
    state->http_request_len = request_len;
    state->timeout = timeout;
    if (!tls_client_open(server, state)) {
        return false;
    }
    while (!state->complete) {
        watchdog_update();
    }
    int err = state->error;
    free(state);
    altcp_tls_free_config(tls_config);
    return err == 0;
}