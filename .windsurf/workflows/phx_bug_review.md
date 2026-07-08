---
description: Deeply analyze the entire PHX library for potential bugs, correctness issues, and design problems
---

# phx_bug_review

Perform a thorough, deep bug analysis of the PHX graphics library. Follow these steps in order:

## 0. Create the report file and header

Write the final report to the user's personal notes directory at `C:\Users\benja\OneDrive\Desktop\Docs\Bug Review\`. Create the `Bug Review` folder if it does not exist.

The filename must follow the format: `bug_review_<date>_<index>.md`

- `<date>`: current date in `YYYY-MM-DD` format
- `<index>`: zero-based integer. If no other `bug_review_<date>_*.md` files exist for today, use `0`. Otherwise, use the next available index (e.g., if `bug_review_2026-07-07_0.md` exists, use `bug_review_2026-07-07_1.md`).

At the very beginning of the file, create a header with the following format:

```
# Bug Report

**Date:** <current date>
**Model:** <name of the LLM model used to generate this report>
```

Replace `<current date>` with today's date and `<name of the LLM model used to generate this report>` with the actual model name (e.g., "Claude Opus 4.8").

## 1. Map the codebase

- List all source files under `PHOENIX/src/lib/` (recursively), including `core/`, `platform/vulkan/`, `utils/`, and any other subdirectories.
- Read every `.cpp` and `.h` file in the library. Do not skip files.
- For each file, understand its purpose, what engine APIs it touches, and how it interacts with other components.

## 2. Analyze each subsystem systematically

Go through each subsystem and look for bugs in these categories:

### Correctness
- Logic errors (wrong conditions, off-by-one, swapped arguments)
- Missing null checks before dereferencing
- Missing default cases in switch statements
- Unhandled enum values
- Incorrect type conversions or casts
- Wrong graphics API usage (wrong flags, wrong layouts, wrong stages)
- Missing graphics API spec compliance (layout transitions, barrier synchronization, destroy ordering)

### Resource management
- Memory leaks (missing `delete`, missing `vkDestroy*`, missing `vmaDestroy*`)
- Use-after-free (dangling pointers, invalidated iterators)
- Double-free possibilities
- Missing ref count decrements
- Vector reallocation invalidating stored pointers/iterators

### Synchronization
- Missing or incorrect pipeline barriers
- Wrong pipeline stage flags
- Wrong access flags
- Missing layout transitions
- Incorrect semaphore/fence usage

### Caching
- Cache keys missing relevant fields (hash or equality)
- Cache lookups returning wrong objects
- Cache entries not properly destroyed on deletion

### Error handling
- Ignored return values
- Missing error propagation
- Silent failures (TODO(), no-ops in error paths)
- Crashes in debug builds from TODO() or ASSERT

### Thread safety
- Shared mutable state without synchronization
- Global static buffers

## 3. Cross-reference subsystems

After analyzing each file individually, check for inter-subsystem issues:

- Handle system: Are handles invalidated when resources are deleted? Does `operator=` correctly manage ref counts?
- Render graph: Are barriers correct between passes? Are layouts tracked correctly across passes?
- Device context: Are command buffers properly managed? Are queue submissions correct?
- Caches: Do cache keys include all relevant Vulkan state (render pass, formats, etc.)?
- Type converters: Are all enum values handled? Are output types correct?

## 4. Document findings

For each bug found, document:

- **ID**: Sequential identifier (e.g., BUG-1, BUG-2)
- **Severity**: Critical / High / Medium / Low
- **File and line number(s)**
- **Code snippet** showing the bug
- **Explanation** of what's wrong and why
- **Impact** — what goes wrong at runtime
- **Suggested fix** — a concrete code-level fix or approach to resolve the bug. This is **required** for every finding.

Group findings by severity (Critical first, then High, Medium, Low).

## 5. Summary

Provide a summary table with all findings, their severity, file, and a brief description. Place it at the beginning of the file, immediately after the report header.
