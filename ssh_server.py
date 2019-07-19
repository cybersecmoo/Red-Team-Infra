import socket
import sys
import paramiko
import threading
import traceback
import subprocess
import pty

QUIT_CMD = "quit"

class Server(paramiko.ServerInterface):
    def __init__(self):
        self.event = threading.Event()

    def check_channel_request(self, kind, chanid): 
        if kind == "session":
            return paramiko.OPEN_SUCCEEDED

        return paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

    def check_auth_password(self, username, password):
        if (username == "user") and (password == "foo"):
            return paramiko.AUTH_SUCCESSFUL

        return paramiko.AUTH_FAILED

    def check_auth_publickey(self, username, key):
        key_for_user = paramiko.RSAKey(filename="{0}_key".format(username))
        
        if (username == "user") and (key == key_for_user):
            return paramiko.AUTH_SUCCESSFUL
        
        return paramiko.AUTH_FAILED

    def check_auth_gssapi_with_mic(self, username, gss_authenticated=paramiko.AUTH_FAILED, cc_file=None):
        return paramiko.AUTH_FAILED

    def check_auth_gssapi_keyex(self, username, gss_authenticated=paramiko.AUTH_FAILED, cc_file=None):        
        return paramiko.AUTH_FAILED

    def enable_auth_gssapi(self):
        return False
    
    def get_allowed_auths(self, username):
        return "password,publickey"
    
    def check_channel_shell_request(self, channel):
        self.event.set()
        print("Shell request")
        return True
    
    def check_channel_pty_request(self, channel, term, width, height, pixelwidth, pixelheight, modes):
        print("PTY request")
        return True

    def check_channel_forward_agent_request(self, channel):
        return True

    def check_channel_direct_tcpip_request(self, channelID, origin, dest):
        """ TODO More robust checking; the final system will set up a list of requested reverse tunnels, and check if this matches any of the ones we wanted
        """
        print("Forwarding request received")
        response = paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

        if origin[0] == "localhost" or origin[0] == "127.0.0.1":
            print("TCP/IP Forwarding request accepted")
            response = paramiko.OPEN_SUCCEEDED

        return response

def setup_sock():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", 2222))

        return sock

    except Exception as e:
        print("*** Failed to open socket ***")
        sys.exit(1)

def establish_channel():
    trans = paramiko.Transport(client)
    
    host_key = paramiko.RSAKey(filename="host_key")
    trans.add_server_key(host_key)
    server = Server()
    trans.start_server(server=server)
    print("Server started!")

    chan = trans.accept(60)

    return chan, trans, server

def run_loop(chan):
    command = input("Command: ").strip("\n")

    if command != QUIT_CMD:
        chan.send(command)
        response = chan.recv(1024)
        text = response.decode("utf-8")
        print(text + "\n")

    return command

sock = setup_sock()

run = True

while run:
    try:
        sock.listen(100)
        print("Listening...")
        client, addr = sock.accept()
        print("Connected!")

    except Exception as e:
        print("*** Failed to connect ***")
    
    try:
        chan, trans, server = establish_channel()

        if chan is None:
            print("*** No Channel ***")

        else:
            print("Authenticated!")

            server.event.wait(10)

            if not server.event.is_set():
                print("*** Client did not ask for a shell! ***")
            
            command = ""

            while command != QUIT_CMD:
                print("Receiving commands")
                command = run_loop(chan)

            chan.close()
    
        trans.close()
    except KeyboardInterrupt as inter:
        print("Closing...")

    except paramiko.SSHException as e:
        print("*** SSH negotiation failure ***")
        traceback.print_exc()
