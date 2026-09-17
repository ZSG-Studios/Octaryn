"""Bounded, seeded UDP impairment for one isolated qualification peer pair."""
import heapq
import random
import select
import socket
import threading
import time


def bind_loopback(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        sock.bind(('127.0.0.1', port))
        return sock
    except BaseException:
        sock.close()
        raise


class UdpImpairment:
    MAX_PACKETS = 2048
    MAX_BYTES = 8 * 1024 * 1024

    def __init__(self, server_port, listen_port=0, minimum_ms=0, maximum_ms=0,
                 loss_percent=0, seed=20260917, packet_limit=MAX_PACKETS,
                 byte_limit=MAX_BYTES):
        if not 0 <= minimum_ms <= maximum_ms <= 2000:
            raise ValueError('delay must satisfy 0 <= min <= max <= 2000ms')
        if not 0 <= loss_percent <= 25 or not 1 <= server_port <= 65535:
            raise ValueError('invalid loss percentage or server port')
        if not 1 <= packet_limit <= self.MAX_PACKETS or not 1 <= byte_limit <= self.MAX_BYTES:
            raise ValueError('invalid queue limits')
        self.server = ('127.0.0.1', server_port)
        self.client = None
        self.minimum, self.maximum = minimum_ms / 1000, maximum_ms / 1000
        self.loss = loss_percent / 100
        self.random = random.Random(seed)
        self.packet_limit, self.byte_limit = packet_limit, byte_limit
        self.socket = bind_loopback(listen_port)
        self.address = self.socket.getsockname()
        self.socket.setblocking(False)
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self.run, name='qualification-udp-proxy', daemon=True)
        self.started = False
        self.queue = []
        self.queued_bytes = 0
        self.sequence = 0
        self.error = None
        self.stats = {
            'seed': seed, 'minimum_one_way_ms': minimum_ms,
            'maximum_one_way_ms': maximum_ms, 'loss_percent': loss_percent,
            'listen': self.address, 'server': self.server,
            'packet_limit': packet_limit, 'byte_limit': byte_limit,
            'peak_packets': 0, 'peak_bytes': 0, 'unexpected_source': 0,
            'udp_connection_resets': 0, 'maximum_schedule_lateness_ms': 0,
            'client_to_server': self.direction_stats(),
            'server_to_client': self.direction_stats(),
        }

    @staticmethod
    def direction_stats():
        return dict(received=0, forwarded=0, random_dropped=0, bound_dropped=0,
                    send_errors=0, shutdown_discarded=0, no_destination=0)

    def start(self):
        self.thread.start()
        self.started = True

    def stop(self):
        self.stop_event.set()
        if self.started:
            self.thread.join(timeout=2)
            if self.thread.is_alive():
                self.socket.close()
                raise RuntimeError('UDP qualification proxy failed to stop')
        else:
            self.discard_pending()
        self.socket.close()
        self.stats['error'] = str(self.error) if self.error else None
        self.stats['remaining_packets'] = len(self.queue)
        self.stats['remaining_bytes'] = self.queued_bytes
        return self.stats

    def enqueue(self, payload, target, direction, now):
        counts = self.stats[direction]
        counts['received'] += 1
        if target is None:
            counts['no_destination'] += 1
            return
        if self.random.random() < self.loss:
            counts['random_dropped'] += 1
            return
        if len(self.queue) >= self.packet_limit or self.queued_bytes + len(payload) > self.byte_limit:
            counts['bound_dropped'] += 1
            return
        due = now + self.random.uniform(self.minimum, self.maximum)
        self.sequence += 1
        heapq.heappush(self.queue, (due, self.sequence, payload, target, direction))
        self.queued_bytes += len(payload)
        self.stats['peak_packets'] = max(self.stats['peak_packets'], len(self.queue))
        self.stats['peak_bytes'] = max(self.stats['peak_bytes'], self.queued_bytes)

    def receive(self):
        for _ in range(64):
            try:
                payload, source = self.socket.recvfrom(65535)
            except BlockingIOError:
                return
            except ConnectionResetError:
                self.stats['udp_connection_resets'] += 1
                return
            if source == self.server:
                target, direction = self.client, 'server_to_client'
            elif source[0] == '127.0.0.1' and (self.client is None or source == self.client):
                self.client = source
                target, direction = self.server, 'client_to_server'
            else:
                self.stats['unexpected_source'] += 1
                continue
            self.enqueue(payload, target, direction, time.monotonic())

    def discard_pending(self):
        for _, _, _, _, direction in self.queue:
            self.stats[direction]['shutdown_discarded'] += 1
        self.queue.clear()
        self.queued_bytes = 0

    def run(self):
        try:
            while not self.stop_event.is_set():
                for _ in range(128):
                    now = time.monotonic()
                    if not self.queue or self.queue[0][0] > now:
                        break
                    due, _, payload, target, direction = heapq.heappop(self.queue)
                    self.queued_bytes -= len(payload)
                    self.stats['maximum_schedule_lateness_ms'] = max(
                        self.stats['maximum_schedule_lateness_ms'], (now - due) * 1000)
                    try:
                        self.socket.sendto(payload, target)
                        self.stats[direction]['forwarded'] += 1
                    except OSError:
                        self.stats[direction]['send_errors'] += 1
                wait = min(.05, max(0, self.queue[0][0] - time.monotonic())) if self.queue else .05
                ready, _, _ = select.select([self.socket], [], [], wait)
                if ready:
                    self.receive()
        except Exception as error:
            self.error = error
        finally:
            self.discard_pending()
