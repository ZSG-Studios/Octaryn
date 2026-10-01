# CPU wall-time outliers in the build10 matrix

The 72-run matrix retains the outliers in its reported worst-frame values.
This analysis joins CPU, GPU and lighting CSVs by renderer frame ID, then maps
that ID to the observed camera ready-frame index. Raw rows and three neighbors
on either side are in `logs/client/performance-build10-full/cpu-outlier-analysis.json`.

| Adaptive workload | Run/case | Renderer / ready frame | Total / render wall ms | Same-frame GPU ms | Wait / encode wall ms |
|---|---|---|---:|---:|---:|
| 1440p mode2 static | repeat0 / `8dtq33s8` | 578 / 264 | 50.603 / 50.496 | 12.828 | 32.274 / 18.042 |
| 4K mode2 motion | repeat0 / `tev6d0k9` | 614 / 271 | 64.281 / 64.096 | 16.577 | 37.224 / 0.846 |

The slow-frame text log uses completed-frame counts, so these are log frames579
and615 respectively. The matrix-wide GPU maxima13.658/32.239 ms occur on other
frames and cannot be paired with these CPU maxima as simultaneous measurements.

Simulation/event/UI times were respectively0.0368/0.0022/0.0333 ms and
0.0620/0.0043/0.0716 ms. Thus the outliers are inside the render call, not evidence
of an authority or gameplay execution stall. CPU/GPU memory remained unchanged
around both events. All shader and pipeline requests hit the warm cache, and
the nearest image captures occur well after these frames.

The1440p render interval is almost fully explained by its measured wait and
encode intervals (unassigned0.029 ms). The4K render interval has25.818 ms outside
the recorded GPU-profile CPU intervals. `WorldFrame` resolves GPU query results
and writes the previous slot's CSV before `begin_cpu`; that interval is outside
the recorded wait and subsequent markers. The render tail is also partly outside
those markers. Without the optional detailed stage trace, the historical row
cannot distinguish these locations or attribute the delay to a specific call.

Both rows report0 ms current-thread CPU advancement. Windows `GetThreadTimes`
samples in this run commonly advance in15.625 ms steps; zero is not proof of
zero execution. Both runs confirmed below-normal process priority. The next
frames took only1.115 and2.754 ms, consistent with queued GPU work completing
while the CPU was delayed. Scheduling or a driver/query wait is plausible, but
the measurements do not identify which. A diagnostic stage trace and OS CPU
scheduling/GPU queue capture would be needed to attribute a reproduction.
No rendering or host algorithm change is justified by these two rows alone.
