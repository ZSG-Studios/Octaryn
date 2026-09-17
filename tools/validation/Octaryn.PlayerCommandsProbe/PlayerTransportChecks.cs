using Octaryn.Server;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking.Remote;

internal static class PlayerTransportChecks
{
    internal static void Run(Action<HostFrameSnapshot> step)
    {
        var random = new Random(20260917);
        double now = 1;
        var queue = new PlayerCommandQueue(() => now);
        var packets = new List<(double Due, byte[] Bytes)>();
        var acknowledgements = new List<(double Due, ulong Frame)>();
        var generated = new List<PlayerCommand>();
        ulong clientAck = 0, lastAck = 0, ticks = 0;
        var consumed = new HashSet<ulong>();
        var jumpPresses = 0;
        var maxClientPending = 0;
        var maximumAckGap = 0.0;
        var lastAckTime = now;
        var dropped = 0;
        var reordered = 0;
        var maximumBytes = 0;
        const int totalCommands = 600;

        // Both directions have 30-60ms one-way jitter and independent 2% loss.
        for (var quantum = 0; quantum <= 2200; quantum++)
        {
            now = 1 + quantum * 0.005;
            var generatedCount = Math.Min(totalCommands, (int)((now - 1) * 60 + 1e-8));
            while (generated.Count < generatedCount)
            {
                var frame = (ulong)generated.Count + 1;
                generated.Add(new(frame, frame == 90 ? 5u : 4u, 1, 1, 0, 0, 0, 0, 1));
            }
            if (quantum % 4 == 0)
            {
                var pending = generated.Where(c => c.FrameIndex > clientAck).Take(64).ToArray();
                maxClientPending = Math.Max(maxClientPending, generated.Count - (int)clientAck);
                for (var offset = 0; offset < pending.Length; offset += 20)
                {
                    var bytes = PlayerCommandPacket.Encode(pending.AsSpan(offset, Math.Min(20, pending.Length - offset)));
                    maximumBytes = Math.Max(maximumBytes, bytes.Length);
                    if (random.NextDouble() < 0.02) { dropped++; continue; }
                    packets.Add((now + 0.030 + random.NextDouble() * 0.030, bytes));
                }
                if (random.NextDouble() >= 0.02)
                    acknowledgements.Add((now + 0.030 + random.NextDouble() * 0.030, queue.Acknowledged));
            }
            foreach (var packet in packets.Where(p => p.Due <= now).OrderBy(p => p.Due).ToArray())
            {
                var decoded = PlayerCommandPacket.Decode(packet.Bytes);
                if (decoded.Length > 0 && decoded[^1].FrameIndex < queue.Acknowledged) reordered++;
                queue.Accept(decoded);
                packets.Remove(packet);
            }
            queue.Accrue();
            while (queue.TrySelect(ticks + 1, out var frame))
            {
                var sequence = queue.SelectedSequence;
                if (sequence != 0)
                {
                    Require(consumed.Add(sequence), "duplicate command simulated twice");
                    if ((frame.Input.Flags & HostInputSnapshot.JumpFlag) != 0) jumpPresses++;
                }
                step(frame);
                queue.Commit(in frame);
                ticks++;
            }
            if (queue.Acknowledged != lastAck)
            {
                maximumAckGap = Math.Max(maximumAckGap, now - lastAckTime);
                lastAck = queue.Acknowledged;
                lastAckTime = now;
            }
            foreach (var ack in acknowledgements.Where(a => a.Due <= now).ToArray())
            {
                clientAck = Math.Max(clientAck, ack.Frame);
                acknowledgements.Remove(ack);
            }
        }
        Require(queue.Acknowledged == totalCommands && clientAck == totalCommands && consumed.Count == totalCommands,
            "impaired command stream lost or expired commands");
        Require(jumpPresses == 1, "one-frame jump edge was lost or duplicated");
        Require(ticks <= 660 && maximumAckGap <= 0.25 && maxClientPending < 64,
            "ack starvation, unbounded backlog, or authority speedup");
        Require(maximumBytes <= 804, "datagram exceeds fragment-free command cap");
        Console.WriteLine($"commands impaired_datagrams=pass seed=20260917 one_way_ms=30-60 loss_percent=2 consumed={consumed.Count} ack={clientAck} max_pending={maxClientPending} max_ack_gap_ms={maximumAckGap * 1000:F1} dropped={dropped} reordered={reordered} max_packet_bytes={maximumBytes} actor_steps={ticks}");

        var original = PlayerCommandPacket.Encode(generated.Take(20).ToArray());
        Require(PlayerCommandPacket.Decode(original.AsSpan(0, original.Length - 1)).Length == 0, "truncated datagram accepted");
        original[1] = 0;
        Require(PlayerCommandPacket.Decode(original).Length == 0, "wrong protocol accepted");
        Console.WriteLine("commands datagram_validation=pass");

        now = 20;
        queue = new PlayerCommandQueue(() => now);
        var shortTap = Enumerable.Range(1, 24).Select(i =>
            new PlayerCommand((ulong)i, i == 1 ? 1u : 0u, 1, 0, 0, 0, 0, 0, 1)).ToArray();
        var head = PlayerCommandPacket.Encode(shortTap.AsSpan(0, 20));
        var tail = PlayerCommandPacket.Encode(shortTap.AsSpan(20));
        queue.Accept(PlayerCommandPacket.Decode(tail)); // Head lost, tail arrives out of order.
        now += 0.06;
        queue.Accept(PlayerCommandPacket.Decode(head)); // Identical batch retransmission.
        queue.Accept(PlayerCommandPacket.Decode(tail));
        queue.Accept(PlayerCommandPacket.Decode(head)); // Duplicate must not replay the edge.
        queue.Accrue();
        jumpPresses = 0;
        ticks = 0;
        while (queue.TrySelect(ticks + 1, out var frame))
        {
            if ((frame.Input.Flags & 1) != 0) jumpPresses++;
            queue.Commit(in frame);
            ticks++;
        }
        Require(queue.Acknowledged == 3 && jumpPresses == 1,
            "unchanged fragmented batch retry lost/duplicated short tap after reordered tail");
        Console.WriteLine("commands unchanged_batch_retry=pass reordered_tail=pass short_press_count=1 ack=3");
    }

    private static void Require(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
    }
}
