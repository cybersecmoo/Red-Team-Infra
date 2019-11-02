import threading
import http


class BeaconClient(threading.Thread):
    def __init__(self, commands):
        # `commands` is a Queue of commands from the main C2 client class
        super(ClientThread, self).__init__()
        self.endpoint = "http://localhost:5000/api/heartbeat/"
        self.commands = commands

        self.stop_request = threading.Event()

    def run(self):
        while not self.stop_request.is_set():
            command = self.commands.get(True, 0.05)

            if command == "CHANGE_ENDPOINT":
                newEndpoint = self.commands.get(True, 0.05)
                self.endpoint = newEndpoint
