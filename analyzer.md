# Analyzer: Independent-Scan Scenario Base-Point-to-Plot Association Recommendations

Date: 23.07.2026  
Revision: tracker-free plot generation and strictly independent scans clarified  
Repository: [`sky349/analyzer`](https://github.com/sky349/analyzer)

## Executive recommendation

Replace the current maximum-cardinality-only matcher in `matchScan()` with a
**global minimum-cost maximum matching**, formulated as a rectangular
two-dimensional linear assignment problem. This is often called Global
Nearest Neighbour (GNN) in tracking literature, but no tracker is involved in
this use case.

The safest migration is:

1. Keep the current 2 NM range gate, 2° azimuth gate and geometric cost.
2. Preserve the current primary objective: maximise the number of gated
   associations.
3. Add the missing secondary objective: among all maximum-cardinality
   solutions, minimise the total association cost.
4. Split the gated bipartite graph into independent connected components and
   solve each component separately.
5. Use a tested rectangular Jonker–Volgenant/shortest-augmenting-path or
   Hungarian solver, with one private missed-detection dummy column per base
   point.
6. Execute this entire operation independently for every North-to-North scan;
   do not propagate association state, ownership, misses or identity between
   scans.

JPDA, MHT and multidimensional/multi-scan assignment are outside the problem
definition. Murty's k-best assignment algorithm may be useful later as a
lightweight **within-scan** ambiguity diagnostic.

## Clarified problem model

The measured plots are produced without any tracker involvement, and the
analyser does not need a tracker to measure Pd.

For scan \(s\), define:

- \(B_s\): deterministic scenario-derived base points whose beam crossings
  occur during scan \(s\);
- \(Z_s\): measured plots produced during scan \(s\);
- \(A_s=f(B_s,Z_s)\): the one-to-one association result for that scan.

The required independence property is:

\[
A_s \text{ depends only on } B_s \text{ and } Z_s.
\]

In particular:

- an association or miss in scan \(s-1\) must not change scan \(s\);
- a plot in scan \(s+1\) cannot rescue a miss in scan \(s\);
- no track state, covariance, identity history, persistence score or
  association hypothesis is propagated;
- every active scenario target contributes a fresh, independent detection
  opportunity in each scan.

The scenario trajectory is used only to calculate where each reference/base
point should be when the antenna beam crosses it. The word “track” in
`base_tracks.txt` describes the source trajectory, not a live tracker track.

Automatic scenario-epoch detection is a separate alignment step. It may sum
scores from several independently solved scans to identify the common scenario
time origin, but it must not couple the assignments themselves. For any trial
epoch, every constituent scan is still solved independently.

## Repository findings

The relevant recent commits are:

- [`427c74b3`](https://github.com/sky349/analyzer/commit/427c74b3a2f31a9fb16c4592f843490b075b13a5)
  — adds the original A/C-resolution task, based on counting visible non-Mode-S
  SSR plots by Mode A.
- [`f422b46f`](https://github.com/sky349/analyzer/commit/f422b46f6939e298aa988e12d56e031fed24e8be)
  — adds scenario-file parsing, scenario epoch detection, per-scan
  base-point-to-plot association, graphics overlay and result statistics.
- [`806a9c77`](https://github.com/sky349/analyzer/commit/806a9c7705a5b5bea3c029f0f7bf70a7f1a90d3c)
  — corrects Mode-A conversion/display and adds Correct-A Pd. It does not
  change the association algorithm.

The current association code is in
[`scenarioresolutiontask.cpp`, `findAugmentingMatch()` and `matchScan()`](https://github.com/sky349/analyzer/blob/806a9c7705a5b5bea3c029f0f7bf70a7f1a90d3c/scenarioresolutiontask.cpp#L496-L588).

The main execution and statistics pass is
[`ScenarioResolutionTask::execute()`](https://github.com/sky349/analyzer/blob/806a9c7705a5b5bea3c029f0f7bf70a7f1a90d3c/scenarioresolutiontask.cpp#L1252-L1416).

The supplied
[`base_tracks.txt`](https://github.com/sky349/analyzer/blob/f422b46f6939e298aa988e12d56e031fed24e8be/base_tracks.txt)
contains 800 scenario targets: 400 FIX and 400 VAR targets. The close FIX/VAR
pairs deliberately create overlapping association gates.

## Current processing pipeline

The following operation is repeated from a clean state for every
North-to-North antenna scan:

1. `scenarioOpportunitiesForScan()` generates at most one expected base point
   for every active scenario target. It iteratively computes the target's beam
   crossing time and corresponding expected Cartesian and polar positions.
2. Eligible radar plots are radar 50 SSR plots whose SSR type is not Mode S.
3. `groupPlotsByNorthScan()` puts measured plots into half-open
   North-to-North intervals.
4. `matchScan()` evaluates every base-point/plot pair in that scan.
5. A candidate edge is created when:

   \[
   |\Delta r| \le 2\ \mathrm{NM}
   \]

   and

   \[
   |\Delta\alpha| \le 2^\circ.
   \]

6. Its cost is:

   \[
   c_{ij} =
   \left(\frac{\Delta r}{2\ \mathrm{NM}}\right)^2 +
   \left(\frac{\Delta\alpha}{2^\circ}\right)^2.
   \]

   Consequently, every valid candidate has \(0 \le c_{ij} \le 2\).
7. Candidates for each base point are sorted by cost.
8. Base points are ordered by increasing candidate count, then by their lowest
   candidate cost.
9. `findAugmentingMatch()` recursively searches for an augmenting path.

Nothing from step 4–9 is retained for the next scan. The only shared inputs are
the fixed scenario definition and the already determined scenario epoch.

## What the current matcher already gets right

The current implementation is **not** a naïve first-come nearest-neighbour
algorithm.

If a plot is already owned by a previously processed base point,
`findAugmentingMatch()` recursively tries to move the previous owner to another
plot. Thus an earlier base point does not permanently reserve a plot.

Processing every base point with a fresh set of visited plot nodes is the
standard Kuhn augmenting-path algorithm for maximum-cardinality bipartite
matching. It therefore finds the largest possible number of one-to-one
associations inside the hard validation gates.

This means the missing capability is not one-to-one enforcement or
reassignment. The missing capability is **global cost optimisation**.

## Principal weakness: cost is not an optimisation objective

The candidate costs currently affect only traversal order. The resulting
matching maximises cardinality but is not guaranteed to minimise total cost
among the maximum-cardinality matchings.

For example:

| | Plot 0 | Plot 1 |
|---|---:|---:|
| Base A | 0.01 | 1.90 |
| Base B | 0.02 | 0.03 |

The current ordering can produce:

- A → Plot 0;
- B subsequently tries Plot 0;
- A is displaced to Plot 1;
- B takes Plot 0;
- total cost = \(1.90 + 0.02 = 1.92\).

The global optimum is:

- A → Plot 0;
- B → Plot 1;
- total cost = \(0.01 + 0.03 = 0.04\).

Both solutions contain two associations. A maximum-cardinality-only algorithm
cannot distinguish them.

This can distort:

- which target is counted as detected;
- per-target Pd;
- Correct-A Pd;
- association lines shown in the overlay;
- automatic scenario-epoch scoring, which uses the same matcher.

## When global optimisation is required

The decisive condition is not whether there are more plots than base points.
It is whether candidate gates overlap.

- If each base point has a disjoint set of candidate plots, every component can
  be solved independently and the result is trivial.
- If two or more base points compete for a plot, their decisions must be made
  jointly.
- This is true even when the number of base points equals the number of plots.
- Surplus measured plots simply remain unassigned.

## Recommended mathematical formulation

Let:

- \(x_{ij}=1\) when base point \(i\) is associated with plot \(j\);
- \(m_i=1\) when base point \(i\) is treated as a missed detection;
- \(E\) be the set of gated base/plot candidate edges.

Minimise:

\[
\sum_{(i,j)\in E} c_{ij}x_{ij}
+ \sum_i c_i^{\mathrm{miss}}m_i
\]

subject to:

\[
\sum_{j:(i,j)\in E}x_{ij} + m_i = 1
\qquad\forall i,
\]

\[
\sum_{i:(i,j)\in E}x_{ij} \le 1
\qquad\forall j,
\]

\[
x_{ij},m_i\in\{0,1\}.
\]

Every base point is therefore assigned either to exactly one measured plot or
to its missed-detection event. Every measured plot is associated with at most
one base point. Unused plot columns are the unassigned measurements.

### Rectangular cost matrix

For \(N\) base points and \(M\) measured plots, create an
\(N\times(M+N)\) matrix:

- columns `0 .. M-1`: real measured plots;
- column `M+i`: private missed-detection dummy for base point `i`;
- all other base/dummy combinations: forbidden.

The formulation handles every count regime:

- \(M>N\): some real plot columns remain unused;
- \(M<N\): some rows select private missed-detection columns;
- \(M=N\): dummies are still necessary, because one missed detection and one
  false or unrelated plot may occur simultaneously.

## Phase 1: minimum-cost maximum matching

The first implementation should preserve the current semantics:

1. maximise the number of real gated associations;
2. among solutions with that cardinality, minimise the sum of geometric costs.

This can be represented lexicographically as:

\[
\operatorname{minimise}
\left(
-\sum_{ij}x_{ij},
\sum_{ij}c_{ij}x_{ij}
\right).
\]

It may also be encoded in one assignment matrix using a missed-detection
penalty \(P\). For a connected component containing at most \(K\) real
associations and current edge costs bounded by \(C_{\max}=2\), choose:

\[
P > K C_{\max}=2K.
\]

Losing one association then costs more than any possible improvement in the
total real-edge cost.

For predictable comparisons and safe forbidden-edge handling, use quantised
`qint64` costs rather than floating-point infinity or arbitrary huge `double`
values.

Expected migration behaviour:

- scan-level and overall associated counts remain equal to the current
  maximum-cardinality matcher;
- total geometric association cost is never greater;
- per-target and Correct-A statistics may change because a better global
  assignment is selected;
- the result should become independent of base-point input order whenever the
  optimum is unique.

## Statistical gating without a tracker

The current maximum-cardinality definition assumes that every plot inside the
hard gate is preferable to a missed detection. Consequently, a gated false
alarm can artificially raise measured Pd. This issue does not require a
tracker, but it does require an association acceptance rule fixed independently
of the Pd result.

Let \(b_i\) be the deterministic scenario reference position for base point
\(i\), and \(z_j\) the measured plot:

\[
\nu_{ij}=z_j-b_i.
\]

If a residual covariance is available, it should describe only uncertainty in
the reference/measurement comparison:

\[
S_i =
R_{\mathrm{measurement},i}
+R_{\mathrm{scenario},i}
+R_{\mathrm{timing},i}.
\]

There is no tracker prediction covariance. If the scenario geometry and beam
crossing time are effectively exact, then \(S_i\) is approximately the radar
plot measurement covariance. Here \(R_{\mathrm{timing},i}\) means timestamp or
epoch uncertainty propagated into the range/azimuth measurement coordinates,
not a standalone time-domain variance.

A statistically interpretable gate is:

\[
d^2_{ij}=\nu_{ij}^{T}S_i^{-1}\nu_{ij}
\le\chi^2_q(P_G),
\]

where \(q\) is the measurement dimension and \(P_G\) is a gate probability
chosen before calculating Pd. Within that gate, \(d^2_{ij}\) can replace the
current normalised range/azimuth cost.

For the present task, the recommended policy remains:

1. establish the gate from radar accuracy, isolated-target measurements or
   another calibration dataset;
2. freeze the gate and cost definition;
3. independently solve minimum-cost maximum matching in every scan;
4. calculate Pd only after all scan associations are complete.

### Why Pd should not be an association input

A conventional tracking likelihood can include assumed detection probability
\(P_D\), missed-detection probability and clutter density. Here \(P_D\) is the
quantity being measured. Feeding a \(P_D\) estimated from the same evaluation
run back into its association cost would be circular and could make the
reported result depend on the initial assumption.

Therefore a \(P_D\)-dependent maximum-likelihood mode is **not recommended for
the current analyser calculation**. It would be defensible only if all of the
following were true:

- \(P_D\) and clutter density came from an independent external calibration;
- the association policy was frozen before evaluating the scenario;
- the resulting Pd were reported together with that policy and its parameters.

False-match bias can instead be assessed without a tracker by reporting
unassigned plots and ambiguity, measuring background/clutter density in an
independent recording, or running a negative-control/Monte-Carlo calculation
of accidental gated matches.

## Avoid circular association features

Mode A and Mode C must not be used as association costs or validation gates in
this analyser task, because A/C resolution is the output being measured. Using
them would favour plots already containing the expected response and
artificially increase the reported result.

The same caution applies to any identity field whose correctness is part of
the evaluation, and to using the Pd estimated from this run as an association
parameter.

Geometry and independent timing/kinematic information are appropriate.
Within-scan timestamp residual could be added if it contains reliable
information independent of azimuth; in a rotating radar it may largely
duplicate the azimuth residual.

## Solver recommendation

Use a rectangular Jonker–Volgenant/shortest-augmenting-path assignment solver
per connected component.

Reasons:

- it solves the required weighted one-to-one assignment exactly;
- rectangular matrices and private dummy columns map directly to the
  independent per-scan association problem;
- cubic worst-case complexity is entirely reasonable for small ambiguity
  components;
- it is generally faster than traditional Hungarian implementations;
- unlike greedy or current DFS ordering, it optimises the explicit global
  objective.

Sparse min-cost flow is an equally correct alternative and maps naturally onto
the existing adjacency lists. It is attractive if ambiguity components become
large and sparse, but a dedicated assignment solver is simpler for the current
problem.

The current DFS augmenting-path code is sometimes called the Kuhn algorithm.
It should not be confused with the weighted Kuhn–Munkres/Hungarian assignment
algorithm.

## Connected-component decomposition

Construct a bipartite graph containing:

- one node for every base point;
- one node for every measured plot;
- an edge for every pair passing the validation gate.

Split it into connected components using DSU or BFS.

Then:

- an isolated base point is immediately a missed detection;
- an isolated measured plot is immediately unassigned;
- a non-conflicting degree-one pair can be decided directly in the
  maximum-cardinality mode;
- only components containing contested plots need a full assignment solve.

Because there are no cross-component edges and the objective is additive,
component-wise optimisation is exactly equivalent to solving one monolithic
matrix.

This is especially useful for the 800-target scenario: rather than solving one
large dense matrix, the scan should normally reduce to many trivial components
and a limited number of small FIX/VAR ambiguity components.

Candidate generation can initially remain the current \(O(NM)\) loop. If
measured clutter counts later make this expensive, range/azimuth binning or a
spatial index can reduce candidate construction work.

## Determinism

The current candidate and opportunity `std::sort` comparators do not define a
final tie-break. Equivalent-cost elements can therefore change order when
inputs or toolchains change, which can alter per-target and Correct-A
statistics.

Recommended rules:

- canonicalise base-point order by stable target ID or source index;
- canonicalise measured plots by timestamp plus a stable source index;
- quantise costs below meaningful sensor resolution;
- make the solver choose the first canonical column on an exact reduced-cost
  tie;
- avoid arbitrary floating-point epsilon perturbations that could turn a
  genuinely better physical solution into a tie.

## Recommended result structure and statistics

The association operation should return at least:

- measured-plot index for every base point, with `-1` for a miss;
- list of unassigned measured-plot indices;
- range and azimuth residuals for every selected pair;
- individual and total association costs;
- connected-component/ambiguity identifier;
- optionally the best-versus-second-best assignment cost gap.

The result window should distinguish:

- expected base points;
- associated base points;
- missed base points;
- correct-A associations;
- wrong-A associations;
- unassigned measured plots;
- ambiguous components.

An unassigned measured plot should initially be labelled "unassigned", not
unconditionally "false alarm": it may also be a valid return displaced outside
the configured gate.

## Suggested code organisation

Move the association logic out of the anonymous namespace in
`scenarioresolutiontask.cpp` into a pure, testable component, for example:

```cpp
struct AssociationCandidate
{
    int baseIndex;
    int plotIndex;
    double rangeResidualMeters;
    double azimuthResidualDegrees;
    double cost;
};

struct AssociationResult
{
    QVector<int> plotForBase;          // -1: missed detection
    QVector<int> unassignedPlots;
    QVector<AssociationCandidate> selectedPairs;
    double totalCost;
};

AssociationResult associateScan(
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots,
        const AssociationConfig& config);
```

`associateScan()` must be stateless: its output is a pure function of the two
collections and fixed configuration. It must not accept previous-scan matches,
track state or a persistent ownership table.

The repository currently specifies `-std=c++0x` in
[`analyser.pro`](https://github.com/sky349/analyzer/blob/main/analyser.pro), so
the implementation should remain C++11-compatible unless the project standard
is intentionally upgraded.

Use the same pure association function for:

- each independent scan evaluated during automatic scenario-epoch scoring;
- the main scan-by-scan statistics pass;
- unit tests;
- optional diagnostic logging.

## Essential tests

### Functional cases

- no base points and no plots;
- only base points;
- only measured plots;
- fewer plots than base points;
- equal counts;
- more plots than base points;
- every candidate outside the gate;
- candidate exactly on the range or azimuth gate;
- azimuth wrapping across \(359.9^\circ/0.1^\circ\);
- one measured plot is never assigned twice;
- one base point never receives two plots.

### Optimisation cases

- the 2×2 cost counterexample in this document;
- augmenting-path rescue where maximum cardinality requires displacement;
- maximum-cardinality mode forces a valid but expensive edge when necessary;
- disconnected component result equals monolithic result;
- exact ties produce a documented deterministic assignment;
- permutations of base and plot input order preserve a unique optimum;
- changing the contents of the previous or following scan does not change the
  current scan's result;
- processing scans in a different order produces identical per-scan results.

### Numerical and robustness cases

- duplicate-coordinate plots with different stable IDs;
- NaN and infinite position/cost inputs;
- singular or nearly singular residual covariance when Mahalanobis gating is
  enabled;
- integer cost scaling and forbidden-cost overflow;
- dense synthetic ambiguity component.

### Verification cases

- randomly generated matrices up to approximately 5×5 checked against
  exhaustive enumeration;
- existing scenario regression:
  - new cardinality equals current Kuhn cardinality;
  - new total geometric cost is no greater;
  - every association satisfies the configured gates;
- North-marker boundary case where an expected point and measured plot lie on
  opposite sides of a half-open scan boundary: verify deterministic scan
  ownership and do not associate across the boundary. If the pair should have
  belonged to one scan, report/fix the epoch or timestamp alignment instead of
  borrowing data from an adjacent scan.

## Alternatives and why they are not the default

| Method | Result | Recommendation |
|---|---|---|
| Sequential per-base nearest-neighbour | Order-dependent hard assignment | Do not use with overlapping gates |
| Globally sorted greedy edges | Unique but still suboptimal hard assignment | Not sufficient |
| Current Kuhn DFS | Exact maximum cardinality, arbitrary cost among those solutions | Retain only if cardinality is the sole objective |
| Hungarian / Jonker–Volgenant 2D assignment | Exact global hard assignment within one scan | Recommended |
| Sparse min-cost flow | Same exact optimum | Good alternative backend |
| Auction | Potentially useful for large parallel/distributed problems; floating-point epsilon needs care | No advantage here |
| JPDA | Soft marginal probabilities intended mainly for probabilistic state updates | Not needed: there is no filter update and Pd requires a hard per-scan count |
| MHT | Competing association histories over multiple scans | Excluded: it contradicts the independent-scan problem definition |
| Multidimensional assignment | Joint multi-scan or multisensor optimisation; generally NP-hard for 3D+ assignment | Excluded: scans must not be coupled |
| Murty k-best | Ranked alternative hard assignments within one scan | Useful optional ambiguity diagnostic |

## Acceptance criteria for the first upgrade

The initial minimum-cost maximum-matching implementation should be accepted
when:

1. It returns the same match cardinality as the current matcher for every
   regression scan.
2. Its total current geometric cost is never greater.
3. It never assigns a plot twice or assigns two plots to one base point.
4. It produces identical results for permuted input orders when the optimum is
   unique.
5. Exact ties follow a documented deterministic rule.
6. The result for scan \(s\) is unaffected by measured plots, base points or
   associations in every other scan.
7. The association component retains no state between calls.
8. The same per-scan implementation is used by epoch detection and final
   statistics; epoch detection may aggregate scores but not assignments.
9. Unassigned plots and missed base points are returned explicitly.

## Principal references

The assignment references below support the recommended within-scan solver.
The JPDA and MHT papers are included only to document the alternatives that
were considered and rejected for this independent-scan measurement.

1. D. F. Crouse, “On Implementing 2D Rectangular Assignment Algorithms,”
   *IEEE Transactions on Aerospace and Electronic Systems*, vol. 52, no. 4,
   pp. 1679–1696, 2016.  
   <https://doi.org/10.1109/TAES.2016.140952>

2. D. F. Crouse, “A Crash Course in Basic Single-Scan Target Tracking,”
   U.S. Naval Research Laboratory/ICERM lecture slides, 2017. Includes the
   GNN likelihood score, private missed-detection columns, JPDA comparison,
   Mahalanobis gating and clustering. The likelihood's \(P_D\) term is
   background theory, not a recommended input to the present Pd measurement.  
   <https://app.icerm.brown.edu/materials/Slides/sp-f17-w2/Intro._to_Single-Scan_Target_Tracking_%5D_David_Crouse%2C_Naval_Research_Laboratory.pdf>

3. R. Jonker and A. Volgenant, “A Shortest Augmenting Path Algorithm for Dense
   and Sparse Linear Assignment Problems,” *Computing*, vol. 38,
   pp. 325–340, 1987.  
   <https://doi.org/10.1007/BF02278710>

4. J. Munkres, “Algorithms for the Assignment and Transportation Problems,”
   *Journal of the Society for Industrial and Applied Mathematics*, vol. 5,
   no. 1, pp. 32–38, 1957.  
   <https://doi.org/10.1137/0105003>

5. T. E. Fortmann, Y. Bar-Shalom and M. Scheffe, “Sonar Tracking of Multiple
   Targets Using Joint Probabilistic Data Association,” *IEEE Journal of
   Oceanic Engineering*, vol. OE-8, no. 3, pp. 173–184, 1983.  
   <https://doi.org/10.1109/JOE.1983.1145560>

6. D. B. Reid, “An Algorithm for Tracking Multiple Targets,” *IEEE
   Transactions on Automatic Control*, vol. AC-24, no. 6, pp. 843–854, 1979.  
   <https://doi.org/10.1109/TAC.1979.1102177>

7. K. G. Murty, “An Algorithm for Ranking All the Assignments in Order of
   Increasing Cost,” *Operations Research*, vol. 16, no. 3,
   pp. 682–687, 1968.  
   <https://doi.org/10.1287/opre.16.3.682>

## Final decision

The immediate code change should be:

> Keep the existing candidate generation and gates, but replace the
> maximum-cardinality-only DFS selection with component-wise global
> minimum-cost maximum matching, executed from a clean state for each scan.

This is a contained, testable improvement that preserves current aggregate
detection semantics while eliminating order-sensitive, cost-suboptimal
associations. It requires no tracker, no temporal association and no state
propagation. Statistical gating may later be calibrated from independent
measurement-error/background data, but Pd measured by this task must not be
fed back into its own association rule.
