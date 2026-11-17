# Windows CI cache and optional release implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement and verify this plan.

**Goal:** Reuse SDKs, compiled libraries and build outputs, retain the latest two caches per family, and optionally publish the current application version with its changelog.

**Architecture:** Keep the existing Qt cache key. Restore the complete build directory using a toolchain/source-content prefix, with a compatible older build as an incremental fallback. Compare compiled content before and after the run; save whenever it changes under a key including that content's hash. Cache PE inspection tools separately. A separate, explicitly enabled release job publishes the successful build with write permission isolated from the build job.

**Tech stack:** GitHub Actions, CMake FetchContent, Python standard library, PowerShell, GitHub CLI.

**Spec:** User's requested CI changes in this conversation; `publish_release` defaults to false.

## Constraints and defaults

- Keep Qt 6.8.3 / MSVC 2022 x64 Release and existing runtime checks.
- Cache the entire build directory, including FetchContent sources/builds, object files, static libraries, EXE and packaged output. Isolate incompatible compiler/SDK/Qt/CMake/Ninja combinations in the restore prefix.
- Matching source/toolchain builds reuse the compiled EXE; compatible older sources reconfigure and build incrementally. Cached dependency sources bypass downloads on both kinds of hit. Packaging runs again to refresh the changelog.
- Cache keys exclude commits, run IDs and dates; source-content and compiled-content hashes distinguish changed builds. Save full builds whenever compiled intermediates/outputs change, even if dependency versions do not change. Ignore log, archive and mtime-only changes. Only the default branch saves custom caches.
- After saving, retain the latest two caches in each managed family (Qt, full build, PE tools), across this workflow's refs. Delete older entries without an age threshold. Also run maintenance weekly.
- Release version comes from CMake, tag is `v<version>`, notes come from that version's `CHANGELOG.md` section, and the full changelog is included in the ZIP.
- Publish only when the manual boolean input is true, after a successful build. Reject an existing release or a tag pointing to another commit.

## Tasks

- [ ] Add behavioral tests for release metadata, byte-identical artifact promotion, duplicate/version protection, and two-generation cache retention/scope. Observe missing implementation failures, then implement the Python helpers.
- [ ] Extend the workflow with conditional full-build restore/save, compatible incremental reuse, PE tools reuse, maintenance, and a separate optional publishing job.
- [ ] Add the changelog to the package and document checkbox usage, cache lifecycle, and release version rules.
- [ ] Run Python tests, PowerShell parsing and actionlint; commit and push to main. The user explicitly requested no further review.
- [ ] Run CI with publishing disabled, then repeat to confirm exact hits and no duplicate cache uploads. Do not publish a Release during verification.

## Review focus

Cached build outputs must match their source/toolchain key; exact hits must skip uploads; cleanup must preserve the newest two entries per family and unrelated caches; release tags and changelog must match the built commit; push/PR/scheduled runs must never publish automatically.
