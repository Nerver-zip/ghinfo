# ghinfo activity widget for Kustom

This directory contains a text recipe for a consumer-side Kustom/KWGT widget.
It is intentionally separate from the `ghinfo` daemon: it does not add
consumer-specific fields or endpoints to the service.

The widget shows the three highest-priority items from:

```text
GET http://<ghinfo-host>:8080/v1/activity?limit=3
```

The address must be reachable from the Android device running Kustom. Do not
put the GitHub token in Kustom; authentication remains owned by `ghinfo`.

## Kustom setup

1. Create a Text global named `ghinfo`.
2. Create a Flow with a WebGet action for the endpoint above.
3. Store the response in `ghinfo`. Keep “store file content, not path”
   enabled so the global contains the JSON response.
4. Trigger the Flow on load and periodically according to the desired refresh
   interval.
5. Add a title Text module and the three card rows using the formulas in
   [`activity-widget.txt`](activity-widget.txt). The rows are indexed from
   `0` through `2`.
   For a single Text module, use the paste-ready
   [`example.txt`](example.txt) instead.
   For the same three-card layout with inline Catppuccin Mocha colors, paste
   [`catpuccin_example.txt`](catpuccin_example.txt) directly into the Text
   field.

## Catppuccin Mocha palette

Set the title and detail text to `#CDD6F4`, repository text to `#A6ADC8`, and
borders/dividers to `#45475A`. The card header and status can use the dynamic
color formula in `activity-widget.txt`:

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

## Visual mapping

```text
  pull_request
  issue
  running_job
  failed_run or failed_job
```

The service already orders the items by priority, recency, and deterministic
tie-breakers. The recipe only renders the returned order; it does not mark
anything as read or maintain consumer state.

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
