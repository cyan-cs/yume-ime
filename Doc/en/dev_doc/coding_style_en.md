# Yume - IME

## 1. Indentation & Braces
- Indent using **4 spaces**
- Use K&R style braces (opening brace at the end of function/control statement line)

## 2. Naming Conventions
- **Type names**: `PascalCase`
- **Enums**: Use `enum class`
- **Functions/Methods**: `camelCase`
- **Member variables**: `camelCase`
- **Constants**: `PascalCase` with `k` prefix
- **Type aliases**: Use `using`
- **Namespaces**: Multi-level, e.g., `yume::...`

## 3. Safety & Guards
- Manage state transitions and settings using **enum-based structures**
- Perform thorough boundary checks (e.g., `sanitizeSelection()`, `sanitizeSelectedSegmentIndex()`)
- On error, revert to a safe state
- Use rollback or session reset to maintain internal state integrity
- File saving should be **atomic with error_code checking**

## 4. Resource Management
- Manage COM resources with `Microsoft::WRL::ComPtr`
- Manage Win32/GDI handles via RAII (`UniqueGdiObject`, `UniqueWindowHandle`)
- Use `std::optional` for optional values

## 5. Code Structure & Headers
- Use `#pragma once` in header files
- Include order: project headers → standard/OS headers
- Keep small internal helpers in an anonymous namespace
- Split files by responsibility, adhering to DRY, KISS, and SOLID principles