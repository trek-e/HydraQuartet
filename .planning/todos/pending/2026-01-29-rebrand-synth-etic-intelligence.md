---
created: 2026-01-29T22:15
title: Rebrand plugin to Synth-etic Intelligence
area: branding
files:
  - dist/HydraQuartet/plugin.json
  - dist/HydraQuartet/res/HydraQuartetVCO.svg
---

## Problem

The plugin needs proper branding before release. Current plugin name is "HydraQuartet" but should be rebranded to "Synth-etic Intelligence" as the manufacturer/brand name, with "HydraQuartet VCO" (or similar) remaining as the module name.

This affects:
- plugin.json: slug, name, brand fields
- Panel SVG: any brand text on the panel
- VCV Library listing: how it appears to users

## Solution

1. Update plugin.json:
   - `"slug": "SyntheticIntelligence"` (or similar valid slug)
   - `"name": "Synth-etic Intelligence"`
   - `"brand": "Synth-etic Intelligence"`
   - Keep module name as current (e.g., "Triax VCO" or "HydraQuartet VCO")

2. Update panel SVG if brand name appears on panel

3. Ensure folder structure matches new slug for VCV plugin directory

TBD: Confirm exact spelling/formatting of brand name (hyphen, spacing, capitalization)
