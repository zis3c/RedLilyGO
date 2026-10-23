// Lint the generated browser bundle. The UI source under ui/src/ is concatenated
// by build_ui.py into one script, so per-file linting would report false
// redeclarations and unused cross-file globals. Lint the assembled bundle
// instead; run `python build_ui.py` first.
export default [
  {
    files: ['content/assets/js/ui.js'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'script',
    },
    rules: {
      'no-unused-vars': ['warn', { args: 'none', varsIgnorePattern: '^_' }],
      'no-redeclare': 'error',
      'no-dupe-keys': 'error',
      'no-dupe-args': 'error',
      'no-unreachable': 'error',
      'no-cond-assign': ['error', 'except-parens'],
      'no-constant-condition': ['error', { checkLoops: false }],
      'no-func-assign': 'error',
      'valid-typeof': 'error',
      'use-isnan': 'error',
      'no-unsafe-negation': 'error',
    },
  },
];
