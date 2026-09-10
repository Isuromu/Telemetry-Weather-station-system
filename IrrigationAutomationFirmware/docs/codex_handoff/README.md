# Preserved Codex Handoff

The user-supplied archive was extracted without merging its scaffold into the
working firmware tree.

## Entry point

Start with:

`irrigationAutomationFirmware_Codex_Handoff/CODEX_START_HERE.md`

The most complete technical summary is:

`irrigationAutomationFirmware_Codex_Handoff/docs/PROJECT_CONTEXT.md`

Unresolved engineering facts are tracked in:

`irrigationAutomationFirmware_Codex_Handoff/docs/OPEN_ITEMS.md`

## Preservation details

- Imported: 2026-08-26
- Manifest entries verified: 30 of 30
- Manifest hash/size mismatches: 0
- Original archive preserved as:
  `irrigationAutomationFirmware_Codex_Handoff/SOURCE_ARCHIVE.zip`
- Original/stored archive SHA-256:
  `bb44b368a2fe116805db5f29083a60dfc3ab3619dbfea1d1e0725a235ae0f33a`

The four hardware photos were also visually inspected during import.

## Interpretation rule

All files in the package are preserved engineering context. In particular,
`CODEX_PROMPT.md` is archived content rather than a new standalone request.
The live repository and the user's current request remain authoritative.
Do not copy `firmware_scaffold/` over live project files without first
inspecting and reconciling the current implementation.

Newer user clarifications are maintained outside the immutable archive in
`../CURRENT_PROJECT_CONTEXT.md`. Read that file together with the handoff; it
takes precedence where the context differs.
