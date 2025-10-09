If $1 is empty, ask user for plan file path (search and show available plans in docs/plan/). Then ask for optional additional requirements.

Execute plan in $1 with these requirements:
- Follow jsrt development guidelines strictly
- Run mandatory tests: make test && make wpt
- Ensure code formatting: make format
- Verify build integrity: make clean && make
- Update plan document with latest progress

Additional requirements: $2

@agent-task-breakdown Execute the plan with parallel task execution

@jsrt-code-reviewer Review focusing on: memory safety, standards compliance, performance, edge cases, code quality
