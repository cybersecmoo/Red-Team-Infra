#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    ssh_session sesh;
    int returnCode;
    int verbosity = SSH_LOG_PROTOCOL;
    int port = 22;

    if(argc > 1) {
        port = atoi(argv[1]);
        printf("Port set to %d\n", port);
    }

    sesh = ssh_new();

    if(sesh == NULL) {
        fprintf(stderr, "*** ERROR: Failed to create session ***\n");
        exit(-1);
    }

    // TODO: pass in host as argument
    ssh_options_set(sesh, SSH_OPTIONS_HOST, "localhost");
    ssh_options_set(sesh, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    ssh_options_set(sesh, SSH_OPTIONS_PORT, &port);

    returnCode = ssh_connect(sesh);

    if(returnCode != SSH_OK) {
        fprintf(stderr, "*** Error connecting to SSH server: %s ***\n", ssh_get_error(sesh));
        exit(-1);
    }

    // We always trust the server, so let's add it to the known hosts
    returnCode = ssh_write_knownhost(sesh);

    printf("Authenticated the server\n");

    ssh_disconnect(sesh);
    ssh_free(sesh);
}