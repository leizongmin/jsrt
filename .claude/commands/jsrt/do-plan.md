If $1 is empty, ask user for plan file path (search and show available plans in docs/plan/). Then ask for optional additional requirements.

Execute plan in $1 with these requirements:
- Follow jsrt development guidelines strictly
- Run mandatory tests: make test && make wpt
- Ensure code formatting: make format
- Verify build integrity: make clean && make
- **Update plan document with latest progress after each phase/milestone**
  - Mark completed tasks with checkmarks (✓)
  - Update progress percentages
  - Document any issues or deviations
  - Commit plan updates immediately after significant progress

Additional requirements: $2

@agent-task-breakdown Execute the plan with parallel task execution

@jsrt-code-reviewer Review focusing on: memory safety, standards compliance, performance, edge cases, code quality

---
**Note**: Keep plan document synchronized with actual progress. After completing each phase or significant milestone:
1. Update task status and progress metrics
2. Document completion status
3. Commit the updated plan to version control
4. This ensures team visibility and prevents progress loss
