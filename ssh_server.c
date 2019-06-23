
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int auth_password(const char *user, const char *password)
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

    return returnCode; // authenticated
}

static int mainLoop(ssh_channel chan)
{
    ssh_session sesh = ssh_channel_get_session(chan);
    socket_t fileDesc;
    
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
                        if(auth_password(ssh_message_auth_user(message),
                           ssh_message_auth_password(message))){
                               auth=1;
                               ssh_message_auth_reply_success(message,0);
                               break;
                           }
                        // not authenticated, send default message
                    case SSH_AUTH_METHOD_NONE:
                    default:
                        ssh_message_auth_set_methods(message,SSH_AUTH_METHOD_PASSWORD);
                        ssh_message_reply_default(message);
                        break;
                }
                break;
            default:
                ssh_message_reply_default(message);
        }
        ssh_message_free(message);
    } while (!auth);

    if(!auth)
    {
        printf("auth error: %s\n",ssh_get_error(session));
        ssh_disconnect(session);
        return 1;
    }

    do
    {
        message=ssh_message_get(session);

        if(message){
            if(ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
                ssh_message_subtype(message)==SSH_CHANNEL_SESSION)
            {
                chan=ssh_message_channel_request_open_reply_accept(message);
                break;
            }

            else
            {
                ssh_message_reply_default(message);
            }

            ssh_message_free(message);
        } 
        
        else
        {
            break;
        }
    } while(!chan);

    if(!chan)
    {
        printf("error : %s\n",ssh_get_error(session));
        ssh_finalize();
        return 1;
    }

    do 
    {
        message=ssh_message_get(session);

        if(message)
        {
            if(ssh_message_type(message)==SSH_REQUEST_CHANNEL)
            {
                if(ssh_message_subtype(message)==SSH_CHANNEL_REQUEST_PTY)
                {
                    ssh_message_channel_request_reply_success(message);
                    ssh_message_free(message);
                    continue;
                }

                else if(ssh_message_subtype(message)==SSH_CHANNEL_REQUEST_SHELL)
                {
                    shell=1;
                    ssh_message_channel_request_reply_success(message);
                    ssh_message_free(message);
                    break;
                }
            }

            ssh_message_reply_default(message);
            ssh_message_free(message);
        }

        else
        {
            break;
        }
        

    } while(!shell);

    if(!shell)
    {
        printf("error : %s\n",ssh_get_error(session));
        return 1;
    }

    printf("it works!\n");

    do
    {
        i = ssh_channel_read(chan,buf, 2048, 0);

        if(i > 0)
        {
            ssh_channel_write(chan, buf, i);

            if(write(1, buf, i) < 0)
            {
                printf("error writing to buffer\n");
                return 1;
            }
        }
    } while (i > 0);

    ssh_disconnect(session);
    ssh_bind_free(sshbind);
    ssh_finalize();

    return 0;
}

