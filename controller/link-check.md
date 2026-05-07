# Example terminal output for `nestdaq-link-check`

`nestdaq-link-check` displays established connection counts and message-rate
difference for one pair of NestDAQ channels.

Example command:

```bash
nestdaq-link-check \
    --service-a Sampler \
    --channel-a data \
    --service-b Sink \
    --channel-b in \
    --redis-url-daq_service 127.0.0.1:6379/0 \
    --redis-url-metrics 127.0.0.1:6379/1 \
    --diff-low -5 \
    --diff-high 5 \
    --refresh 0 \
    --no-color
```

Example output:

```text
NestDAQ link check  Sampler:data <-> Sink:in  updated=2026-05-07 12:34:56  diff-low=-5  diff-high=5

Connection link count
Rows A: Sampler:data  Columns B: Sink:in

A\B               0[1]          1[0]
0[1]              1             0
1[2]              0             2

Sampler:data -> Sink:in message-rate diff [msg/s]
A\B               0[1]          1[0]
0[1]              4.0           n/a
1[2]              -1.0          12.5

Sink:in -> Sampler:data message-rate diff [msg/s]
A\B               0[1]          1[0]
0[1]              1.0           n/a
1[2]              0.0           -2.0

Legend: connection cells are established bind/connect address match counts. Headers are instance-index[sub-channel-count].
Traffic cells are sender msg-out rate minus receiver msg-in rate. Colors: LOW/OK/HIGH by thresholds.
```

## Display layout

- Rows are instances of `--service-a`.
- Columns are instances of `--service-b`.
- Row and column headers use `instance-index[sub-channel-count]`.
- `instance-index` is the numeric suffix after the last `-` in the instance name. If the suffix is not numeric, the full instance name is shown.
- `sub-channel-count` is the number of addressed socket sub-channels for that instance and channel.
- The connection matrix cell is the number of established links for that instance pair.
- An established link is counted when a socket with `method=bind` and a socket with `method=connect` have the same non-empty address. Comma-separated address lists are split before comparison.
- `n/a` in a traffic cell means the required metrics fields were not found.

## Traffic cells

The first traffic matrix shows:

```text
service-a:channel-a msg-out rate - service-b:channel-b msg-in rate
```

The second traffic matrix shows the reverse direction:

```text
service-b:channel-b msg-out rate - service-a:channel-a msg-in rate
```

When color output is enabled, values are classified by the thresholds:

- `diff > --diff-high`: high side
- `--diff-low <= diff <= --diff-high`: normal
- `diff < --diff-low`: low side
