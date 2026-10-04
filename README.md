# Analyzer

Analyzer is a separate Git repository from `main`. Its remote, branches, and history must remain independent. The
standalone Linux build consumes headers and already-built libraries from a neighbouring `main` checkout; it does not
add Analyzer to the `main` CMake project and does not alter either repository's Git configuration.

## Prerequisites

- Qt Creator with the Desktop Qt 5.15.2 GCC 64-bit Release kit.
- A C++17 compiler for Analyzer. The neighbouring main project keeps its own compiler-standard setting.
- A configured and built `main` tree. Analyzer requires `main`'s generated `version.h`, Qwt archive, and shared
  libraries.

The current workspace's active `main` kit is Qt 5.15.2/GCC, Release, with this build directory:

```text
/home/ad/git_repo/build/Desktop_Qt_5_15_2_GCC_64bit-Release
```

## PRP3 analysis

The data-source dialog accepts RDPS3 V3 Doppler logs named `*.prp3.cbor`, either as one file or as all matching files
in a selected folder. If a folder contains PRP3 logs, that import uses only those logs; RDB files in the same folder
are not co-imported. Legacy PRP1/PRP2 inputs are unsupported.

### Recorded trajectory and H_T/H_C evidence

Schema-2 recordings from RDPS3 contain the tracker's actual committed trajectory and H_T/H_C diagnostics, including
tentative tracks that never confirmed. To convert a V1 archive, run the updated `rdps3 --fast --prp <archive>` with
`--replay-config <xml> --replay-output <new-directory> --replay-cbor`, then open `<new-directory>/prp3/` here.
Choose `--replay-period` according to the recording's nominal antenna period. The existing hypothesis setting
`inputs.prp.psr.hypothesis.enabled` must be true to record H_T/H_C; its default remains false. No Doppler samples are
invented for old V1 inputs, and no tracker decision is changed by the export.

Click a plot or track point to open **Recorded tracker evidence** (also available from its menu-bar toggle). Choose
the source-use context when a physical plot was used by several tracker tables. The panel shows exact recorded
fields, run/model configuration, availability/reason, time since the last new score, full event history and a metric
history chart. The track selector includes PSR tracks and slot zero. Lifetimes remain separate when a slot is reused.

- **Metric:** H_T/H_C cumulative log evidence, trajectory `Jcv/dof` or `Jcv`, NIS, or the target/persistent/clutter
  log evidence against the Poisson baseline. These are scenario diagnostics, not aircraft probabilities or calibrated
  acceptance thresholds. Higher H_T/H_C favours the target model; lower trajectory residual favours the CV fit.
- **Scope:** each observation filters/colours individual points and segments. For H_T/H_C, a missing value with no
  new score carries the preceding valid value forward within the same run/table/lifetime, without borrowing from a
  later observation time. Points before the first valid score and invalid or ambiguous uses remain grey. Other metrics
  are not carried. The observation chart and numeric filters use the carried value; raw fields/history stay unchanged,
  and inspection identifies a carried value with its source time and age. The new-score filter still requires an actual
  recorded update. Confirmation uses the value available
  at the first recorded confirmation for the whole lifetime; last new H_T/H_C score applies that retrospective snapshot
  to the whole lifetime. Confirmation never borrows a later score. A lifetime can confirm before any finite score.
- **Table and availability:** select a tracker table; choose any, finite, unavailable, or newly updated H_T/H_C
  evidence. An ambiguous physical plot remains grey/unavailable until its use is uniquely selected by the table filter.
  The inspection context selects details only; it does not silently choose a global filtering interpretation.
- **Range and colours:** enable numeric range filtering and/or metric colours. The minimum/maximum boxes also set
  the blue-low to red-high colour scale. Grey means unavailable, invalid or ambiguous, never numeric zero. Disabling
  metric colours restores the existing Doppler/MTI colours. Existing time, spatial, source and track filters still apply.

The importer joins exact run/input/table/lifetime identities; it never infers a score from a nearby track. Raw input
plots without an association remain inspectable but have no tracker score. Missing parts/end records produce coverage
warnings. Missing source references and lifecycle anomalies are retained and their affected samples are unavailable
for numeric filtering. Corrupt/truncated records fail the import transaction. Schema-1 files still open with their
original Doppler displays and unavailable tracker evidence.

