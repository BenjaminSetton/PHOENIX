---
description: Perform a thorough cleanup analysis of the PHX library (including samples) to find redundant code, duplication, misplaced logic, and DRY principle violations. The output MUST be a new markdown file written to disk — do not output the report as a chat message.
---

# phx_cleanup_report

Perform a thorough cleanup and redundancy analysis of the PHX graphics library, including all sample projects. The goal is to **identify redundant code, duplicated logic, misplaced functionality, unreferenced functions, and DRY principle violations**, then provide concrete, actionable recommendations for cleanup. The report MUST be written to a **new file on disk** (see Step 0). Do not output the report as a chat message — the file IS the deliverable. Follow these steps in order:

## 0. Create the report file and header

**This is a mandatory output step.** The entire report MUST be written to a new file on disk using the `write_to_file` tool. Do NOT output the report as a chat message — the file IS the deliverable.

Write the final report to the user's personal notes directory at `C:\Users\benja\OneDrive\Desktop\Docs\Cleanup Report\`. Create the `Cleanup Report` folder if it does not exist.

The filename must follow the format: `cleanup_report_<date>_<index>.md`

- `<date>`: current date in `YYYY-MM-DD` format
- `<index>`: zero-based integer. If no other `cleanup_report_<date>_*.md` files exist for today, use `0`. Otherwise, use the next available index (e.g., if `cleanup_report_2026-07-21_0.md` exists, use `cleanup_report_2026-07-21_1.md`).

At the very beginning of the file, create a header with the following format:

```
# Cleanup Report

**Date:** <current date>
**Model:** <name of the LLM model used to generate this report>
```

Replace `<current date>` with today's date and `<name of the LLM model used to generate this report>` with the actual model name (e.g., "Claude Opus 4.8").

## 1. Map the codebase

- List all source files under `PHOENIX/src/lib/` and `PHOENIX/src/api/` (recursively), including `core/`, `platform/vulkan/`, `utils/`, and any other subdirectories.
- List all source files under `samples/` (recursively), including every sample project's `src/` directory.
- Read every `.cpp` and `.h` file in the library and in the samples. Do not skip files.
- For each file, understand its purpose, what subsystems it touches, and how it interacts with other components.
- Build a mental model of the overall codebase: public API surface, internal interfaces, backend implementations, sample usage patterns, and data flow.

## 2. Analyze each subsystem systematically

Go through each subsystem and evaluate it for redundancy and cleanup opportunities in these categories:

### Unreferenced / Dead Code
- Functions or methods that are declared and defined but never called anywhere in the library or samples
- Classes or structs that are defined but never instantiated
- Enum values that are never used
- `#include` directives that are unnecessary (the included headers are not used)
- Preprocessor macros that are never expanded
- Entire files that are not part of any build target or are orphaned
- Commented-out code blocks that serve no purpose
- `TODO` / `FIXME` / `HACK` comments that reference already-completed work or are stale

### Code Duplication (DRY Principle)
- Functions or logic blocks that are duplicated across multiple files with minor or no differences
- Repeated initialization sequences that could be factored into a helper
- Repeated Vulkan boilerplate (e.g., barrier creation, info struct setup) that could be abstracted
- Duplicate type conversion functions or mapping tables
- Repeated error-handling patterns that could use a shared macro or utility
- Similar or identical helper functions in samples that should be promoted to the library
- Copy-pasted shader setup, pipeline creation, or resource creation code

### Misplaced Code (Wrong Context)
- Functions or logic that live in a file/class that doesn't own that responsibility (e.g., `texture_vk` calling into the render graph, or holding pointers to objects it shouldn't)
- Cross-layer coupling that violates the dependency direction (api → core → platform): e.g., platform code reaching into API types, or core code depending on platform internals
- Utility functions defined locally in a subsystem that belong in a shared utils module
- Library-internal logic leaking into sample code (and vice versa: sample-level concerns embedded in library code)
- Functions defined in headers that should be in .cpp files (causing potential ODR issues or unnecessary recompilation)
- Static/global state in a class that should be instance-level

