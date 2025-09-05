#!/usr/bin/env python3
"""
WPT runner for jsrt - Run Web Platform Tests for WinterCG Minimum Common API
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# WinterCG Minimum Common API test categories
# Based on: https://wintercg.org/work/minimum-common-api/
WINTERCG_TESTS = {
    'console': [
        'console/console-is-a-namespace.any.js', 
        'console/console-tests-historical.any.js', 
        'console/console-log-large-array.any.js'
    ],
    'encoding': [
        'encoding/api-basics.any.js',
        'encoding/textdecoder-fatal.any.js',
        'encoding/textdecoder-labels.any.js',
        'encoding/textencoder-constructor-non-utf.any.js',
        'encoding/textencoder-utf16-surrogates.any.js'
    ],
    'hr-time': [
        'hr-time/monotonic-clock.any.js'
    ],
    'performance': [
        'hr-time/monotonic-clock.any.js'
    ],
    'url': [
        'url/url-constructor.any.js', 
        'url/url-origin.any.js', 
        'url/url-tojson.any.js',
        'url/urlsearchparams-constructor.any.js',
        'url/urlsearchparams-get.any.js',
        'url/urlsearchparams-getall.any.js',
        'url/urlsearchparams-has.any.js',
        'url/urlsearchparams-set.any.js',
        'url/urlsearchparams-size.any.js',
        'url/urlsearchparams-stringifier.any.js',
    ],
    'fetch-api': [
        'fetch/api/headers/headers-basic.any.js',
        'fetch/api/request/request-consume-empty.any.js',
        'fetch/api/response/response-consume-empty.any.js'
    ],
    'webcrypto': [
        'WebCryptoAPI/getRandomValues.any.js'
    ],
    'base64': [
        'html/webappapis/atob/base64.any.js'
    ],
    'timers': [
        'html/webappapis/timers/cleartimeout-clearinterval.any.js',
        'html/webappapis/timers/missing-timeout-setinterval.any.js',
        'html/webappapis/timers/negative-settimeout.any.js',
        'html/webappapis/timers/negative-setinterval.any.js'
    ],
    'streams': [
        'streams/readable-streams/default-reader.any.js',
        'streams/writable-streams/constructor.any.js'
    ],
    'abort': [
        'dom/abort/abort-signal-any.any.js'
    ]
}

class JSRTTestRunner:
    def __init__(self, jsrt_path: str, wpt_path: str, verbose: bool = False):
        self.jsrt_path = Path(jsrt_path)
        self.wpt_path = Path(wpt_path)
        self.verbose = verbose
        self.results = {
            'passed': 0,
            'failed': 0,
            'skipped': 0,
            'tests': []
        }
    
    def log(self, message: str):
        if self.verbose:
            print(f"[WPT] {message}")
    
    def find_test_files(self, patterns: List[str]) -> List[Path]:
        """Find test files matching the given patterns."""
        test_files = []
        for pattern in patterns:
            # Handle both exact file paths and glob patterns
            if '*' in pattern:
                # Simple glob implementation
                parts = pattern.split('*')
                if len(parts) == 2:
                    dir_part, file_part = parts
                    test_dir = self.wpt_path / dir_part.rstrip('/')
                    if test_dir.exists():
                        for file in test_dir.iterdir():
                            if file.name.endswith(file_part) and file.is_file():
                                test_files.append(file)
            else:
                # Exact file path
                test_file = self.wpt_path / pattern
                if test_file.exists():
                    test_files.append(test_file)
                else:
                    self.log(f"Warning: Test file not found: {pattern}")
        
        return sorted(test_files)  # Sort for consistent order
    
    def is_test_supported(self, test_path: Path) -> Tuple[bool, str]:
        """Check if a test is supported by jsrt."""
        try:
            with open(test_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            # Skip tests that require DOM or browser-specific features that jsrt doesn't have
            unsupported_features = [
                'document.',
                'window.',  # Only block window.something, not just "window" string
                # 'DOMException',  # Now supported!
                'WorkerGlobalScope', 
                'SharedWorker',
                'Worker(',
                'importScripts',
                'navigator.',
                'location.',
                'XMLHttpRequest',
                'EventTarget',
                'addEventListener',
                'performance.mark',  # jsrt may not have full Performance API
                'performance.measure',
                'getComputedStyle',
            ]
            
            for feature in unsupported_features:
                if feature in content:
                    return False, f"Uses unsupported feature: {feature}"
            
            # Skip tests that explicitly require window global
            if 'global=window' in content and 'dedicatedworker' not in content and 'shadowrealm' not in content:
                return False, "Requires window global only"
                    
            return True, ""
            
        except Exception as e:
            return False, f"Error reading file: {e}"
    
    def parse_meta_directives(self, test_content: str) -> Dict[str, List[str]]:
        """Parse META directives from test content."""
        meta_directives = {'script': [], 'title': [], 'timeout': []}
        
        for line in test_content.split('\n')[:20]:  # Only check first 20 lines
            line = line.strip()
            if line.startswith('// META:'):
                meta = line[8:].strip()
                if '=' in meta:
                    key, value = meta.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    if key in meta_directives:
                        meta_directives[key].append(value)
        
        return meta_directives
    
    def load_resource_file(self, resource_path: str, test_dir: Path) -> str:
        """Load a WPT resource file."""
        # Resource paths in META directives are relative to the test directory
        # First try relative to test directory
        full_path = test_dir / resource_path
        
        # If not found, try relative to WPT root (some tests may use absolute paths from WPT root)
        if not full_path.exists():
            full_path = self.wpt_path / resource_path
            
        if full_path.exists():
            try:
                with open(full_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    self.log(f"Loaded resource: {resource_path} from {full_path}")
                    return content
            except Exception as e:
                self.log(f"Warning: Could not read resource file {resource_path}: {e}")
                return f"// Error loading resource: {resource_path} - {e}\n"
        else:
            self.log(f"Warning: Resource file not found: {resource_path} (tried {test_dir / resource_path} and {self.wpt_path / resource_path})")
            return f"// Resource file not found: {resource_path}\n"
    
    def create_test_wrapper(self, test_path: Path) -> Path:
        """Create a wrapper script that includes the WPT test harness and the test."""
        wrapper_dir = Path('/tmp/jsrt-wpt-tests')
        wrapper_dir.mkdir(exist_ok=True)
        
        # Create a unique wrapper file name
        wrapper_name = f"wrapper_{test_path.stem}_{hash(str(test_path)) % 10000}.js"
        wrapper_path = wrapper_dir / wrapper_name
        
        # Read the original test
        with open(test_path, 'r', encoding='utf-8') as f:
            test_content = f.read()
        
        # Parse META directives
        meta_directives = self.parse_meta_directives(test_content)
        
        # Load required resource scripts
        resource_scripts = []
        for script_path in meta_directives['script']:
            resource_content = self.load_resource_file(script_path, test_path.parent)
            resource_scripts.append(f"// === Resource: {script_path} ===\n{resource_content}\n")
        
        # Create wrapper script
        harness_path = Path(__file__).parent / 'wpt-testharness.js'
        wrapper_content = f'''
// WPT Test Harness for jsrt
{harness_path.read_text()}

{''.join(resource_scripts)}
// Original test content:
{test_content}
'''
        
        with open(wrapper_path, 'w', encoding='utf-8') as f:
            f.write(wrapper_content)
            
        return wrapper_path

    def run_single_test(self, test_path: Path) -> Dict:
        """Run a single test file with jsrt."""
        self.log(f"Running test: {test_path.relative_to(self.wpt_path)}")
        
        # Check if test is supported
        supported, reason = self.is_test_supported(test_path)
        if not supported:
            self.log(f"Skipping {test_path.name}: {reason}")
            self.results['skipped'] += 1
            return {
                'test': str(test_path.relative_to(self.wpt_path)),
                'status': 'SKIP',
                'reason': reason
            }
        
        try:
            # Create wrapper with test harness
            wrapper_path = self.create_test_wrapper(test_path)
            
            # Run jsrt with the wrapper file
            cmd = [str(self.jsrt_path), str(wrapper_path)]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                cwd=str(self.wpt_path)  # Set working directory to wpt for relative imports
            )
            
            # Clean up wrapper
            try:
                wrapper_path.unlink()
            except:
                pass
            
            # Determine test result based on output
            if result.returncode == 0:
                # Check if any tests failed in the output
                if '❌' in result.stdout:
                    status = 'FAIL'
                    self.results['failed'] += 1
                else:
                    status = 'PASS'
                    self.results['passed'] += 1
            else:
                status = 'FAIL'
                self.results['failed'] += 1
                
            return {
                'test': str(test_path.relative_to(self.wpt_path)),
                'status': status,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'returncode': result.returncode
            }
            
        except subprocess.TimeoutExpired:
            self.results['failed'] += 1
            return {
                'test': str(test_path.relative_to(self.wpt_path)),
                'status': 'TIMEOUT',
                'reason': 'Test timed out after 30 seconds'
            }
        except Exception as e:
            self.results['failed'] += 1
            return {
                'test': str(test_path.relative_to(self.wpt_path)),
                'status': 'ERROR',
                'reason': str(e)
            }
    
    def run_tests(self, categories: Optional[List[str]] = None) -> Dict:
        """Run WPT tests for specified categories."""
        if categories is None:
            categories = list(WINTERCG_TESTS.keys())
        
        print(f"Running WPT tests for categories: {', '.join(categories)}")
        print(f"Using jsrt: {self.jsrt_path}")
        print(f"WPT directory: {self.wpt_path}")
        print()
        
        for category in categories:
            if category not in WINTERCG_TESTS:
                print(f"Warning: Unknown category '{category}'")
                continue
                
            patterns = WINTERCG_TESTS[category]
            if not patterns:
                print(f"No test patterns defined for category '{category}'")
                continue
                
            print(f"=== Running {category} tests ===")
            
            test_files = self.find_test_files(patterns)
            if not test_files:
                print(f"No test files found for category '{category}'")
                continue
                
            for test_file in test_files:
                result = self.run_single_test(test_file)
                self.results['tests'].append(result)
                
                status_symbol = {
                    'PASS': '✓',
                    'FAIL': '✗',
                    'SKIP': '○',
                    'TIMEOUT': '⚠',
                    'ERROR': '✗'
                }.get(result['status'], '?')
                
                print(f"  {status_symbol} {result['test']} - {result['status']}")
                
                if result['status'] in ['FAIL', 'ERROR', 'TIMEOUT'] and 'reason' not in result:
                    if result.get('stderr'):
                        print(f"    Error: {result['stderr'][:200]}...")
                    elif result.get('stdout'):
                        # Show stdout for debugging
                        lines = result['stdout'].split('\n')
                        for line in lines:
                            if line.strip() and '❌' in line:
                                print(f"    {line}")
                                break
                elif 'reason' in result:
                    print(f"    {result['reason']}")
                
            print()
        
        return self.results
    
    def print_summary(self):
        """Print test results summary."""
        total = self.results['passed'] + self.results['failed'] + self.results['skipped']
        
        print("=== Test Summary ===")
        print(f"Total tests: {total}")
        print(f"Passed: {self.results['passed']}")
        print(f"Failed: {self.results['failed']}")
        print(f"Skipped: {self.results['skipped']}")
        
        if total > 0:
            pass_rate = (self.results['passed'] / total) * 100
            print(f"Pass rate: {pass_rate:.1f}%")


def main():
    parser = argparse.ArgumentParser(description='Run WPT tests for jsrt')
    parser.add_argument('--jsrt', default='./target/release/jsrt',
                        help='Path to jsrt executable (default: ./target/release/jsrt)')
    parser.add_argument('--wpt-dir', default='./wpt',
                        help='Path to WPT directory (default: ./wpt)')
    parser.add_argument('--category', action='append',
                        choices=list(WINTERCG_TESTS.keys()),
                        help='Test categories to run (can be specified multiple times)')
    parser.add_argument('--list-categories', action='store_true',
                        help='List available test categories')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose output')
    parser.add_argument('--output', '-o',
                        help='Output results to JSON file')
    
    args = parser.parse_args()
    
    if args.list_categories:
        print("Available test categories:")
        for category, patterns in WINTERCG_TESTS.items():
            print(f"  {category}: {patterns if patterns else 'No patterns defined'}")
        return 0
    
    # Validate jsrt executable
    jsrt_path = Path(args.jsrt)
    if not jsrt_path.exists():
        print(f"Error: jsrt executable not found at {jsrt_path}")
        print("Please build jsrt first with 'make jsrt'")
        return 1
    
    # Validate WPT directory
    wpt_path = Path(args.wpt_dir)
    if not wpt_path.exists():
        print(f"Error: WPT directory not found at {wpt_path}")
        print("Please run 'make wpt-download' or 'make wpt' to download WPT tests")
        return 1
    
    # Run tests
    runner = JSRTTestRunner(jsrt_path, wpt_path, args.verbose)
    results = runner.run_tests(args.category)
    runner.print_summary()
    
    # Output results to file if requested
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"\nResults written to {args.output}")
    
    # Exit with error code if any tests failed
    return 1 if results['failed'] > 0 else 0


if __name__ == '__main__':
    sys.exit(main())