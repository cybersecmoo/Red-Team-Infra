import sys
import paramiko
import threading
import socket
import subprocess
import select

closeTunnel = False

def tunnel_handler(channel, host, port):
    sock = socket.socket()

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


def establish_reverse_tunnel(forwardToHostPort, forwardFromPort, transport):
    """ Sets up a reverse tunnel, such that traffic on the server's port `forwardFromPort` is forwarded via SSH to `forwardToHostPort`
    """
    transport.request_port_forward("", forwardFromPort)

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
        remoteForwardPort = 9090
        localSSHPort = 2222

        try:
            client = paramiko.SSHClient()
            client.load_system_host_keys()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy)
            client.connect(hostname, port, username=username, password=password)
            chan = client.get_transport().open_session()
            command = ""

            print("Awaiting data")

            establish_reverse_tunnel(("localhost", localSSHPort), remoteForwardPort, client.get_transport())  
        finally:
            client.close()

if __name__ == "__main__":
    main()

