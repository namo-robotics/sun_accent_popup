# Agent Development Guide

A file for guiding coding agents.

## Commands

- Never use git commands except readonly ones like `git status` and `git diff`.
- Run commands from this repository's root. Keep temporary files in `tmp/` and generated artifacts in `build/`.
- Download published dependency packages with `./scripts/install-dependencies.sh`. Do not clone or build dependency subrepositories.
- Build and test the application: `./build.sh`. Build only: `./build.sh --build`. Tests: `./build.sh --test`. Build tested Debian and Snap packages: `./build.sh --package`.
- Implement build tooling in Sun under `src/tools/`; declare products in `sun-config.json` and select them by name.
- Keep application source in `src/`, tests in `tests/`, and desktop assets in `packaging/`.
- Format only first-party paths: `sun fmt src/main.sun src/state.sun src/view.sun src/platform src/tools tests`.
- Format only first-party sources; never modify downloaded dependency artifacts.
- `deps.repos` imports no repositories. Dependency bundles, native archives, tools, and license notices are stored under `build/dependencies/`.
- Target Ubuntu/GNOME through IBus. Keep input policy and tests in Sun; native IBus object registration, timers, and protocol glue belong in `native/`.
- Test the input method through an isolated D-Bus session, never by changing the developer’s active input source.
- Native GTK ownership and callbacks belong in sun_gobject; UI components use sun_ui's public API and keep application state explicitly owned.
- Builds default to downloaded release bundles. Explicit `SUN_UI_BUILD` and `SUN_GOBJECT_BUILD` overrides select compatible prebuilt bundles; `SUN_UI_SOURCE` and `SUN_GOBJECT_SOURCE` override license locations.

## Sun Language Conventions

- Implement as much project functionality, tooling, and test logic as possible in Sun rather than other scripts or languages. Use other languages only where Sun cannot reasonably provide the required functionality.

- Sun does not allow implicit copies; make ownership transfers and any required copies explicit.
- Sun does not have macros; use ordinary functions for reusable logic.
- Keep allocations explicit and avoid `unsafe` blocks wherever possible.

## Code Comments (all code, including .sun files)
- Write concise comments in plain English for a general audience of software engineers.
- Every namespace, function, and type definition must have a documentation comment describing what it does or what it is for. This applies to all first-party source code, including C++ and `.sun` files, regardless of visibility.
- Use `/** ... */` blocks in C++, Sun, and JavaScript/TypeScript. Use the language's native equivalent elsewhere, such as Python docstrings or a shell comment block.
- Cover private and internal helpers, methods, constructors, destructors, anonymous namespaces, classes, structs, interfaces, enums, and type aliases, as well as public APIs. Document modules too.
- Put the comment immediately before the declaration or definition it documents. For a function declared in a header and implemented separately, document the declaration; add implementation comments when they explain details beyond that contract.
- Explain the purpose or behavior instead of merely restating the symbol name. Mention ownership, side effects, constraints, or failure behavior when needed to understand correct use.
- Preserve useful existing documentation and keep it accurate when changing code. Do not edit generated code or third-party dependencies to add comments.
- Do not hard-code numeric values that are subject to change in comments.

## Commit messages
- Subject: `<scope>: <description>` (scope = subsystem/package/area; imperative; no `feat`/`fix` types).
- Body: blank line, then one concise bullet per key change if not already captured in the subject.
- Do not add AI attribution to commits or PRs (no Co-Authored-By, Generated-with, tool names, or session links).

## Planning

- Write plans in plain english, avoiding unnecessary jargon and abreviations.
- Use simple, concrete examples in explanations when it makes sense to do so.

## Issues and PR Guidelines

- Never create PRs or issues unless specifically asked by the user.

## Roadmap

- Roadmap items that are entirely completed should be removed from the roadmap doc.
