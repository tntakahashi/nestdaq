# Example terminal output for `nestdaq-link-check`

`nestdaq-link-check` displays established connection counts and communicated
message rates for one pair of NestDAQ channels.

Example command:

```bash
nestdaq-link-check \
    --service-a Sampler \
    --channel-a data \
    --service-b Sink \
    --channel-b in \
    --redis-url-daq_service 127.0.0.1:6379/0 \
    --redis-url-metrics 127.0.0.1:6379/1 \
    --rate-format auto \
    --refresh 0 \
    --no-color
```

Example output:

```text
NestDAQ link check  Sampler:data <-> Sink:in  updated=2026-05-07 12:34:56

Connection link count
Rows A: Sampler:data  Columns B: Sink:in

A\B               0[1]          1[0]
0[1]              1             0
1[2]              0             2

Sampler:data -> Sink:in message rate [msg/s]
src\dst           0[1]          1[0]          Total
0[1]              1.2K          n/a           1.2K
1[2]              n/a           12.5          12.5
Total             1.2K          12.5          1.2K

Legend: connection cells are established bind/connect address match counts. Headers are instance-index[sub-channel-count].
Traffic cells are min(sender msg-out rate, receiver msg-in rate) for established links. Totals sum numeric traffic cells.
```

## Display layout

- Rows are instances of `--service-a`.
- Columns are instances of `--service-b`.
- Row and column headers use `instance-index[sub-channel-count]`.
- `instance-index` is the numeric suffix after the last `-` in the instance name. If the suffix is not numeric, the full instance name is shown.
- `sub-channel-count` is the number of addressed socket sub-channels for that instance and channel.
- The connection matrix cell is the number of established links for that instance pair.
- An established link is counted when a socket with `method=bind` and a socket with `method=connect` have the same non-empty address. Comma-separated address lists are split before comparison.
- `n/a` in a traffic cell means the pair is not connected or the required metrics fields were not found.

## Traffic cells

The first traffic matrix shows:

```text
min(service-a:channel-a msg-out rate, service-b:channel-b msg-in rate)
```

The rightmost `Total` column is the sum of the numeric cells for that sender
instance. The bottom `Total` row is the sum of the numeric cells for that
receiver instance.

By default, only the `service-a -> service-b` traffic matrix is shown. Add
`--show-reverse` to show the reverse direction with the same layout:

```text
min(service-b:channel-b msg-out rate, service-a:channel-a msg-in rate)
```

Traffic cells are not colorized. `--diff-low` and `--diff-high` are accepted by
the command for compatibility, but they are not used by the traffic-rate table.

## Rate formatting

`--rate-format` controls traffic cell and total formatting:

- `auto`: default; values with absolute value 1000 or larger use SI prefixes.
- `si`: same as `auto`.
- `plain`: fixed numeric `msg/s`, for example `1200.0`.

SI-prefixed values use base 1000 and fixed one decimal digit:

```text
1200 msg/s = 1.2K
12500000 msg/s = 12.5M
```
