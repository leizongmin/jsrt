If $1 is empty, ask user to describe the task or feature they want to plan. Then ask for optional additional requirements.

Create breakdown plan for: $1

Planning Requirements:
- Break down into atomic, actionable tasks
- Include dependency mapping and parallel execution opportunities
- Provide testing strategy and success criteria
- Follow jsrt development guidelines strictly
- Consider mandatory tests: make test && make wpt
- Ensure code formatting: make format
- Verify build integrity: make clean && make
- Save the plan document to docs/plan/ directory with appropriate naming (e.g., node-MODULE-plan.md)

Additional requirements: $2

@agent-task-breakdown Create detailed task breakdown with parallel execution planning
