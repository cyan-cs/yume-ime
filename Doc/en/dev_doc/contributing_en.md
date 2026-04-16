# Contributing to Yume IME

Thank you for considering contributing to Yume IME! We welcome contributions of all kinds.

## Code of Conduct

This project follows the [Contributor Covenant 3.0 Code of Conduct](../../../CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## How to Contribute

### Reporting Bugs

1. Check if the bug already exists in [GitHub Issues](https://github.com/cyan-cs/yume-ime/issues)
2. If not, create a new issue with:
   - Clear title describing the problem
   - Step-by-step reproduction
   - Expected vs. actual behavior
   - OS version, Windows build number

### Suggesting Features

1. Open a GitHub Discussion or Issue
2. Describe the use case and desired behavior
3. Consider implementation complexity

### Submitting Code

#### Setup Development Environment

1. Clone the repository:
   ```powershell
   git clone https://github.com/cyan-cs/yume-ime.git
   cd yume-ime
   ```

2. Configure the environment:
   ```powershell
   .\scripts\build.ps1  # First run installs dependencies
   ```

3. Open in Visual Studio 2022:
   ```powershell
   .\build\YumeIME.sln
   ```

#### Coding Guidelines

Follow the [Coding Style Guide](./coding_style_en.md):

- **Language**: C++20
- **Indentation**: 4 spaces
- **Braces**: K&R style
- **Naming**: See guide for details (PascalCase for types, camelCase for functions)
- **Compilation**: Must compile with `/W4 /WE` (MSVC) flags enabled

#### Making Changes

1. Create a feature branch:
   ```powershell
   git checkout -b feature/my-feature
   ```

2. Make focused, atomic commits:
   ```powershell
   git add .
   git commit -m "Fix candidate selection when using space key"
   ```

3. Run tests locally:
   ```powershell
   cd .\build-tests-local
   cmake --build . --config Release
   ctest --output-on-failure
   ```

4. Push to your fork and create a Pull Request:
   ```powershell
   git push origin feature/my-feature
   ```

#### Pull Request Process

1. Ensure CI passes (all tests)
2. Provide a clear description of changes
3. Link related issues
4. The PR should focus on a single concern

#### Code Review

Maintainers will:
- Review code for style, logic, and performance
- Request changes if needed
- Merge when approved

## Development Workflow

### Project Structure

See [Architecture Guide](./architecture_en.md) for detailed system design.

Key directories:
- `src/ime/` - Core engine (tests in `tests/ime/`)
- `src/platform/tsf/` - Windows TextService Framework
- `src/ui/` - User interface components

### Building

```powershell
# Development build (with debug symbols)
.\scripts\build.ps1 -Configuration Debug

# Release build
.\scripts\build.ps1 -Configuration Release

# Build and test
cd build-tests-local
cmake --build . --config Release
ctest
```

### Testing

Run unit tests:
```powershell
# Using CTest
ctest --output-on-failure

# Or directly
.\build-tests-local\Release\ImeCoreSelfTests.exe
```

Add tests for new features in [tests/ime/](../../tests/ime/).

### Debugging

1. In Visual Studio, set breakpoints in core engine modules
2. Run debugger (F5)
3. View logs: `%APPDATA%\Yume IME\logs\debug.log`

## Documentation

When contributing:
- Update README if adding user-facing features
- Update architecture docs if changing system design
- Add code comments for complex logic
- Use Japanese or English consistently

## Areas for Contribution

- **Bug fixes**: See "good first issue" label
- **Performance**: Optimize dictionary lookup, rendering
- **Features**: New input modes, dictionary management, UI themes
- **Localization**: English documentation and translations
- **Testing**: More unit tests, integration tests

## License

By contributing, you agree to license your code under the [MIT License](../../LICENSE).

## Questions?

- Open a GitHub Discussion
- Check the [Architecture Guide](./architecture_en.md)
- Read [Environment Setup](./environment_en.md)

---

Thank you for contributing to Yume IME! 
