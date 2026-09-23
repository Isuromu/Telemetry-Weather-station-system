# Import history

The source was imported from `suv.zip`. Its original Git history is preserved
verbatim on the local branch `archive/suv-waterlevel`. The commits were also
replayed under `examples/WaterLevel` so their file changes remain visible in
the main repository history despite the added path prefix.

Original commits, oldest first:

1. `20a72b00399d137b0c0bc4badb3f892bbddbafe3` — `Initial commit`
   - added `.gitignore`;
   - added `include/Pneumatic Water Gauge Sensor-RD-RWG-01.pdf`;
   - added `include/README`, `lib/README`, `src/main.cpp`,
     and `test/README`.
2. `cf7dcc884650d527ffb1802f66c6bc4826230f3a` —
   `Corrected sensor data parsing according to Datasheet documenta and given prob profile`
   - modified `src/main.cpp`.
3. `a23fac1c626b086d30bc668fed598a078a56fa55` —
   `Battery measuring logic Updated`
   - modified `src/main.cpp`.

Generated `.pio` output and IDE/tool state from the ZIP were not imported.
