# ghinfo activity widget for Kustom

This directory contains a text recipe for a consumer-side Kustom/KWGT widget.
It is intentionally separate from the `ghinfo` daemon: it does not add
consumer-specific fields or endpoints to the service.

The widget shows the five highest-priority items from:

```text
GET http://<ghinfo-host>:8080/v1/activity?limit=5
```

The address must be reachable from the Android device running Kustom. Do not
put the GitHub token in Kustom; authentication remains owned by `ghinfo`.

## Kustom setup

1. Create a Text global named `ghinfo_activity`.
2. Create a Flow with a WebGet action for the endpoint above.
3. Store the response in `ghinfo_activity` and enable “store file content,
   not path” if that option is shown.
4. Trigger the Flow on load and periodically according to the desired refresh
   interval.
5. Add a title Text module and five Text modules using the formulas in
   [`activity-widget.txt`](activity-widget.txt). Each formula is already
   indexed from `0` through `4`.

The JSON parsing uses Kustom's `wg`/JSON workflow and `tc(json, ...)` syntax.
See the [Kustom Web Get reference](https://docs.kustom.rocks/tags/wg/) and
[JSON text-converter reference](https://docs.kustom.rocks/categories/Reference/page/4/)
for the application-side details.

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