Schema 2 records the logical source radar and map centre. Input measurement/processing clocks and artificial replay
tail are explicit. A V1-derived date is a reconstructed replay coordinate; a live input ingress timestamp is a proxy,
not proof of sensor measurement time. Large recordings retain rich histories in memory; select a complete part or
shorter replay when inspecting a bounded interval, and heed the partial-history warning.

The standalone regression project in `tests/` exercises the actual producer, reader, scorer, replay parser and panel:

```sh
cmake -S tests -B /path/to/analyzer-recording-tests -G Ninja -DCMAKE_PREFIX_PATH=/opt/Qt/5.15.2/gcc_64
cmake --build /path/to/analyzer-recording-tests
cmake -E chdir /path/to/analyzer-recording-tests ctest --output-on-failure
```

### Doppler records and signal inspection

PRP3 import validates the file and record envelopes, declared size, ISO-3309 checksum, self-described CBOR tag,
schema/version, producer, required typed fields, redundant byte-string sizes, and the typed DPS1 evidence against the
exact logged input frame. Unknown map keys remain forward-compatible. Event records are validated and counted but are
not spatial plots. Every plot retains its exact CBOR payload, input frame, output APOI, and parsed analysis fields for
the lifetime of the import.

Plots with a valid restoration result use the shared physical Doppler limit for a continuous dark-blue (maximum
negative) through neutral to light-red (maximum positive) colour scale. A grey plot has no valid restored frequency;
it is not treated as zero Doppler. Selecting a PRP3 plot opens or updates one modeless window containing a complex-plane
trajectory with equal I/Q scale and labelled endpoints, `abs(z)` for all 13 samples, and the 12 wrapped adjacent-sample
phase increments at transition indices `n+0.5`, with adjacent points connected. Zero-magnitude pairs have undefined
phase and are omitted, leaving breaks in the phase curve and a count in the summary. Samples and magnitudes remain
visible, including zeros.

For a restored result, the displayed channels follow its logged contribution mask. MH always displays channel A only,
including Diversity mode, because B contains noise. Older MH records that report B participation are explicitly flagged;
the recorded Doppler is retained, not recomputed or represented as an A-only result. Without a valid restoration,
decoded admissible samples remain available as explicitly labelled diagnostic evidence. Undecoded snapshots produce
empty charts rather than fabricated samples. These displays use the exact logged snapshot, not just its representative
CPI or a new reconstruction from plot metadata.

The **Phase diagnostics** tab analyses the selected plot and one displayed physical channel at a time. It provides:

- Measured/model phase increments and wrapped residual charts, with excluded residuals in grey and CPI boundaries at
  samples 3, 6 and 9; the original signal charts remain in the **Signal samples** tab.
- All 12 transitions with endpoint magnitudes, exact transported PRI entry/value, measured and predicted phase,
  residual and support reason. A sample must be valid, finite and at least `max(40, 0.2 × valid peak)` in magnitude;
  both endpoints must pass, and the interval must be nonzero. The gate is fixed diagnostic policy, independent of
  phase agreement and the production restorer's configured thresholds. Indices and time gaps are never compressed.
- A timing selector: `n→n+1` uses `period[n]` by default, or `period[n+1]` for the next-sample comparison. Each uses this
  channel's transported values, with no hard-coded period order. These are explicit hypotheses; neither selection
  establishes hardware timing or infers which association an older logged restoration used.
- A global wrapped pair least-squares search over **raw-phase −20000…+20000 Hz**, weighted by adjacent magnitude
  products. Positive raw Hz means increasing `arg(I+jQ)`. No polarity reversal, calibration, known test tone, MTD
  result or other plot enters the calculation. This diagnostic interval and pair-RMS objective differ from the
  operational Doppler bound and coherent-score ranking. The logged Doppler is never overwritten.
- The best eight candidate fits and every additional candidate within 1° RMS of the optimum. This allowance is an
  explicit diagnostic convention, not a confidence interval. One supported PRI is labelled underdetermined; several
  PRIs can still admit competing aliases. The optional **Manual raw Hz** model lets you inspect another candidate or
  raw-frequency hypothesis without changing the fitted ranking or holdouts.
- Within-CPI weighted circular phase dispersion for the fixed four-sample windows 0–3, 3–6, 6–9 and 9–12. Dispersion
  is shown only with at least two supported transitions sharing one PRI under the selected mapping; otherwise the
  table states insufficient support or mixed PRI. A vanishing circular resultant has no defined mean.
