import sys
import paramiko
import threading
import socket

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
    transport.request_port_forward("localhost", forwardFromPort)
    closeTunnel = False

    while closeTunnel is not True:
        channel = transport.accept(1000)

        if channel is None:
            continue
        
        thread = threading.Thread(target=tunnel_handler, args=(channel, forwardToHostPort[0], forwardToHostPort[1]))
        thread.setDaemon(True)
        thread.start()

def main():
    if len(sys.argv) < 5:
        print("Too few arguments!")

    else:
        hostname = sys.argv[1]
        username = sys.argv[2]
        password = sys.argv[3]
        port = sys.argv[4]
        localSSHDPort = 22

        try:
            client = paramiko.SSHClient()
            client.load_system_host_keys()
            client.set_missing_host_key_policy(paramiko.WarningPolicy)
            client.connect(hostname, port, username=username, password=password)
            establish_reverse_tunnel(("localhost", localSSHDPort), port, client.get_transport())
        
        finally:
            client.close()

if __name__ == "__main__":
    main()

