@agent-task-breakdown Execute the plan in $1 with parallel task execution

Requirements:
- Follow jsrt development guidelines strictly
- Run mandatory tests: make test && make wpt
- Ensure code formatting: make format
- Verify build integrity: make clean && make

@jsrt-code-reviewer Conduct comprehensive review focusing on:
- Memory safety: leak prevention, proper allocation/deallocation
- Standards compliance: Web Platform Tests compatibility
- Performance impact: runtime efficiency, resource usage
- Edge case handling: error conditions, boundary values
- Code quality: maintainability, readability
