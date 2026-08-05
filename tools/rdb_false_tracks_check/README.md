# RDB false-track diagnostic

This standalone command-line tool parses an RDB recording through the same
ASTERIX, radar-data, and radar-map libraries used by Analyser. It reports:

- RDB record/category and decoded radar/source counts;
- raw, exact-unique, and physical North-marker counts per radar;
- North-marker timing and common rotation gaps;
- continuous track-instance and inferred-ended-instance counts;
- ended false PSR/SSR tracks for configurable point thresholds.

The tool preserves the GUI application's semantics:

- PSR scans use radar 51 North markers and SSR scans use radar 50 markers;
- North reports less than one second apart are one antenna rotation;
- continuous track instances use the same 30-second, source, address, and
  position-continuity checks as `AppWindow`;
- a track is ended by an explicit endpoint, later reuse/discontinuity of the
  same track number, or more than 30 seconds of subsequent recording data.

## Build on Windows

From this directory, with the repository's Qt/MinGW installation available:

```powershell
$analyserLibDir = (Resolve-Path ..\..\..\..\..\out\lib).Path
$env:Path = "$analyserLibDir;C:\Qt\Tools\mingw810_64\bin;C:\Qt\5.15.2\mingw81_64\bin;$env:Path"
C:\Qt\5.15.2\mingw81_64\bin\qmake.exe rdb_false_tracks_check.pro
C:\Qt\Tools\mingw810_64\bin\mingw32-make.exe release
```

## Run

```powershell
.\release\rdb_false_tracks_check.exe C:\recordings\sample.rdb
```

Optional arguments:

```text
--thresholds 3,10       Comma-separated maximum track lengths
--begin ISO_DATETIME    Analysis interval start (UTC when no offset is given)
--end ISO_DATETIME      Analysis interval end
```

If Begin/End are omitted, the complete RDB recording range is used. Track
length always uses the complete imported continuous instance; Begin/End only
select which physical scans and track-ending events are reported.
