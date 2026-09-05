# ghinfo activity widget for Kustom

This directory contains a Kustom/KWGT widget and text templates for displaying
the three highest-priority items returned by `ghinfo`:

```text
GET http://<ghinfo-host>:8080/v1/activity?limit=3
```

The address must be reachable from the Android device running Kustom. Do not
put the GitHub token in Kustom; authentication remains owned by `ghinfo`.

## Ready-to-import preset

The ready-to-import widget preset is available at
[`../assets/ghinfo-kustom-widget.kwgt`](../assets/ghinfo-kustom-widget.kwgt).
Import this file in Kustom to load the layout, text formulas, Catppuccin
colors, font, and activity flows. After importing, update the WebGet URLs if
your `ghinfo` server uses a different address. The preset contains no GitHub
credential.

## Kustom setup

1. Create a Text global named `ghinfo`.
2. Create a Flow with a WebGet action for the endpoint above.
3. Store the response in `ghinfo`. Keep “store file content, not path”
   enabled so the global contains the JSON response.
4. Trigger the Flow on load and periodically according to the desired refresh
   interval.
5. For a single Text module, paste the formula from
   [`example.txt`](example.txt). For the Catppuccin Mocha version, use
   [`catpuccin_example.txt`](catpuccin_example.txt) instead.

## Catppuccin Mocha palette

Set the title and detail text to `#CDD6F4`, repository text to `#A6ADC8`, and
borders/dividers to `#45475A`. For dynamic card colors, use this formula in
the Text Color property:

```text
$if(gv(ghinfo)="","#6C7086",if(tc(json,gv(ghinfo),".activity.items[0].kind")="failed_run","#F38BA8",if(tc(json,gv(ghinfo),".activity.items[0].kind")="failed_job","#F38BA8",if(tc(json,gv(ghinfo),".activity.items[0].kind")="running_job","#A6E3A1",if(tc(json,gv(ghinfo),".activity.items[0].kind")="pull_request","#CBA6F7","#F9E2AF")))))$
```

Replace `[0]` with `[1]` and `[2]` for the other cards. The
[`catpuccin_example.txt`](catpuccin_example.txt) variant uses Kustom's inline
`[c=#...]...[/c]` text-color tags, so it keeps the three-card layout and the
per-item colors in a single Text module.

The formulas parse the raw JSON stored in `ghinfo` with Kustom's
`tc(json, ...)` converter. If WebGet exposes a file path instead of its
contents, change that action to store the response body before parsing.

The API returns items in priority and recency order. The preset includes the
font needed for the widget icons.

Pull-request cards show `open pull request` or `closed pull request` according
to the state returned by `ghinfo`.

## Empty-global troubleshooting

`ghinfo` is empty until the Flow runs successfully for the first
time. Do not call `wg(...)` against an empty global. Use this temporary Text
formula while testing:

```text
$if(gv(ghinfo)="","Aguardando atualização...",tc(json,gv(ghinfo),".activity.items[0].repository"))$
```

Run the Flow manually from its test/play control. If the global remains empty,
verify that the WebGet URL is reachable from the Android device and that the
next action is `Set Global` targeting `ghinfo`. If the action exposes
a value field, use the previous action's `#last` result.

## Slow responses and failed polls

`poll failed (transport)` means a server-to-GitHub connection failed (for
example DNS, connection, TLS, or timeout). `poll failed (http)` means GitHub
returned an unsuccessful HTTP status. Neither log alone proves that the
phone-to-server request is slow. Current logs include `curl_code` and a fixed
libcurl reason, or `http_status`, plus the consecutive failure count and retry
delay. Do not share tokens, environment contents, or raw authenticated traces.

During failures, a running service continues serving the last complete
snapshot with `stale: true`. If the service has restarted and no complete poll
has succeeded, data endpoints return `503 snapshot_unavailable`. Restarting
only the phone does not clear the server snapshot.

Check these URLs using the same host as the widget, including from the phone:

- `/healthz`: process reachability, independent of GitHub.
- `/v1/meta`: `snapshotAvailable`, `generatedAt`, and `poll` failure/retry state.
- `/v1/activity?limit=3`: the widget response and its `stale` flag.

From a terminal on the same network, measure the actual request:

```bash
curl --max-time 5 -sS -o /dev/null \
  -w 'HTTP %{http_code}; connect %{time_connect}s; total %{time_total}s\n' \
  'http://<ghinfo-host>:8080/v1/activity?limit=3'
```

A quick stale `200` indicates failed refreshes, while a connection timeout
requires checking the phone's Wi-Fi/VPN, host reachability, and port exposure.
If the phone browser responds promptly but the widget does not, inspect the
Flow execution and Android background/battery restrictions. Transport retry
waits now reach at most 60 seconds; HTTP failures retain the longer backoff.
New logs and retry behavior require rebuilding and restarting the deployment;
the first complete poll must finish again after restart.
