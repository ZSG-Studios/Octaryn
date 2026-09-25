"""Focused queue-bound and actual loopback forwarding checks; no engine launch."""
import socket
import time
import unittest

from udp_impairment import UdpImpairment, bind_loopback


class ImpairmentTests(unittest.TestCase):
    def assert_accounted(self, stats):
        for direction in ('client_to_server', 'server_to_client'):
            counts = stats[direction]
            self.assertEqual(counts['received'], sum(value for key, value in counts.items() if key != 'received'))
        self.assertEqual(stats['remaining_packets'], 0)
        self.assertEqual(stats['remaining_bytes'], 0)

    def test_packet_and_byte_caps_with_shutdown_accounting(self):
        with bind_loopback(0) as server:
            for packet_limit, byte_limit, payloads, expected in (
                    (2, 1024, [b'x'] * 3, 1),
                    (20, 5, [b'1234', b'12', b'a', b'b'], 2)):
                proxy = UdpImpairment(server.getsockname()[1], minimum_ms=1000, maximum_ms=1000,
                                      packet_limit=packet_limit, byte_limit=byte_limit)
                try:
                    for payload in payloads:
                        proxy.enqueue(payload, proxy.server, 'client_to_server', 1)
                    self.assertLessEqual(len(proxy.queue), packet_limit)
                    self.assertLessEqual(proxy.queued_bytes, byte_limit)
                finally:
                    stats = proxy.stop()
                self.assertEqual(stats['client_to_server']['bound_dropped'], expected)
                self.assertEqual(stats['client_to_server']['shutdown_discarded'], 2)
                self.assert_accounted(stats)

    def test_seeded_loss_and_delay_bounds(self):
        with bind_loopback(0) as server:
            histories = []
            for _ in range(2):
                proxy = UdpImpairment(server.getsockname()[1], minimum_ms=30, maximum_ms=60,
                                      loss_percent=2, seed=20260917)
                try:
                    for index in range(500):
                        proxy.enqueue(index.to_bytes(4, 'little'), proxy.server, 'client_to_server', 1)
                    histories.append(sorted((due, payload) for due, _, payload, _, _ in proxy.queue))
                    self.assertTrue(all(1.030 <= due <= 1.060 for due, _ in histories[-1]))
                finally:
                    stats = proxy.stop()
                self.assertGreater(stats['client_to_server']['random_dropped'], 0)
                self.assertEqual(stats['client_to_server']['bound_dropped'], 0)
                self.assert_accounted(stats)
            self.assertEqual(*histories)

    def test_real_bidirectional_udp_and_peer_pinning(self):
        with bind_loopback(0) as server, bind_loopback(0) as client, bind_loopback(0) as stranger:
            server.settimeout(2)
            client.settimeout(2)
            proxy = UdpImpairment(server.getsockname()[1], minimum_ms=5, maximum_ms=10)
            proxy.start()
            try:
                client.sendto(b'press-release', proxy.address)
                payload, peer = server.recvfrom(128)
                self.assertEqual(payload, b'press-release')
                self.assertEqual(peer, proxy.address)
                server.sendto(b'ack-2', peer)
                self.assertEqual(client.recvfrom(128)[0], b'ack-2')
                stranger.sendto(b'cannot-steal-peer', proxy.address)
                deadline = time.monotonic() + 2
                while proxy.stats['unexpected_source'] == 0 and time.monotonic() < deadline:
                    time.sleep(.005)
                self.assertEqual(proxy.client, client.getsockname())
            finally:
                stats = proxy.stop()
            self.assertIsNone(stats['error'])
            self.assertEqual(stats['unexpected_source'], 1)
            self.assertEqual(stats['client_to_server']['forwarded'], 1)
            self.assertEqual(stats['server_to_client']['forwarded'], 1)
            self.assert_accounted(stats)

    def test_stop_discards_scheduled_real_packet(self):
        with bind_loopback(0) as server, bind_loopback(0) as client:
            proxy = UdpImpairment(server.getsockname()[1], minimum_ms=1000, maximum_ms=1000)
            proxy.start()
            try:
                client.sendto(b'queued', proxy.address)
                deadline = time.monotonic() + 2
                while proxy.stats['client_to_server']['received'] == 0 and time.monotonic() < deadline:
                    time.sleep(.005)
            finally:
                stats = proxy.stop()
            self.assertEqual(stats['client_to_server']['shutdown_discarded'], 1)
            self.assertEqual(stats['client_to_server']['forwarded'], 0)
            self.assert_accounted(stats)
            server.settimeout(.05)
            with self.assertRaises(socket.timeout):
                server.recvfrom(128)


if __name__ == '__main__':
    unittest.main()
