
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pty.h>
#include <poll.h>

static int copyChannelToFD(ssh_session sesh, ssh_channel chan, void *data, uint32_t len, int isStderr, void *userData) {
    printf("Chan to FD\n");
    
    int fileDesc = *(int *)userData;
    int size;
    (void)sesh;
    (void)chan;
    (void)isStderr;

    size = write(fileDesc, data, len);

    return size;
}

static int copyFDToChannel(socket_t fileDesc, int rEvents, void *userData) {
    printf("Copy FD to Chan\n");
    ssh_channel chan = (ssh_channel)userData;
    char buff[2048];
    int size = 0;

    if(!chan) {
        close(fileDesc);
        printf("blergh\n");

        return -1;
    }

    if(rEvents & POLLIN) {
        printf("Reading...\n");
        size = read(fileDesc, buff, 2048);

        if(size > 0) {
            printf("Writing...\n");
            ssh_channel_write(chan, buff, size);
        }
    }

    if(rEvents & POLLHUP) {
        printf("HUP\n");
        ssh_channel_close(chan);
        size = -1;
    }

    return size;
}

static void channelClose(ssh_session sesh, ssh_channel chan, void *userData) {
    int fileDesc = *(int *)userData;
    (void)sesh;
    (void)chan;

    close(fileDesc);
}

struct ssh_channel_callbacks_struct callbacks = {
    .channel_data_function = copyChannelToFD,
    .channel_eof_function = channelClose,
    .channel_close_function = channelClose,
    .userdata = NULL
};

static int authPassword(const char *user, const char *password)
{
    int returnCode = 1;

    if(strcmp(user,"username"))
    {
        returnCode = 0;
    }

    if(strcmp(password,"password"))
    {
        returnCode = 0;
    }

    return returnCode;
}

static void sendDefaultAuthReply(ssh_message message) {
    ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE);
    ssh_message_reply_default(message);
}

static int mainLoop(ssh_channel chan)
{
    ssh_session sesh = ssh_channel_get_session(chan);
    socket_t fileDesc;
    struct termios *term = NULL;
    struct winsize *win = NULL;
    pid_t childPID;
    ssh_event event;
    short events;

    printf("Setting up PTY child process...\n");
    childPID = forkpty(&fileDesc, NULL, term, win);

    if(childPID == 0) {
        execl("/bin/bash", "/bin/bash", (char *)NULL);
        abort();
    }

    callbacks.userdata = &fileDesc;
    ssh_callbacks_init(&callbacks);
    ssh_set_channel_callbacks(chan, &callbacks);

    printf("Callbacks set up\n");

    events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
    event = ssh_event_new();

    if(event == NULL) {
        printf("Couldn't get event\n");
        return -1;
    }

    if(ssh_event_add_fd(event, fileDesc, events, copyFDToChannel, chan) != SSH_OK) {
        printf("Couldn't add file desc\n");
        return -1;
    }

    if(ssh_event_add_session(event, sesh) != SSH_OK) {
        printf("Couldn't add sesh\n");
        return -1;
    }

    printf("polling...\n");

    do {
        ssh_event_dopoll(event, 1000);
    } while(!ssh_channel_is_closed(chan));

    printf("Closing file descs\n");
    ssh_event_remove_fd(event, fileDesc);
    ssh_event_remove_session(event, sesh);
    ssh_event_free(event);

    return 0;
}

int main(int argc, char **argv){
    ssh_session session;
    ssh_bind sshbind;
    ssh_message message;
    ssh_channel chan = 0;
    char buf[2048];
    int auth = 0;
    int shell = 0;
    int i;
    int r;

    sshbind = ssh_bind_new();
    session = ssh_new();
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, "2222");
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, "host_key");

    if(ssh_bind_listen(sshbind)<0)
    {
        printf("Error listening to socket: %s\n",ssh_get_error(sshbind));
        return 1;
    }

    r = ssh_bind_accept(sshbind,session);

    if(r == SSH_ERROR)
    {
      printf("error accepting a connection : %s\n",ssh_get_error(sshbind));
      return 1;
    }

    if(ssh_handle_key_exchange(session)) 
    {
        printf("ssh_handle_key_exchange: %s\n", ssh_get_error(session));
        return 1;
    }

    do 
    {
        message=ssh_message_get(session);
        if(!message)
            break;

        switch(ssh_message_type(message)){
            case SSH_REQUEST_AUTH:
                switch(ssh_message_subtype(message)){
                    case SSH_AUTH_METHOD_PASSWORD:
                        if(authPassword(ssh_message_auth_user(message),
                           ssh_message_auth_password(message))){
                               auth=1;
                               ssh_message_auth_reply_success(message, 0);
                               ssh_message_free(message);

                               break;
                           }

                    default:
                        sendDefaultAuthReply(message);
                        break;
                }

                break;

            default:
                sendDefaultAuthReply(message);
        }

        ssh_message_free(message);
    } while (!auth);

    if(!auth) {
        printf("auth error: %s\n",ssh_get_error(session));
        ssh_disconnect(session);
        return 1;
    }

    do {
        message = ssh_message_get(session);

        if(message) {
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
                    ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
                chan = ssh_message_channel_request_open_reply_accept(message);
                ssh_message_free(message);
                break;
            }

            else {
                ssh_message_reply_default(message);
                ssh_message_free(message);
            }
        } 
        
        else {
            break;
        }
    } while(!chan);

    if(!chan) {
        printf("error : %s\n", ssh_get_error(session));
        ssh_finalize();
        return 1;
    }

    do {
        message = ssh_message_get(session);

        if(message) {
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL) {
                if(ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
                    shell=1;
                    ssh_message_channel_request_reply_success(message);
                    ssh_message_free(message);
                    break;
                }

                else if(ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_PTY) {
                    ssh_message_channel_request_reply_success(message);
                    ssh_message_free(message);
                    continue;
                }
            }

            ssh_message_reply_default(message);
            ssh_message_free(message);
        }

        else {
            break;
        }
        

    } while(!shell);

    if(!shell) {
        printf("error : %s\n",ssh_get_error(session));
        return 1;
    }

    printf("it works!\n");

    mainLoop(chan);

    ssh_disconnect(session);
    ssh_bind_free(sshbind);
    ssh_finalize();

    return 0;
}

