# GitHub Copilot Instructions for Hermes

## Always Reference
- Use `.copilot-rules.md` as the primary reference for all development work
- Follow Hermes-specific patterns and architectural guidelines
- Consider performance implications for all code suggestions

## Context Priority
1. **Hermes Architecture**: JavaScript engine optimized for React Native
2. **Performance Critical**: All code affects startup time and execution speed
3. **Cross-Platform**: Support Windows, macOS, Linux, iOS, Android
4. **Memory Safety**: Use proper GC-aware patterns and RAII

## Before Suggesting Code
- Check which folder you're working in (API/, lib/VM/, tools/, etc.)
- Apply folder-specific rules from `.copilot-rules.md`
- Consider the performance and memory implications
- Ensure cross-platform compatibility

## Common Patterns to Follow
- Use `Handle<>` for GC objects in VM code
- Use `CallResult<>` for error-prone operations
- Follow LLVM coding standards
- Include proper error handling and source locations
- Use FileCheck patterns for tests

## Testing Requirements
- Include appropriate test coverage
- Use `lit` for JavaScript tests with FileCheck
- Use `gtest` for C++ unit tests
- Consider cross-platform test execution
