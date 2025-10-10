# Task Planning Documents

This directory contains detailed task breakdown and execution plans for jsrt features and improvements.

## Document Format

### File Extension: `.md` (Markdown)

All plan documents use the `.md` extension for maximum compatibility across tools and platforms.

### Content Format: Org-mode Syntax

While files use `.md` extension, the **content follows Org-mode syntax** for rich task management features:

- **Hierarchical task structure** with `*`, `**`, `***`, `****`
- **Status keywords**: `TODO`, `IN-PROGRESS`, `DONE`, `BLOCKED`, `CANCELLED`
- **Priority levels**: `[#A]`, `[#B]`, `[#C]`
- **Properties drawers**: `:PROPERTIES:...:END:`
- **Logbook entries**: `:LOGBOOK:...:END:`
- **Tags**: `:tag1:tag2:`
- **Timestamps** and state transitions

### Why This Hybrid Approach?

1. **Universal Compatibility**: `.md` files open in any editor and render on GitHub, GitLab, VS Code, etc.
2. **Progressive Enhancement**: Org-mode syntax is readable as plain text but provides rich features for Emacs users
3. **No Tooling Changes**: Existing workflows, scripts, and tools continue to work
4. **Best of Both Worlds**: Markdown compatibility + Org-mode power

## For Emacs Users

To get full Org-mode functionality in plan documents, add to your Emacs config:

```elisp
;; Enable Org-mode for plan documents
(add-to-list 'auto-mode-alist '("docs/plan/.*\\.md\\'" . org-mode))
```

This gives you:
- **Interactive task management**: Toggle TODO states with `C-c C-t`
- **Folding/unfolding**: Navigate with TAB and S-TAB
- **Timestamps**: Insert with `C-c .`
- **Agenda integration**: Track tasks across all plan files
- **Export**: Generate HTML, PDF, or other formats
- **Column view**: View properties in table format
- **Clocking**: Track time spent on tasks

## For Non-Emacs Users

The Org-mode syntax is designed to be readable as plain text:

```org
* TODO [#A] Phase 1: Implementation :feature:
:PROPERTIES:
:CREATED: 2025-10-10
:PROGRESS: 5/10
:END:

** DONE [#A] Task 1.1: Setup
CLOSED: [2025-10-10]

** IN-PROGRESS [#A] Task 1.2: Core logic
```

Most modern editors (VS Code, GitHub, etc.) will render:
- Headers as different sizes
- Lists and checkboxes
- Tables correctly
- Properties as code blocks

## Document Naming Convention

```
docs/plan/<feature-name>-plan.md
```

Examples:
- `node-dgram-plan.md` - Main plan for dgram module
- `node-dgram-plan/implementation.md` - Sub-document for implementation details
- `url-parsing-fix-plan.md` - Plan for URL parsing bug fix
- `performance-optimization-plan.md` - Performance improvement plan

## Sub-documents

For large plans exceeding 1500 lines, create a subdirectory:

```
docs/plan/
├── node-dgram-plan.md          # Main plan (overview)
└── node-dgram-plan/            # Sub-documents directory
    ├── implementation.md       # Implementation details
    ├── api-reference.md        # API mappings
    └── testing.md              # Test strategy
```

Reference sub-documents with full paths: `@docs/plan/node-dgram-plan/implementation.md`

## Plan Document Structure

All plans follow this structure:

1. **Org-mode Header**: Document metadata (`#+TITLE`, `#+TODO`, etc.)
2. **Task Metadata**: Properties with status, progress, timestamps
3. **Task Analysis**: Requirements, success criteria, constraints
4. **Task Breakdown**: Hierarchical task structure with dependencies
5. **Execution Dashboard**: Current status and next steps
6. **History & Updates**: Logbook and change table

See `.claude/agents/task-breakdown.md` for the complete template.

## Version Control

- **Commit regularly**: After each significant milestone
- **Meaningful messages**: "Update dgram plan: Phase 1 completed (5/5 tasks)"
- **Track progress**: Git history shows plan evolution
- **Team visibility**: Everyone sees current status

## Questions?

See `.claude/agents/task-breakdown.md` for detailed guidelines and examples.
