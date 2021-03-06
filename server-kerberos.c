#include <arpa/inet.h>
#include <errno.h>
#include <gssapi.h>
#include <gssapi/gssapi_ext.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>

#include "shared.h"
#include "server-kerberos.h"

int
do_establish_server_context(gss_ctx_id_t *ctx_handle,
                            gss_cred_id_t server_creds, int client_socket)
{
    OM_uint32 maj_stat = 0;
    OM_uint32 min_stat = 0;
    OM_uint32 ret_flags = 0;

    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;

    int exit_out = 0;

    gss_channel_bindings_t cb = GSS_C_NO_CHANNEL_BINDINGS;

    char *ip = "192.168.122.49";
    char *app_data = "magic";

    cb = calloc(sizeof(struct gss_channel_bindings_struct), 1);
    cb->initiator_addrtype = GSS_C_AF_NULLADDR;
    cb->initiator_address.length = 0;
    cb->acceptor_addrtype = GSS_C_AF_INET;
    cb->acceptor_address.length = strlen(ip);
    cb->acceptor_address.value = ip;
    cb->application_data.length = strlen(app_data);
    cb->application_data.value = app_data;

    maj_stat = gss_create_sec_context(&min_stat, ctx_handle);
    assert(maj_stat == GSS_S_COMPLETE);

    maj_stat = gss_set_context_flags(&min_stat, *ctx_handle, 0, 0xFFFFFFFF);
    assert(maj_stat == GSS_S_COMPLETE);

    do {
        receive_token_from_peer(&input_token, client_socket);

        maj_stat = gss_accept_sec_context(&min_stat, ctx_handle, server_creds,
                                          &input_token,
                                          cb,
                                          NULL, NULL, &output_token, &ret_flags,
                                          NULL, NULL);

        if (GSS_ERROR(maj_stat)) {
            print_error(maj_stat, min_stat);
            if ((maj_stat & GSS_S_BAD_BINDINGS) == 0) {
                exit_out = 1;
                goto cleanup;
            }
        }

        if (output_token.length != 0) {
            if (send_token_to_peer(&output_token, client_socket) != 0) {
                exit_out = 2;
                goto cleanup;
            }

            maj_stat = gss_release_buffer(&min_stat, &output_token);
            output_token.length = 0;
        }
    } while (maj_stat & GSS_S_CONTINUE_NEEDED);

    if (*ctx_handle == GSS_C_NO_CONTEXT) {
        printf("Still no context... but done?\n");
    }

    printf("ret_flags: %u, %u\n", ret_flags, ret_flags & 2048);

cleanup:
    maj_stat = gss_release_buffer(&min_stat, &input_token);
    maj_stat = gss_release_buffer(&min_stat, &output_token);

    return exit_out;
}


int
do_print_context_names(gss_ctx_id_t ctx_handle)
{
    OM_uint32 maj_stat;
    OM_uint32 min_stat;

    gss_name_t src_name = GSS_C_NO_NAME;
    gss_name_t target_name = GSS_C_NO_NAME;
    gss_buffer_desc exported_src_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc exported_target_name = GSS_C_EMPTY_BUFFER;

    int exit_out = 0;

    maj_stat = gss_inquire_context(&min_stat, ctx_handle, &src_name,
                                   &target_name, NULL, NULL, NULL, NULL, NULL);
    if (GSS_ERROR(maj_stat)) {
        print_error(maj_stat, min_stat);
        exit_out = 1;
        goto cleanup;
    }


    maj_stat = gss_display_name(&min_stat, src_name, &exported_src_name, NULL);
    if (GSS_ERROR(maj_stat)) {
        print_error(maj_stat, min_stat);
        exit_out = 2;
        goto cleanup;
    }

    printf("Source Name (%zu): %s\n", exported_src_name.length,
           (char *)exported_src_name.value);

    maj_stat = gss_display_name(&min_stat, target_name, &exported_target_name,
                                NULL);
    if (GSS_ERROR(maj_stat)) {
        print_error(maj_stat, min_stat);
        exit_out = 3;
        goto cleanup;
    }

    printf("Target Name (%zu): %s\n", exported_target_name.length,
           (char *)exported_target_name.value);

cleanup:
    maj_stat = gss_release_name(&min_stat, &src_name);
    maj_stat = gss_release_name(&min_stat, &target_name);

    maj_stat = gss_release_buffer(&min_stat, &exported_src_name);
    maj_stat = gss_release_buffer(&min_stat, &exported_target_name);

    return exit_out;
}
