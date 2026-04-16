# Architecture Guide - Yume IME

## System Overview

Yume IME is a Japanese input method editor (IME) that runs on Windows. It automatically generates katakana and romaji suggestions from hiragana input.

```
┌─────────────────────────────────────────┐
│         Windows TextService Framework   │
│              (TSF/CTFIME)               │
└──────────────────┬──────────────────────┘
                   │ IPC/COM Interface
┌──────────────────▼──────────────────────┐
│          IME Core Engine                │
│  • Composition (Character Conversion)   │
│  • Converter (Candidate Generation)     │
│  • Dictionary (Lexicon)                 │
└──────────────────┬──────────────────────┘
                   │
        ┌──────────┼──────────┐
        │          │          │
    ┌───▼──┐  ┌───▼──┐  ┌───▼──┐
    │Buffer│  │State │  │Candidate
    │      │  │      │  │Window
    └──────┘  └──────┘  └──────┘
```

## Directory Structure

### `src/ime/` - Core Engine
Platform-independent components compiled into ImeCore.lib. Implemented in C++20.

- **engine/**: Main IME engine
  - `ImeEngine` - Session management, input processing main
  - `ime_engine_input.cpp` - Key input handling
  - `ime_engine_segments.cpp` - Character segment management

- **composition/**: Text composition and conversion processing
  - `Buffer` - Input buffer (hiragana)
  - `Converter` - Candidate generation engine
  - `Normalizer` - Normalization processing
  - `RomajiTable` - Romaji-kana correspondence

- **dictionary/**: Dictionary management
  - `Dictionary` - Dictionary interface
  - `default_db` - Default dictionary data
  - `user_db` - User custom dictionary
  - `trie` - Trie tree for fast lookup
  - `black_db` - Blocklist

- **candidate/**: Candidate management
  - `Candidate` - Candidate data structure
  - `CandidateList` - List management

- **state/**: State machine
  - `ImeState` - Input state (hiragana, Latin, etc.)
  - `ImeSession` - Session state management

- **input/**: Input events
  - `KeyEvent` - Key input event type definitions

### `src/platform/tsf/` - Windows TextService Framework Integration
DLL implementation for communicating with OS via TSF.

- `DllMain.cpp` - DLL initialization
- `text_service_sinks.cpp` - Input source registration
- `text_service_config.cpp` - Configuration management
- `candidate_window.cpp` - Candidate window UI integration

### `src/ui/` - User Interface
Components for desktop UI.

- **candidate_window/**: Candidate window
  - Direct2D/DirectWrite rendering
  - Theme support (dark mode)

### `src/utils/` - Utilities
- **logger.hpp/cpp** - Logging (JSON format)
- **windows_theme.hpp/cpp** - Windows dynamic theme support
- **com_ptr.hpp** - COM management utilities
- **win_raii.hpp** - Win32 resource RAII

## Processing Flow

### Basic Input Processing Flow

```
User Input
    │
    ▼
[KeyEvent] ─────────────────────────┐
                                    │
                      [IMEEngine]   │
                           │        │
[Composition]◄─────────────┼────────┤
(Hiragana buffer)           │        │
                           │        │
[Converter] ◄──────────────┼────────┤
(Candidate generation)      │        │
    │                      │        │
    ├─ [Dictionary]◄───────┼────────┤
    │ (Dictionary lookup)   │        │
    │                      │ Session
    ├─ [Candidate List]    │ State
    │ (Candidate list)     │        │
    │                      │        │
    ▼                      │        │
[Display]◄─────────────────┼────────┘
(Display)

    ▼ 
[TSF] ──→ [Windows Input Method]
```

### State Transition

```
Editing State
    ▲
    │ (Key: Space, Enter)
    │
Selecting State ──────────┐
    ▲                     │
    │                     │(Key: Escape)
    │                     │
    │◄────────────────────┘
    │
    ├─ (Key: Confirm) ──→ Committed ──→ Reset
    │
    └─ (Key: Cancel) ──→ Reset
```

## Dictionary Lookup Data Flow

1. User input: "わたし"
2. CompositionBuffer: "わたし"
3. Converter.suggest():
   - Dictionary.lookup("わたし")
   - Fast lookup via Trie tree
   - Generate candidates: [渡し, 私, 綿, ...]
4. Store in CandidateList
5. Display rendered output

## Key Input Event Processing

```cpp
void ImeEngine::processKeyEvent(const KeyEvent& key) {
    switch(key.virtualKey) {
        case VK_BACK:       // Delete
        case VK_SPACE:      // Candidate selection start
        case VK_RETURN:     // Confirm
        case VK_ESCAPE:     // Cancel
        case 'A'-'Z', ...  // Character input
    }
    
    // State transition and UI update
    updateComposition();
    updateCandidateList();
}
```

## Threading and Synchronization Model

- **Single-threaded**: IME engine runs on main thread
- **Asynchronous dictionary loading**: User dictionary can be loaded in background
- **COM STA**: TSF runs in Single Threaded Apartment (STA)

## Resource Management

- **COM references**: Managed automatically with `Microsoft::WRL::ComPtr<>`
- **Memory**: Managed automatically with `std::unique_ptr<>`, `std::shared_ptr<>`
- **Atomic file operations**: File locking used during dictionary saves

## Performance Optimizations

1. **Trie tree lookup**: O(m) - where m is input string length
2. **Caching**: Recent query results can be cached in memory
3. **LTO (Link-Time Optimization)**: Enabled in Release builds