- Strict CPI holdouts within the selected plot: exclude all four CPI samples and every training transition touching
  them, then predict supported transitions inside that CPI without refitting. Display every training candidate within
  1° of the training optimum, with training and held-out RMS; do not choose aliases using the held-out error. Empty
  tests and insufficient training are labelled explicitly. Adjacent-pair errors remain correlated, and a disagreeing
  prediction from one alias does not by itself identify bad samples.

The branch selector follows the existing logged-contribution/MH rules above. Timing and manual-model controls persist
while inspecting subsequent plots, and the selected mapping/model is stated in every report. The separate **Compare PRP3 plots** task below adds cross-plot comparison without changing this per-plot analysis.

### Compare PRP3 plots

Choose **Tasks → Compare PRP3 plots** to open a modeless comparison of all imported PRP3 plots passing the current
filters, with an allocated visible plot item and a visible layer. Map pan/zoom and drawing decimation do not select
this cohort. Each run takes a snapshot; run the task again after changing filters or importing another file. The
window retains shared record ownership, so importing/clearing data cannot leave dangling plot references. Phase
analysis runs in short GUI timer batches; closing the window cancels pending batches.

The sortable table shows logged frequency and deviation from its group's median, restoration validity/reason,
logged residual/margin, raw-phase optimum and RMS, valid/signal sample and pair counts, supported PRI diversity,
competitive alias count, reference prediction error and nearest-reference alias offset/RMS excess. UTC, file,
sequence, raw range and azimuth identify the plot; tooltips retain full file and byte-offset provenance. Invalid
logged frequencies remain unavailable and do not enter frequency charts or statistics. Horizontal scrolling exposes
all diagnostic columns; sorting preserves plot identity and the selected reference.

- **Frequency and phase fit:** logged valid Hz versus chronological plot number, coloured by group, alongside best
  raw-phase RMS and RMS at that group's reference frequency. Selecting a table row highlights it in both charts.
- **Group statistics:** valid/total counts, median, median absolute deviation (MAD), mean, population standard
  deviation, extrema and reference identity. Groups separate file, input channel, producer build, mode/signal/bin,
  branch mask, source SIC, recorded PRI association and logged calibration profiles. Missing calibration metadata
  is not interpreted as identity calibration. A comparison group is not an identified track; filter time/area to
  isolate the target before interpreting differences. Median deviation includes the plot itself and is descriptive,
  not an outlier test or an error probability.
- **Radial speed comparison:** approved restored Doppler radial speed versus an independently calculated local
  range/time slope, in m/s, with MH and NFM distinguished and an equal-speed reference line. Selecting any table
  row shows its inferred geometry chain's local and approved-Doppler speed history. Local estimates include plots
  whose Doppler is rejected; those plots have gaps in the Doppler series rather than fabricated zero speeds.
  The table also shows chain, both speeds, signed difference, local point count, actual window duration and
  conditional slope standard error. Missing estimates remain unavailable.

  Association uses raw polar positions and recorded ingress time only, independently of Doppler, phase, amplitude,
  and the selected phase-analysis channel/timing controls. Straight-motion hypotheses use 200 m radial and
  `max(100 m, range × radians(0.35))` tangential gates, one point per scan, disjoint chains, at least eight plots,
  35 seconds, 60% scan coverage and at most 350 m/s fitted planar speed. Local ordinary least-squares range slopes
  use up to nine nearest-in-time points in that chain; they may include future points. File/input/build/beam
  partitions and scan/clock reversals are not joined. Short cohorts remain without a chain; competing spatial
  paths can still be misassociated and straight fits may miss manoeuvres or sparse paths. These are inferred chains, not tracker or aircraft identities.

  CBOR `arrival.utc_ms` is the available clock, not original PCAP or transmitter time. Replay acceleration,
  batching and unverified ingress timing can distort slopes and chain eligibility; the tab states this limitation
  explicitly. Positive local slope means increasing range. The logged radial speed is used directly without an
  additional carrier/sign correction, and rejected diagnostic frequencies never become approved speed evidence.
  Standard errors condition on the local fit and omit systematic timing/range and association uncertainty.
  Calculations run in cancellable GUI timer batches and do not require PCAP or alter recorded restoration results.
- **Selected plot and aliases:** the best eight raw aliases and all others within 1° RMS of the optimum, with offsets
  from the reference and RMS excess. All twelve transitions retain their original indices, actual PRI, measured
  phase and best/reference residuals. No phase-based sample exclusions are introduced.
