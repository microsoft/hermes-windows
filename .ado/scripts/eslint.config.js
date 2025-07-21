import js from "@eslint/js";

export default [
  js.configs.recommended,
  {
    files: ["build.js", "setVersionNumber.js"], // Only target the actual script files
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: "module",
      globals: {
        console: "readonly",
        process: "readonly",
        Buffer: "readonly",
        __dirname: "readonly",
        __filename: "readonly",
        global: "readonly",
      },
    },
    rules: {
      // Code quality rules
      "no-unused-vars": ["error", { argsIgnorePattern: "^_" }],
      "no-console": "off", // Allow console in build scripts
      "prefer-const": "error",
      "no-var": "error",
      eqeqeq: ["error", "always"],
      curly: ["error", "all"],

      // Style rules - only the important ones to avoid Prettier conflicts
      semi: ["error", "always"],
      quotes: ["error", "double", { avoidEscape: true }],
      "comma-dangle": ["error", "always-multiline"],

      // Best practices
      "no-implicit-globals": "error",
      "no-throw-literal": "error",
      "prefer-template": "error",
      "object-shorthand": "error",
      "arrow-spacing": "error",
      "no-duplicate-imports": "error",
      "no-useless-escape": "error",
    },
  },
];
