# Quest Recipe Graphs

This README uses practical ASD-STE100 Simplified Technical English. It uses short instructions and consistent terms.

These Archify workflow diagrams show the ten curated recipes in [`quest-recipe-design-guide.md`](../../quest-recipe-design-guide.md).

Each recipe includes:

- One standalone HTML file.
- Dark and light themes.
- PNG, JPEG, WebP, and SVG export controls.
- One adjacent `.workflow.json` source file for editing.

| # | Recipe | Diagram | Source |
|---:|---|---|---|
| 1 | Awaken the Fortress | [Open HTML](01-awaken-the-fortress.html) | [Workflow JSON](01-awaken-the-fortress.workflow.json) |
| 2 | Broken Circuit | [Open HTML](02-broken-circuit.html) | [Workflow JSON](02-broken-circuit.workflow.json) |
| 3 | Resonant Sequence | [Open HTML](03-resonant-sequence.html) | [Workflow JSON](03-resonant-sequence.workflow.json) |
| 4 | Perimeter Wake | [Open HTML](04-perimeter-wake.html) | [Workflow JSON](04-perimeter-wake.workflow.json) |
| 5 | The Hollow Signal | [Open HTML](05-the-hollow-signal.html) | [Workflow JSON](05-the-hollow-signal.workflow.json) |
| 6 | Predator's Lens | [Open HTML](06-predators-lens.html) | [Workflow JSON](06-predators-lens.workflow.json) |
| 7 | Borrowed Charge | [Open HTML](07-borrowed-charge.html) | [Workflow JSON](07-borrowed-charge.workflow.json) |
| 8 | Arsenal Covenant | [Open HTML](08-arsenal-covenant.html) | [Workflow JSON](08-arsenal-covenant.workflow.json) |
| 9 | Blackout Protocol | [Open HTML](09-blackout-protocol.html) | [Workflow JSON](09-blackout-protocol.workflow.json) |
| 10 | The Living Lock | [Open HTML](10-the-living-lock.html) | [Workflow JSON](10-the-living-lock.workflow.json) |

## Re-render

Run these commands from the Archify installation directory:

```sh
for source in /path/to/stalberg-grid/docs/diagrams/quest-recipes/*.workflow.json; do
  output="${source%.workflow.json}.html"
  node bin/archify.mjs render workflow "$source" "$output"
  node bin/archify.mjs validate workflow "$source" --json
  node bin/archify.mjs check "$output"
done
```
