# Status Update Guidelines

## Three-Level Status Tracking System

This plan uses a three-level status tracking system for clarity:

### Level 1: Document-Level Status (Task Metadata)

Shows overall plan status:
- 🟡 PLANNING - Initial planning and research phase
- 🔵 IN_PROGRESS - Actively working on implementation
- 🟢 COMPLETED - All tasks complete, validated, documented
- 🔴 BLOCKED - Blocked by critical issues

### Level 2: Phase-Level Status (Org-mode Properties)

Each phase tracks its own progress:
- :STATUS: (Same color codes as document level)
- :PROGRESS: X/Y (completed tasks / total tasks)
- :COMPLETION: N% (percentage complete)

### Level 3: Task-Level Status (Org-mode Keywords)

Individual task states using Org-mode TODO keywords:
- TODO - Not yet started
- IN-PROGRESS - Currently working on this
- BLOCKED - Cannot proceed due to dependencies/issues
- DONE - Task completed successfully
- CANCELLED - Task no longer needed

## Update Discipline

### MANDATORY Updates

Update the plan document IMMEDIATELY when:
1. Task starts → Change TODO to IN-PROGRESS, log start time
2. Task completes → Change to DONE, update phase progress
3. Task blocked → Change to BLOCKED, document issue in History
4. Phase completes → Update phase status and completion %
5. Scope changes → Add/modify tasks, log in History

### Update Frequency

- After EVERY significant state change (not in batches)
- Real-time updates as work progresses
- Timestamp all changes using ISO 8601 format
