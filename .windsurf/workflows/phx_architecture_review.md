---
description: Perform a deep architectural review of the PHX library to identify weaknesses and provide actionable improvement feedback. The output MUST be a new markdown file written to disk — do not output the review as a chat message.
---

# phx_architecture_review

Perform a thorough architectural and design review of the PHX graphics library. The goal is to **identify architectural weaknesses and provide concrete, actionable feedback for improvements**. The review MUST be written to a **new file on disk** (see Step 0). Do not output the review as a chat message — the file is the deliverable. Follow these steps in order:

## 0. Create the report file and header

**This is a mandatory output step.** The entire review MUST be written to a new file on disk using the `write_to_file` tool. Do NOT output the review as a chat message — the file IS the deliverable.

Write the final report to the user's personal notes directory at `C:\Users\benja\OneDrive\Desktop\Docs\Architecture Review\`. Create the `Architecture Review` folder if it does not exist.

The filename must follow the format: `architecture_review_<date>_<index>.md`

- `<date>`: current date in `YYYY-MM-DD` format
- `<index>`: zero-based integer. If no other `architecture_review_<date>_*.md` files exist for today, use `0`. Otherwise, use the next available index (e.g., if `architecture_review_2026-07-07_0.md` exists, use `architecture_review_2026-07-07_1.md`).

At the very beginning of the file, create a header with the following format:

```
# Architecture Review

**Date:** <current date>
**Model:** <name of the LLM model used to generate this report>
```

Replace `<current date>` with today's date and `<name of the LLM model used to generate this report>` with the actual model name (e.g., "Claude Opus 4.8").

## 1. Map the codebase

- List all source files under `PHOENIX/src/lib/` and `PHOENIX/src/api/` (recursively), including `core/`, `platform/vulkan/`, `utils/`, and any other subdirectories.
- Read every `.cpp` and `.h` file in the library. Do not skip files.
- For each file, understand its purpose, what subsystems it touches, and how it interacts with other components.
- Build a mental model of the overall architecture: public API surface, internal interfaces, backend implementations, and data flow.

## 2. Analyze each subsystem systematically

Go through each subsystem and evaluate its architectural quality in these categories:

### API Design & Boundaries
- Is the public API (`api/PHX/`) clean and free of internal implementation details?
- Are internal types (e.g., `HandleOwner`, `HandleAccessor`) leaking into public headers?
- Are there empty or placeholder structs that add noise (e.g., `DeviceContextCreateInfo`)?
- Is there type safety, or does the API rely on `void*` erasure and `static_cast`?
- Are create info structs complete and well-structured?

### Object Lifecycle & Ownership
- Who owns what? Are ownership relationships clear from the code?
- Is raw `new`/`delete` used where smart pointers or RAII would be better?
- Can constructor failures leave objects in a zombie (half-initialized) state?
- Are singletons used appropriately, or do they create hidden coupling and untestability?
- Are arbitrary single-instance limits enforced where multi-instance should be supported?

### Handle System Design
- Is the handle-based resource management pattern sound?
- Is the reference counting logic correct and complete (construct, copy-construct, operator=, destruct)?
- Is the generation counter functional, or is it dead code?
- Does the underlying storage (e.g., vector + erase) preserve index stability?

### Render Graph Architecture
- Is the render graph class a god class? Should it be decomposed?
- Is the "declare once, execute many" pattern supported, or are passes re-registered every frame?
- Are render pass combination / subpass merging implemented?
- Is cross-queue synchronization handled?
- Are resource limits (e.g., max registered resources) reasonable?
- Are there O(n²) lookup patterns that should be O(1)?

### Modularity & Coupling
- Are subsystems tightly coupled where they shouldn't be?
- Are there circular dependencies between modules?
- Is the dependency direction correct (api → core → platform)?
- Can a subsystem be modified without rippling changes through the codebase?

### Error Handling Strategy
- Is there a consistent error handling pattern (status codes vs exceptions vs asserts)?
- Do `TODO()` or `ASSERT` macros cause silent failures in release builds?
- Are error paths actually reachable and tested?

### Scalability & Extensibility
- How easy is it to add a new backend (e.g., D3D12, Metal)?
- How easy is it to add a new resource type?
- Are hardcoded values (pool sizes, limits, queue counts) configurable?
- Is the library designed for multi-window / multi-device scenarios?

### Code Organization
- Are files appropriately sized, or are there god files (1000+ lines)?
- Is functionality logically grouped into files and directories?
- Are there dead code paths or unused abstractions?
- Is naming consistent across the codebase?

## 3. Cross-reference subsystems

After analyzing each subsystem individually, check for inter-subsystem architectural issues:

- Does the handle system design support the usage patterns the render graph and device context require?
- Does the render graph's per-frame teardown align with the handle system's lifetime management?
- Are the caching strategies (pipeline, framebuffer, render pass) architecturally sound, or do they create hidden coupling?
- Does the singleton pattern (CoreObjectManager, CoreVk, GlobalSettings) prevent proper dependency injection between subsystems?
- Is the public API boundary consistent, or do some public types require knowledge of internal implementation?

## 4. Document findings

For each architectural issue found, document:

- **ID**: Sequential identifier (e.g., ARCH-1, ARCH-2)
- **Severity**: Critical / High / Medium / Low
  - **Critical**: Fundamental design flaws that cause incorrect behavior or make the system unsafe to build on
  - **High**: Design flaws with significant runtime impact or that block future development
  - **Medium**: Architectural debt that increases maintenance cost and risk
  - **Low**: Polish, style, or minor design improvements
- **Subsystem**: Which subsystem(s) are affected
- **File and line number(s)** where relevant
- **Code snippet** showing the issue (if applicable)
- **Explanation** of what's wrong and why it matters architecturally
- **Impact** — how this affects users of the library, future development, and maintainability
- **Recommendation** — a concrete architectural change or refactoring approach to resolve the issue. This is **required** for every finding.

Group findings by severity (Critical first, then High, Medium, Low).

## 5. Summary

Provide a summary table with all findings, their severity, affected subsystem, and a brief description. Place it at the beginning of the file, immediately after the report header.

## 6. Final output reminder

**The review MUST be saved as a new file on disk.** Verify that the file was successfully written using the `write_to_file` tool before completing. Do not output the review content as a chat message — the file is the deliverable. If the file was not created, the workflow has failed.
