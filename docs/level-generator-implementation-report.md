# Level Generator Recommendation Implementation Report

## Purpose

This report records each implementation experiment, its tests, and its measured effect. The work follows `level-generator-evaluation.md`.

## Evaluation labels

- **Positive:** The change improves safety, quality, repeatability, or observability without a larger measured cost.
- **Neutral:** The change adds required infrastructure, but does not directly change accepted level quality.
- **Negative:** The change reduces measured quality or causes an unacceptable cost or regression.
- **Reverted:** The change was negative and is not in the final implementation.

## Baseline

Commit: `4b48c2c` (`Checkpoint level generation planning work`)

Baseline commands:

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure
timeout 5s xvfb-run -a ./build/stalberg_game
```

Baseline results:

- Build: passed.
- Tests: 7 of 7 passed.
- Test time: 22.30 seconds.
- Virtual-display smoke test: the game initialized with Mesa llvmpipe and ran until the five-second timeout.
- Match admission returned only a Boolean result.
- Whole-map retries could change the large-map archetype.
- Match generation accepted the first valid candidate.
- Gate presence was checked, but the locked progression sequence was not checked.

## Change record

### 1. Structured match-admission reports

Changes:

- Added a version number to each physical constraint profile.
- Added stable failure codes and names.
- Added expected and actual values to each failure.
- Added the full metric set to the validation report.
- Kept the old Boolean admission function as a report wrapper.

Tests:

- Added checks for named failures, measured ranges, profile versions, and empty reports for accepted maps.
- Build passed.
- Tests: 7 of 7 passed in 22.63 seconds.

Evaluation: **Positive**.

The accepted map set did not change. Rejections are now machine-readable and explain which limits failed. This is required for safe tuning and seed audits. The measured test-time change was small and within normal run variation.
