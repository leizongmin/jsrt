/**
 * TypeScript definitions for node:module hook system
 * These types provide IntelliSense and type checking for module hook APIs
 */

// Import the base Module type definitions
/// <reference path="./module.d.ts" />

declare module 'node:module' {
  // ============================================================================
  // Hook Context Types
  // ============================================================================

  /**
   * Context object passed to resolve hooks
   */
  interface ResolveContext {
    /** The original module specifier */
    specifier: string;

    /** Path of the parent module (if applicable) */
    parentPath?: string;

    /** Resolved URL (if available) */
    resolvedUrl?: string;

    /** Whether this is the main module */
    isMain: boolean;

    /** Resolution conditions (e.g., ['node', 'import']) */
    conditions?: string[];
  }

  /**
   * Context object passed to load hooks
   */
  interface LoadContext {
    /** Expected module format */
    format?: string;

    /** Load conditions */
    conditions?: string[];

    /** Import attributes (for ESM) */
    importAttributes?: Record<string, any>;
  }

  /**
   * Result object returned by resolve hooks
   */
  interface ResolveResult {
    /** Resolved module URL */
    url: string;

    /** Module format (optional) */
    format?: string;

    /** Whether to skip remaining hooks */
    shortCircuit?: boolean;
  }

  /**
   * Result object returned by load hooks
   */
  interface LoadResult {
    /** Module format */
    format: string;

    /** Module source code (string, ArrayBuffer, or Uint8Array) */
    source: string | ArrayBuffer | Uint8Array;

    /** Whether to skip remaining hooks */
    shortCircuit?: boolean;
  }

  /**
   * Hook registration options
   */
  interface ModuleHooksOptions {
    /** Resolve hook function - called during module resolution */
    resolve?: (
      specifier: string,
      context: ResolveContext,
      nextResolve: (specifier?: string) => ResolveResult | string | null
    ) => ResolveResult | string | null | void;

    /** Load hook function - called during module loading */
    load?: (
      url: string,
      context: LoadContext,
      nextLoad: (url?: string) => LoadResult | string | null
    ) => LoadResult | string | null | void | Promise<LoadResult | string | null | void>;
  }

  /**
   * Hook registration handle (for future unregistration API)
   * Currently just undefined, but preserved for future compatibility
   */
  type HookRegistrationHandle = void;

  // ============================================================================
  // Extended Module API
  // ============================================================================

  namespace Module {
    // Existing Node.js Module properties and methods...

    /**
     * Register module resolution and loading hooks
     *
     * @param options Hook configuration with resolve and/or load functions
     * @returns Registration handle (for future unregistration API)
     *
     * @example
     * ```typescript
     * registerHooks({
     *   resolve(specifier, context, nextResolve) {
     *     if (specifier.startsWith('custom:')) {
     *       return {
     *         url: `./src/${specifier.replace('custom:', '')}`,
     *         format: 'commonjs',
     *         shortCircuit: true
     *       };
     *     }
     *     return nextResolve();
     *   },
     *   load(url, context, nextLoad) {
     *     if (url.includes('transform-me')) {
     *       return {
     *         format: 'commonjs',
     *         source: transformSource(nextLoad()?.source || ''),
     *         shortCircuit: true
     *       };
     *     }
     *     return nextLoad();
     *   }
     * });
     * ```
     */
    function registerHooks(options: ModuleHooksOptions): HookRegistrationHandle;

    // Also export as named export
    export { registerHooks, type ModuleHooksOptions, type ResolveContext, type LoadContext, type ResolveResult, type LoadResult };
  }

  export = Module;
}

// Hook context types for standalone imports
export type ResolveContext = import('node:module').ResolveContext;
export type LoadContext = import('node:module').LoadContext;
export type ResolveResult = import('node:module').ResolveResult;
export type LoadResult = import('node:module').LoadResult;
export type ModuleHooksOptions = import('node:module').ModuleHooksOptions;
