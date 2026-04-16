# Yume IME に貢献する

Yume IME への貢献を検討していただき、ありがとうございます。あらゆる種類の貢献を歓迎します。

## 行動規範

このプロジェクトは [Contributor Covenant 3.0 行動規範](../../../CODE_OF_CONDUCT.md) に従っています。参加することで、この規範を遵守することに同意したものとみなされます。

## 貢献方法

### バグ報告

1. [GitHub Issues](https://github.com/cyan-cs/yume-ime/issues) で同様のバグが既に報告されていないか確認
2. 新規にIssueを作成する場合：
   - 問題を明確に説明するタイトル
   - 再現手順
   - 期待される動作と実際の動作
   - OSバージョン、Windows ビルド番号

### 機能提案

1. GitHub DiscussionまたはIssueを作成
2. ユースケースと望ましい動作を説明
3. 実装の複雑さを考慮

### コード投稿

#### 開発環境のセットアップ

1. リポジトリをクローン：
   ```powershell
   git clone https://github.com/cyan-cs/yume-ime.git
   cd yume-ime
   ```

2. 環境を構成：
   ```powershell
   .\scripts\build.ps1  # 初回実行で依存関係をインストール
   ```

3. Visual Studio 2022 で開く：
   ```powershell
   .\build\YumeIME.sln
   ```

#### コーディングガイドライン

[コーディングスタイルガイド](./coding_style_ja.md) に従ってください：

- **言語**: C++20
- **インデント**: 4スペース
- **中括弧**: K&Rスタイル
- **命名規則**: ガイド参照（型は PascalCase、関数は camelCase など）
- **コンパイル**: `/W4 /WE` (MSVC) フラグで必ずコンパイルできるようにしてください

#### 変更を加える

1. フィーチャーブランチを作成：
   ```powershell
   git checkout -b feature/my-feature
   ```

2. 焦点を絞ったコミットを作成：
   ```powershell
   git add .
   git commit -m "スペース入力時の候補選択バグを修正"
   ```

3. ローカルでテストを実行：
   ```powershell
   cd .\build-tests-local
   cmake --build . --config Release
   ctest --output-on-failure
   ```

4. フォークにプッシュしてプルリクエストを作成：
   ```powershell
   git push origin feature/my-feature
   ```

#### プルリクエストのプロセス

1. CI がパスしていることを確認（全テスト実行）
2. 変更内容を明確に説明
3. 関連する Issue をリンク
4. PR は単一の関心事に焦点を当てる

#### コードレビュー

メンテナは以下を行います：
- スタイル、ロジック、パフォーマンスをレビュー
- 必要に応じて修正を要求
- 承認後にマージ

## 開発ワークフロー

### プロジェクト構造

詳細なシステム設計は [アーキテクチャガイド](./architecture_ja.md) を参照してください。

主要ディレクトリ：
- `src/ime/` - コアエンジン（テストは `tests/ime/`）
- `src/platform/tsf/` - Windows TextService Framework
- `src/ui/` - ユーザーインターフェース

### ビルド

```powershell
# デバッグビルド（デバッグシンボル付き）
.\scripts\build.ps1 -Configuration Debug

# リリースビルド
.\scripts\build.ps1 -Configuration Release

# ビルド＆テスト
cd build-tests-local
cmake --build . --config Release
ctest
```

### テスト

ユニットテストを実行：
```powershell
# CTest を使用
ctest --output-on-failure

# または直接実行
.\build-tests-local\Release\ImeCoreSelfTests.exe
```

[tests/ime/](../../tests/ime/) に新機能のテストを追加してください。

### デバッグ

1. Visual Studio でコアエンジンモジュール内にブレークポイントを設置
2. デバッガを実行 (F5)
3. ログを表示: `%APPDATA%\Yume IME\logs\debug.log`

## ドキュメント

貢献する際：
- ユーザー向け機能を追加した場合は README を更新
- システム設計を変更した場合はアーキテクチャドキュメントを更新
- 複雑なロジックにはコメントを追加
- 日本語または英語で統一

## 貢献できる領域

- **バグ修正**: "good first issue" ラベルを参照
- **パフォーマンス**: 辞書検索、レンダリングの最適化
- **機能**: 新しい入力モード、辞書管理、UIテーマ
- **ローマ字化**: 英語ドキュメントと翻訳
- **テスト**: より多くのユニットテスト、統合テスト

## ライセンス

貢献することで、あなたは [MIT ライセンス](../../LICENSE) の下であなたのコードをライセンスすることに同意します。

## 質問がある場合

- GitHub Discussion を開く
- [アーキテクチャガイド](./architecture_ja.md) を確認
- [環境セットアップ](./environment_ja.md) を読む

---

Yume IME への貢献ありがとうございます！
