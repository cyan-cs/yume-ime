# Yume IME - アーキテクチャ

## システム概要

Yume IMEは、Windows上で動作する日本語入力メソッドエディタ（IME）です。ひらがな入力から自動的にカタカナやローマ字の候補を生成します。

```
┌─────────────────────────────────────────┐
│         Windows TextService Framework   │
│              (TSF/CTFIME)               │
└──────────────────┬──────────────────────┘
                   │ IPC/COM Interface
┌──────────────────▼──────────────────────┐
│          IME Core Engine                │
│  • Composition (文字変換)                │
│  • Converter (候補生成)                  │
│  • Dictionary (辞書)                    │
└──────────────────┬──────────────────────┘
                   │
        ┌──────────┼──────────┐
        │          │          │
    ┌───▼──┐  ┌───▼──┐  ┌───▼──┐
    │Buffer│  │State │  │Candidate
    │      │  │      │  │Window
    └──────┘  └──────┘  └──────┘
```

## ディレクトリ構造

### `src/ime/` - コアエンジン
ImeCore.libにビルドされるプラットフォーム非依存のコンポーネント。C++20で実装。

- **engine/**: メインのIMEエンジン
  - `ImeEngine` - セッション管理、入力処理のメイン
  - `ime_engine_input.cpp` - キー入力ハンドリング
  - `ime_engine_segments.cpp` - 文字セグメント管理

- **composition/**: テキスト構成・変換処理
  - `Buffer` - 入力バッファ（ひらがな）
  - `Converter` - 候補生成エンジン
  - `Normalizer` - 正規化処理
  - `RomajiTable` - ローマ字・かな対応

- **dictionary/**: 辞書管理
  - `Dictionary` - 辞書インターフェース
  - `default_db` - デフォルト辞書データ
  - `user_db` - ユーザーカスタム辞書
  - `trie` - Trie木による高速検索
  - `black_db` - ブロックリスト

- **candidate/**: 候補管理
  - `Candidate` - 候補データ構造
  - `CandidateList` - リスト管理

- **state/**: ステートマシン
  - `ImeState` - 入力状態（ひらがな、ラテン等）
  - `ImeSession` - セッション状態管理

- **input/**: 入力イベント
  - `KeyEvent` - キー入力イベント型定義

### `src/platform/tsf/` - Windows TextService Framework統合
TSF経由でOSと通信するDLL実装。

- `DllMain.cpp` - DLL初期化
- `text_service_sinks.cpp` - 入力ソース登録
- `text_service_config.cpp` - 設定管理
- `candidate_window.cpp` - 候補ウィンドウUI統合

### `src/ui/` - ユーザーインターフェース
デスクトップUI向けのコンポーネント。

- **candidate_window/**: 候補ウィンドウ
  - Direct2D/DirectWriteによるレンダリング
  - テーマ対応（ダークモード）

### `src/utils/` - ユーティリティ
- **logger.hpp/cpp** - ロギング（JSON形式）
- **windows_theme.hpp/cpp** - Windows動的テーマ対応
- **com_ptr.hpp** - COM管理ユーティリティ
- **win_raii.hpp** - Win32リソースのRAII

## 処理フロー

### 基本入力処理フロー

```
User Input
    │
    ▼
[KeyEvent] ─────────────────────────┐
                                    │
                      [IMEEngine]   │
                           │        │
[Composition]◄─────────────┼────────┤
(ひらがなバッファ)          │        │
                           │        │
[Converter] ◄──────────────┼────────┤
(候補生成)                  │        │
    │                      │        │
    ├─ [Dictionary]◄───────┼────────┤
    │ (辞書検索)            │        │
    │                      │ Session
    ├─ [Candidate List]    │ State
    │ (候補リスト)         │        │
    │                      │        │
    ▼                      │        │
[Display]◄─────────────────┼────────┘
(表示)

    ▼ 
[TSF] ──→ [Windows Input Method]
```

### ステートトランジション

```
Editing State
    ▲
    │ (キー: スペース、Enter)
    │
Selecting State ──────────┐
    ▲                     │
    │                     │(キー: Escape)
    │                     │
    │◄────────────────────┘
    │
    ├─ (キー:確定) ──→ Committed ──→ Reset
    │
    └─ (キー: キャンセル) ──→ Reset
```

## データフロー - 辞書検索

1. ユーザー入力: 「わたし」
2. CompositionBuffer: "わたし"
3. Converter.suggest(): 
   - Dictionary.lookup("わたし")
   - Trie木での高速検索
   - 候補生成: [渡し、私、綿、...]
4. CandidateList に格納
5. UI表示

## キー入力イベント処理

```cpp
void ImeEngine::processKeyEvent(const KeyEvent& key) {
    switch(key.virtualKey) {
        case VK_BACK:       // 削除
        case VK_SPACE:      // 候補選択開始
        case VK_RETURN:     // 確定
        case VK_ESCAPE:     // キャンセル
        case 'A'-'Z', ...  // 文字入力
    }
    
    // 状態遷移とUIの更新
    updateComposition();
    updateCandidateList();
}
```

## スレッド・同期モデル

- **単一スレッド**: IMEエンジンはメインスレッドで動作
- **非同期辞書読み込み**: ユーザー辞書はバックグラウンドで読み込み可能
- **COM STA**: TSFはシングルスレッドアパートメント (STA) で動作

## リソース管理

- **COM参照**: `Microsoft::WRL::ComPtr<>` で自動管理
- **メモリ**: `std::unique_ptr<>`, `std::shared_ptr<>` で自動管理
- **Atomic ファイル操作**: 辞書保存時にロックファイルを使用

## パフォーマンス最適化

1. **Trie木検索**: O(m) - m は入力文字列の長さ
2. **キャッシング**: 最近のクエリ結果をメモリなどに保持可能
3. **LTO（Link-Time Optimization）**: Release buildで有効化
