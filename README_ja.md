
# Yume - 日本語入力IME

### ![Read in English here](./README.md)
完全OSS+MITライセンス。ひらがな入力でカタカナ・ローマ字候補を自動生成し、効率的な入力体験を提供します。<br />
Copyright (c) 2026-present cyan-cs

![再読み込み推奨](./assets/gif/testPlay.gif)

## 1. 特徴
- このIMEのすべてのコードがMITライセンスの下で公開されています。
- 現代ではあまり使われなくなった半角カタカナや全角英字を廃止しています。
- 比較的モダンなUIを採用しています。

## 2. インストール方法

**簡易インストール（推奨）**:
```powershell
# 管理者権限で実行
.\scripts\Register-YumeIME.ps1
```

詳細は [インストールガイド](./Doc/ja/user_doc/install_ja.md) を参照してください。

## 3. ビルド方法

### ビルドだけする場合
```powershell
.\scripts\build.ps1
```

### ビルド＆レジストリ登録
```powershell
# 管理者権限で実行
.\scripts\Register-YumeIME.ps1
```

### 登録解除
```powershell
# 管理者権限で実行
.\scripts\unregister-YumeIME.ps1
```

詳細は [ビルドガイド](./Doc/ja/user_doc/build_ja.md) を参照してください。

## 4. ドキュメント

- **ユーザー向け**
  - [インストール方法](./Doc/ja/user_doc/install_ja.md)
  - [トラブルシューティング](./Doc/ja/user_doc/troubleshooting_ja.md)

- **開発者向け**
  - [アーキテクチャ](./Doc/ja/dev_doc/architecture_ja.md)
  - [コーディングスタイル](./Doc/ja/dev_doc/coding_style_ja.md)
  - [貢献ガイド](./Doc/ja/dev_doc/contributing_ja.md)
  - [環境セットアップ](./Doc/ja/dev_doc/environment_ja.md)

- **English Documentation**
  - [Installation Guide](./Doc/en/user_doc/install_en.md)
  - [Troubleshooting](./Doc/en/user_doc/troubleshooting_en.md)
  - [Architecture](./Doc/en/dev_doc/architecture_en.md)
  - [Contributing Guide](./Doc/en/dev_doc/contributing_en.md)

## 5. 既知の問題（Todo）

v0.2で修正予定:
- 数字の後に優先辞書の融合
- ハイフン入力時の予測確定バグ
- スペース入力時の予測確定バグ

詳細は [トラブルシューティング](./Doc/ja/user_doc/troubleshooting_ja.md#已知-issues) を参照。

## 6. ライセンス
Yume-IMEのコードはすべて[MIT LICENSE](https://opensource.org/license/mit) の下で公開されています。

