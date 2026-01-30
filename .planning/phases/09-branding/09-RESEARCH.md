# Phase 9: Branding - Research

**Researched:** 2026-01-29
**Domain:** VCV Rack plugin metadata and library submission
**Confidence:** HIGH

## Summary

VCV Rack uses a three-level hierarchy for plugin identification: **Brand → Plugin → Modules**. The branding phase for this project involves updating `plugin.json` metadata fields to rebrand from "HydraQuartet" to "Synth-etic Intelligence" while preserving the module name "Triax VCO". The critical constraint is that the `slug` field (currently "HydraQuartet") should **not** be changed as this would break backwards compatibility with existing patches.

The standard approach is to update the `brand` field (which becomes the manufacturer name in the VCV Library), update the `name` field (the plugin's display name), and ensure the `author` field is populated. The `slug` must remain unchanged to maintain patch compatibility. Panel SVG files do not typically display manufacturer branding, focusing instead on module names.

VCV Rack reads plugin metadata from `plugin.json` at the root directory. The distribution system (`make dist`) packages files from the `res/` directory (containing SVG panels) into a `.vcvplugin` file named using the slug, creating `dist/<slug>-<version>-<os>-<cpu>.vcvplugin`.

**Primary recommendation:** Update `brand` to "Synth-etic Intelligence", keep `slug` as "HydraQuartet", update `name` to describe the plugin, populate `author` field, and optionally add manufacturer text to panel SVG.

## Standard Stack

VCV Rack plugin branding requires no external libraries or tools. The stack consists entirely of VCV Rack's built-in plugin manifest system.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| plugin.json | VCV Rack 2.x | Plugin manifest | Required manifest format for VCV Rack 2.x |
| VCV Rack SDK | 2.x | Build and packaging | Official SDK provides plugin.mk for distribution |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| SVG editor | Any | Panel graphics | Optional: if adding brand text to panel |
| Text editor | Any | Editing plugin.json | Required: for metadata updates |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Manual JSON editing | helper.py script | Script is for initial plugin creation only; manual editing is standard for updates |

**Installation:**
```bash
# No installation required
# Edit plugin.json in place
# Use existing VCV Rack SDK Makefile for distribution
```

## Architecture Patterns

### VCV Rack Plugin Identification Hierarchy

**Three-level structure:**
```
Brand (manufacturer/developer)
  └── Plugin (collection of modules)
      └── Modules (individual synthesizer units)
```

**Example from VCV Fundamental:**
- Brand: "VCV"
- Plugin Name: "VCV Free"
- Plugin Slug: "Fundamental"
- Module Names: "VCV VCO", "VCV VCF", etc. (brand prefixes module names)

**Example from stoermelder PackTau:**
- Brand: "stoermelder"
- Plugin Name: "PackTau"
- Plugin Slug: "Stoermelder-PackTau"
- Author: "Benjamin Dill"

### plugin.json Structure

```json
{
  "slug": "UniqueIdentifier",        // IMMUTABLE - do not change after release
  "name": "Display Name",            // Mutable - can be changed anytime
  "brand": "Manufacturer Name",      // Mutable - appears as manufacturer in library
  "author": "Developer Name",        // Required - person or organization
  "version": "2.0.0",               // Semantic versioning (major.minor.patch)
  "license": "GPL-3.0-or-later",    // SPDX identifier or "proprietary"

  "authorEmail": "",                // Optional
  "authorUrl": "",                  // Optional
  "pluginUrl": "",                  // Optional
  "manualUrl": "",                  // Optional
  "sourceUrl": "",                  // Optional
  "donateUrl": "",                  // Optional
  "changelogUrl": "",              // Optional
  "minRackVersion": "2.4.0",       // Optional - minimum Rack version

  "modules": [
    {
      "slug": "ModuleSlug",          // IMMUTABLE - module identifier
      "name": "Module Display Name",  // Mutable - shown in module browser
      "description": "Brief description",
      "tags": ["Oscillator", "Polyphonic"]
    }
  ]
}
```

### Recommended Project Structure
```
plugin-root/
├── plugin.json          # Manifest at root (required)
├── Makefile            # Build configuration
├── src/                # C++ source files
│   ├── plugin.cpp
│   ├── plugin.hpp
│   └── *.cpp
├── res/                # Panel SVGs (distributed)
│   └── *.svg
├── build/              # Build artifacts (gitignored)
├── dep/                # Dependencies (gitignored)
└── dist/               # Distribution packages (gitignored)
```

### Pattern 1: Slug Immutability

**What:** The `slug` field in plugin.json acts as the unique, permanent identifier for the plugin and must never change after initial release.

**When to use:** Always. Slug is set once during plugin creation and preserved for the lifetime of the plugin.

**Why immutable:** VCV Rack patches save module references using `pluginSlug:moduleSlug` format. Changing the plugin slug breaks all existing patches that use your modules.

**Example:**
```json
{
  "slug": "HydraQuartet",  // Set once, never changed
  "name": "Synth-etic Intelligence Triax",  // Can be updated for rebranding
  "brand": "Synth-etic Intelligence"        // Can be updated for rebranding
}
```

### Pattern 2: Brand as Manufacturer Identity

**What:** The `brand` field defines the manufacturer/developer identity shown in the VCV Library browser. If omitted, defaults to plugin `name`.

**When to use:** Use `brand` when you want a consistent manufacturer identity across multiple plugins, or when your plugin name differs from your brand identity.

**Example:**
```json
{
  "brand": "Synth-etic Intelligence",  // Manufacturer shown in library
  "name": "Triax Collection",          // Plugin collection name
  "author": "Your Name"                // Actual developer name
}
```

### Pattern 3: Distribution Folder Naming

**What:** The `make dist` command creates distribution packages named `<slug>-<version>-<os>-<cpu>.vcvplugin`.

**When to use:** Every release.

**Important:** The dist folder structure uses the slug for package naming, not the brand or name fields. This reinforces why slug must remain stable.

**Example:**
```bash
make dist
# Creates: dist/HydraQuartet-2.0.0-mac-arm64.vcvplugin
# Even though brand is "Synth-etic Intelligence"
```

### Anti-Patterns to Avoid

- **Changing slug for rebranding:** Never change the slug. It breaks patch compatibility. Use `brand` and `name` fields instead.
- **Omitting author field:** The `author` field is required for VCV Library submission. Always populate it.
- **Using spaces in slugs:** Slugs must use only `a-z`, `A-Z`, `0-9`, `-`, and `_`. No spaces allowed.
- **Adding brand to module slug:** Module slugs should be descriptive of the module, not prefixed with brand. The brand field handles manufacturer identification.

## Don't Hand-Roll

VCV Rack provides built-in systems for branding and identification. Do not attempt custom solutions.

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Plugin identification | Custom registry system | plugin.json manifest | VCV Rack requires plugin.json; custom systems ignored |
| Distribution packaging | Manual ZIP creation | `make dist` | SDK Makefile handles platform-specific packaging correctly |
| Module browser display | Custom UI overlays | `brand` and `name` fields | VCV Rack browser reads metadata directly from manifest |
| Versioning system | Custom version tracking | Semantic versioning in plugin.json | VCV Library requires semver format (major.minor.patch) |
| Slug generation | Dynamic slug creation | Static slug in plugin.json | Changing slugs breaks patch compatibility |

**Key insight:** VCV Rack's plugin system is declarative, not programmatic. All branding, identification, and metadata is defined in plugin.json and read by Rack at load time. Attempting to override or extend this with custom code will not work and may break library submission.

## Common Pitfalls

### Pitfall 1: Changing Slug During Rebrand

**What goes wrong:** Developer changes `slug` field when rebranding, breaking all existing user patches.

**Why it happens:** Confusion between slug (permanent identifier) and name/brand (display fields). Developers assume changing the brand requires changing the slug.

**How to avoid:**
- Understand the slug is analogous to a database primary key - it never changes
- Use `brand` field for manufacturer identity changes
- Use `name` field for plugin display name changes
- Document clearly: "slug = permanent ID, brand/name = changeable labels"

**Warning signs:**
- User reports: "My patches can't find modules anymore"
- VCV Rack console errors: "Could not find module <old-slug>:<module-slug>"

**Prevention strategy:**
```json
// WRONG - breaks patches
{
  "slug": "SyntheticIntelligence",  // Changed from "HydraQuartet"
  "brand": "Synth-etic Intelligence"
}

// CORRECT - preserves compatibility
{
  "slug": "HydraQuartet",           // Never change
  "brand": "Synth-etic Intelligence", // Change for rebrand
  "name": "Synth-etic Intelligence Triax"
}
```

### Pitfall 2: Empty Author Field

**What goes wrong:** Plugin.json has empty `author` field, causing VCV Library submission rejection.

**Why it happens:** The `author` field appears optional in the JSON schema, but VCV Library **requires** it for submission.

**How to avoid:** Always populate the `author` field with developer name, company name, GitHub username, or alias.

**Warning signs:**
- Library submission process asks for author information
- Plugin appears incomplete in library metadata

**Prevention strategy:**
```json
{
  "brand": "Synth-etic Intelligence",
  "author": "Your Name or Company",  // Required for library submission
  "authorEmail": "contact@example.com"
}
```

### Pitfall 3: Misunderstanding Brand vs Name vs Slug

**What goes wrong:** Confusion about which field controls what, leading to inconsistent metadata or broken functionality.

**Why it happens:** Three similar-sounding fields with overlapping purposes create cognitive overhead.

**How to avoid:** Use this mental model:
- **slug:** Permanent database ID (never shown to users, never changes)
- **brand:** Manufacturer label (shown in library, can change)
- **name:** Plugin collection label (shown in library, can change)
- **author:** Developer identity (required, can change)

**Warning signs:**
- Brand shows up as plugin name in unexpected places
- Module browser displays confusing or duplicate names
- Users can't find plugin under expected manufacturer

**Prevention strategy:**
```json
{
  "slug": "HydraQuartet",              // Technical ID - invisible to users
  "brand": "Synth-etic Intelligence",   // What users see as manufacturer
  "name": "Triax VCO Collection",       // What users see as plugin
  "author": "Developer Name"            // Who created it
}
```

### Pitfall 4: Modifying SVG Panel Without Understanding Componentization

**What goes wrong:** Developer edits panel SVG to add branding, inadvertently breaking VCV Rack's component detection system.

**Why it happens:** VCV Rack's `createmodule` helper parses SVG component IDs to generate C++ boilerplate. Adding text elements without understanding this can break the parsing.

**How to avoid:**
- Panel SVG branding is **optional** - brand field in plugin.json is sufficient
- If adding manufacturer text to panel, use standalone `<text>` elements without special IDs
- Keep branding elements separate from component definitions (ports, params, inputs, outputs)
- Test with `make` after SVG changes to ensure build still works

**Warning signs:**
- `createmodule` script fails to find expected components
- Build errors about missing port or parameter definitions
- Module loads with missing controls or incorrect layout

**Prevention strategy:** Add branding text as non-component elements:
```svg
<!-- Safe: standalone text element -->
<text style="..." x="101.6" y="120">Synth-etic Intelligence</text>

<!-- Avoid: text with component-like ID -->
<text id="BrandLabel_Widget" ...>Synth-etic Intelligence</text>
```

### Pitfall 5: Folder Structure Mismatch

**What goes wrong:** Distribution package has incorrect folder structure, causing installation failures.

**Why it happens:** Manual packaging or misunderstanding of how `make dist` works.

**How to avoid:** Always use `make dist` from the SDK Makefile. It handles correct folder structure automatically.

**Warning signs:**
- Plugin installs but doesn't appear in module browser
- VCV Rack logs "Could not load plugin.json"
- Missing panel graphics or resources

**Prevention strategy:**
```bash
# CORRECT - use SDK Makefile
make dist

# WRONG - manual packaging
zip -r MyPlugin.zip src/ res/ plugin.json
```

## Code Examples

### Complete plugin.json for Rebranding

```json
{
  "slug": "HydraQuartet",
  "name": "Synth-etic Intelligence Triax",
  "version": "2.0.0",
  "license": "GPL-3.0-or-later",
  "brand": "Synth-etic Intelligence",
  "author": "Your Name",
  "authorEmail": "contact@example.com",
  "authorUrl": "https://yourwebsite.com",
  "pluginUrl": "https://yourwebsite.com/triax-vco",
  "manualUrl": "https://yourwebsite.com/triax-vco/manual",
  "sourceUrl": "https://github.com/yourusername/vcvrack_modules",
  "donateUrl": "",
  "changelogUrl": "",
  "modules": [
    {
      "slug": "HydraQuartetVCO",
      "name": "Triax VCO",
      "description": "8-voice polyphonic dual-VCO with through-zero FM",
      "tags": ["Oscillator", "Polyphonic", "Hardware Clone"]
    }
  ]
}
```
Source: Based on [VCV Rack Plugin Manifest documentation](https://vcvrack.com/manual/Manifest)

### Optional: Adding Manufacturer Text to Panel SVG

```svg
<!-- Add near bottom of panel, after other text elements -->
<text
   style="font-size:3px;font-family:sans-serif;fill:#666677;text-anchor:middle"
   x="101.6"
   y="124">Synth-etic Intelligence</text>
```

### Build and Distribution Commands

```bash
# Build plugin
make

# Create distribution package
make dist

# Install to local Rack for testing
make install

# Clean build artifacts
make clean
```
Source: [VCV Rack Building documentation](https://vcvrack.com/manual/Building)

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual manifest editing only | helper.py createplugin for initialization | VCV Rack 1.0 | Initial plugin setup automated, but updates still manual |
| Plugin directory in Rack folder | User folder plugins directory | VCV Rack 1.0 | Better separation of user-installed vs system plugins |
| Arbitrary version strings | Semantic versioning required | VCV Rack 1.0 | Standardized version comparison |
| Optional minRackVersion | Enforced minRackVersion check | VCV Rack 2.4.0 | Prevents incompatible plugin downloads |

**Deprecated/outdated:**
- **v0.6 plugin format:** Used different manifest structure. All plugins must migrate to v1/v2 format.
- **Spaces in slugs:** VCV Rack 1.0+ automatically strips spaces from slugs in patches for backwards compatibility, but new slugs must not contain spaces.
- **Brand-less plugins:** While `brand` field is technically optional, modern best practice is to always specify it for clear manufacturer identification in the library.

## Open Questions

1. **Panel SVG manufacturer branding**
   - What we know: Panel SVG currently shows "TRIAX VCO" as module name, no manufacturer branding
   - What's unclear: Whether to add "Synth-etic Intelligence" text to panel SVG or rely solely on plugin.json brand field
   - Recommendation: Brand field in plugin.json is sufficient. Only add SVG text if user explicitly wants visible manufacturer branding on panel. Most commercial VCV Rack modules show manufacturer in library but keep panels focused on module name.

2. **Author field content**
   - What we know: Field is required for VCV Library submission
   - What's unclear: User's preferred author name (personal name, company name, alias, etc.)
   - Recommendation: Populate with whatever identity the user wants associated with the plugin. Common patterns: personal name, company name, or GitHub username.

3. **Plugin name vs brand distinctiveness**
   - What we know: Brand becomes manufacturer, name becomes plugin collection name
   - What's unclear: Whether plugin name should be "Synth-etic Intelligence Triax", just "Triax", or something else
   - Recommendation: Since there's only one module, simple approaches work: brand="Synth-etic Intelligence", name="Triax VCO" mirrors the module name, or name="Synth-etic Intelligence" (brand defaults to name if omitted).

## Sources

### Primary (HIGH confidence)
- [VCV Rack Plugin Manifest](https://vcvrack.com/manual/Manifest) - Complete manifest field documentation
- [VCV Rack Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial) - Official setup and structure guide
- [VCV Library GitHub README](https://github.com/VCVRack/library/blob/master/README.md) - Library submission requirements
- [VCV Fundamental plugin.json](https://github.com/VCVRack/Fundamental/blob/v2/plugin.json) - Reference implementation from official plugin
- [stoermelder PackTau plugin.json](https://github.com/stoermelder/vcvrack-packtau/blob/v2/plugin.json) - Third-party example showing brand/name/author distinction

### Secondary (MEDIUM confidence)
- [VCV Community: Plugin tags, names, and brands](https://community.vcvrack.com/t/plugin-tags-names-and-brands/4262) - Discussion of brand vs plugin hierarchy
- [VCV Community: Module slug naming convention](https://community.vcvrack.com/t/modules-slug-naming-convention/6541) - Slug immutability and backwards compatibility
- [VCV Rack Building documentation](https://vcvrack.com/manual/Building) - make dist command and folder structure

### Tertiary (LOW confidence)
- None - all findings verified with official documentation or authoritative source code examples

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - No external dependencies, only plugin.json editing required
- Architecture: HIGH - Official documentation and multiple reference implementations confirm patterns
- Pitfalls: HIGH - Slug immutability and patch compatibility issues are well-documented in official migration guides and community discussions

**Research date:** 2026-01-29
**Valid until:** 90 days (VCV Rack plugin manifest format is stable; changes occur only with major Rack versions)