### Redundant Checks and Logic
- Redundant null checks (checking for null when the pointer is guaranteed non-null by prior logic or invariants)
- Redundant state validation (checking the same condition multiple times in a single code path)
- Redundant Vulkan calls (e.g., calling `vkCmdBindPipeline` twice without a state change in between)
- Redundant barrier insertions (barriers that are no-ops or duplicate a prior barrier's effect)
- Redundant cache lookups (looking up the same cached object multiple times in a single function)
- Redundant member variables that duplicate state available elsewhere
- Redundant wrapper functions that add no value over calling the underlying function directly
- Redundant `if` guards around operations that are already safe (e.g., checking a flag that was just set)

### Redundant Abstractions
- Interfaces or abstract base classes with a single implementation and no planned extensibility
- Wrapper classes that simply forward every call to an inner object with no added logic
- Unnecessary indirection layers (e.g., calling through a manager that just forwards to the underlying object)
- Factory functions that always return the same type
- Configuration structs with fields that are never read

### Redundant Includes and Dependencies
- Header files that include more than necessary (increasing compile times)
- Circular or unnecessary include dependencies between modules
- Source files that include headers they don't use
- Forward declarations that could replace full includes (or vice versa, full includes where a forward declaration suffices)

## 3. Cross-reference subsystems

After analyzing each subsystem individually, check for inter-subsystem redundancy and cleanup issues:

- Are there utility functions duplicated across `core/`, `platform/vulkan/`, and `utils/` that should be consolidated?
- Are type conversion or mapping functions duplicated between the API layer and the platform layer?
- Do multiple subsystems implement their own version of the same pattern (e.g., hashing, caching, handle lookup) when a shared implementation would suffice?
- Are there sample projects that duplicate library functionality instead of using the library API?
- Are there helper functions in samples that are duplicated across multiple samples and should be promoted to the library?
- Are there header files that are included by many translation units but only provide a small subset of their declarations to each one (should be split)?

## 4. Analyze samples for library gaps and duplication

For each sample project under `samples/`:

- Identify code in the sample that duplicates library functionality (the sample should be using the library, not reimplementing it)
- Identify helper functions in samples that are duplicated across multiple samples
- Identify sample code that works around a library limitation by reimplementing logic that should be in the library
- Note any sample code that accesses library internals it shouldn't need to
- Check for shared boilerplate across samples (e.g., window creation, device initialization, swapchain setup) that could be factored into a shared sample utility or library helper

## 5. Document findings

For each cleanup issue found, document:

- **ID**: Sequential identifier (e.g., CLEAN-1, CLEAN-2)
- **Category**: Unreferenced Code / Code Duplication / Misplaced Code / Redundant Checks / Redundant Abstractions / Redundant Includes
- **Severity**: Critical / High / Medium / Low
  - **Critical**: Code that is actively harmful (e.g., duplicated logic that has already diverged and caused bugs, or dead code that confuses maintainers into thinking it's active)
  - **High**: Significant duplication or misplaced code that substantially increases maintenance cost and bug risk
  - **Medium**: Cleanup that would improve maintainability and reduce code size but isn't causing immediate problems
  - **Low**: Minor polish, style cleanup, or small redundancy removals
- **File and line number(s)** where relevant
- **Code snippet** showing the issue (if applicable)
- **Explanation** of what's redundant, duplicated, or misplaced and why it matters
- **Impact** — how this affects maintainability, compile times, bug risk, or onboarding
- **Recommendation** — a concrete cleanup action (e.g., "move function X to utils/", "delete unreferenced function Y", "extract shared helper Z from samples A and B into the library"). This is **required** for every finding.

Group findings by category first, then by severity within each category (Critical first, then High, Medium, Low).

## 6. Summary

Provide a summary table with all findings, their category, severity, affected file(s), and a brief description. Place it at the beginning of the file, immediately after the report header.

## 7. Final output reminder

**The report MUST be saved as a new file on disk.** Verify that the file was successfully written using the `write_to_file` tool before completing. Do not output the report content as a chat message — the file is the deliverable. If the file was not created, the workflow has failed.
