# Popular npm Packages Compatibility Samples

This directory provides smoke-test style compatibility snippets for the top 100 most depended-upon npm packages (according to [anvaka's dependency graph](https://gist.github.com/anvaka/8e8fa57c7ee1350e3491)).  
Each snippet demonstrates that jsrt can `require()` the package and exercise a representative API.

## Contents

- `package.json` / `package-lock.json` – npm workspace preloaded with the 100 packages.
- `node_modules/` – installed dependencies (Angular packages are pinned to 7.2.x to satisfy `@angular/http`).
- `generate_examples.js` – helper script that regenerates the sample files.
- `test_<package>.js` – individual usage samples, one per package (e.g. `test_lodash.js`, `test_express.js`).

The samples intentionally keep side-effects minimal. Some scripts make outbound HTTP requests (`request`, `axios`, `node-fetch`, etc.) or attempt local service connections (`mongodb`, `redis`); these are noted in the console output and are safe to ignore when the corresponding service is unavailable.

## Usage

1. From this directory run `npm install` (already executed during initialization).
2. Execute an individual sample with `node test_<package>.js`.
3. Use `node generate_examples.js` after adding/removing packages to refresh the sample files.

Feel free to extend the examples, but keep them focused on the "core" behavior that validates jsrt compatibility.
