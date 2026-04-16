# Yume - IME

## 1. インデント・波かっこ
- インデントは **4スペース**
- 波かっこは K&R スタイル（関数・制御構文の行末に開く）

## 2. 名前付け規則
- **型名**: `PascalCase`
- **enum**: `enum class` を使用
- **関数・メソッド**: `camelCase`
- **メンバ変数**: `camelCase`
- **定数**: k プレフィックス付き `PascalCase`
- **型エイリアス**: `using` を使用
- **名前空間**: 多段 `yume::...` を使用

## 3. 安全性・ガード
- 状態分岐や設定値は **enum ベースで管理**
- 境界値チェックを徹底（`sanitizeSelection()`, `sanitizeSelectedSegmentIndex()` など）
- 異常系では「安全側に戻す」方針
- rollback や session reset を使い、内部状態を安全に保つ
- ファイル保存は **atomic save + error_code 検知**

## 4. リソース管理
- COM リソースは `Microsoft::WRL::ComPtr` で管理
- Win32/GDI ハンドルは RAII (`UniqueGdiObject`, `UniqueWindowHandle`) で管理
- optional な値は `std::optional` を使用

## 5. コード構造・ヘッダ
- ヘッダは `#pragma once` を使用
- include 順序: 自プロジェクト → 標準/OS ヘッダ
- 小さな内部 helper は無名 namespace に閉じ込める
- 責務ごとにファイルを分割し、DRY/KISS/SOLID を意識