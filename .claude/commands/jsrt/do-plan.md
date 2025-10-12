If $1 is empty, ask user for plan file path (search and show available plans in docs/plan/). Then ask for optional additional requirements.

Execute plan in $1 with these requirements:
- Follow jsrt development guidelines strictly
- Run mandatory tests: make test && make wpt
- Ensure code formatting: make format
- Verify build integrity: make clean && make
- **Update plan document with latest progress after each phase/milestone**
  - **CRITICAL**: Update ALL THREE LEVELS of status tracking:
    1. Phase header: TODO → DONE, add COMPLETED timestamp, update PROGRESS/COMPLETION
    2. ALL Task headers in that phase: TODO → DONE, add COMPLETED timestamps
    3. ALL Subtask checkboxes in those tasks: [ ] → [X]
  - Update progress percentages at all levels
  - Document any issues or deviations
  - Commit plan updates immediately after significant progress

Additional requirements: $2

@agent-task-breakdown Execute the plan with parallel task execution

@jsrt-code-reviewer Review focusing on: memory safety, standards compliance, performance, edge cases, code quality

---
**Note**: Keep plan document synchronized with actual progress. After completing each phase or significant milestone:

**Three-Level Status Update Checklist** (DO NOT skip any level):
1. ✅ **Phase Level**: Update phase header status (TODO → DONE), add COMPLETED timestamp, verify PROGRESS field
2. ✅ **Task Level**: Update ALL task headers within that phase (TODO → DONE), add COMPLETED timestamps to each
3. ✅ **Subtask Level**: Check ALL subtask boxes within those tasks ([ ] → [X])
4. ✅ **Verification**: Scan the phase visually to ensure no TODO/unchecked items remain
5. ✅ **Document**: Note any issues or deviations
6. ✅ **Commit**: Save the updated plan to version control immediately

**Common mistake**: Only updating Phase status while forgetting Tasks and Subtasks. This creates documentation inconsistency!