- **Selected plot residuals:** supported-transition residuals for its own best raw frequency and the group's
  reference raw frequency. Reference prediction does not refit the inspected plot. The reference plot's own result
  is explicitly an in-sample fit, not an independent check.

By default, the raw reference is the plot with most supported pairs, then lowest RMS, among group members with at
least two supported PRI values and exactly one candidate within the diagnostic 1° allowance. If none qualifies,
reference comparisons stay unavailable. **Use selected plot as raw reference** overrides this choice within its group;
an ambiguous manual reference is labelled. Changing the channel or timing restarts analysis and resets references.
The 1° allowance is a diagnostic convention, not a confidence interval. The same period[n] default, fixed independent
amplitude/validity mask, ±20000 Hz raw search and MH A-only rule as the per-plot window apply.

To investigate an isolated frequency difference, sort the median-deviation column, select that plot and compare its
support/PRI counts and aliases with its neighbours. A low best RMS but competing aliases, plus a nearby reference
alias with little RMS penalty, supports an alias-selection explanation. High best RMS points to phase inconsistency
under the selected timing/model. High reference RMS with low best RMS can also reflect a real frequency change or a
different target. Raw-phase Hz and logged physical Doppler are kept distinct; no calibration, polarity correction,
MTD input or automatic Doppler correction is applied. Double-click the row or choose **Open selected plot diagnostics**
for the existing signal/CPI/holdout window. Its channel/timing controls remain independently selectable.

### MTI labels

Choose **Tasks → MTI labels** to colour PRP3 PSR plots by their per-plot motion evidence (rdps3 F-108) instead of
restored Doppler; choose it again to return to the Doppler palette. The choice persists across filter changes and
imports, and the status-bar legend follows it. The evidence is not binary, so the palette is ordinal:

| Colour | Meaning |
|---|---|
| violet `#9085e9` | stationary-like: the pulses behave like a fixed scatterer (MTI ratio `q < 0.05`) |
| cool grey `#97a9a9` | undecided: some phase change, but neither moving rule fires |
| orange `#b13e06` / `#ce511e` / `#e56c40` / `#fe7d4d` | moving, brightening with `q`: `< 0.3` (including a moving call from `m` alone), `0.3–0.6`, `0.6–1`, `>= 1` |
| dark grey `#646462` | unavailable: no decoded snapshot, or too few supported samples |

`q` is the two-pulse MTI ratio (about 0 for stationary clutter, about 1 for random phase, up to 2 for alternating
phase) and `m` is one minus the coherence at 0 Hz. The class uses the production rule: moving if `q >= 0.15` at a peak
SNR of at least 15 dB, or `m >= 0.5` unless `q < 0.05`; stationary-like if `q < 0.05` (or, without `q`, `m < 0.05`).
Higher `q` means stronger pulse-to-pulse decorrelation, not higher speed: `q` falls again towards the ~38 m/s blind
speed. The class means "not a stationary scatterer", not "aircraft": birds, weather, vehicles and turbines move, and
tangential aircraft or targets under strong same-cell clutter can look stationary. The palette was validated for
colour-vision-deficient separation and contrast on the satellite (dark) and tiles-off (light) backgrounds; blue is
deliberately unused because the Doppler palette uses it for negative frequency.

Records written by an RDPS3 build with motion evidence carry `restoration.motion`; Analyzer uses it whenever the
producer evaluated restoration. Older records, and records logged with restoration disabled, are classified at import
from the logged DPS1 snapshot by the production `DopplerRestorer::motion()` with its default settings (the header is
compiled in from `main/processor/rdps3`). Plot details show the class, `q`, `m`, peak SNR, supported samples and
whether the values were logged or recomputed. A recomputation can differ from the producer only where the site
configured non-default Doppler support or motion thresholds.

Schema version 1 does not record the configured logical radar identity, so Analyzer uses display-only radar ID 1.
Choose the appropriate radar-site centre in the source dialog; this limitation affects identity/provenance, not the
logged polar position or signal evidence.

## Build

Open `CMakeLists.txt` in Qt Creator, select the Desktop Qt 5.15.2 GCC 64-bit Release kit, and build `analyser`. The
project uses the neighbouring `main` checkout and its existing Release build. The old `.pro` files are retained only
as historical records; CMake is the supported build authority.
