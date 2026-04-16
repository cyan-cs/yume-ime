# Yume IME インストールガイド

## システム要件

- **OS**: Windows 10/11 (x64)
- **アーキテクチャ**: x86-64 (x64のみ)
- **ディスク容量**: 約50 MB
- **メモリ**: 最小256 MB

## インストール方法

### 方法1: 簡易インストール（ユーザー向け推奨）

**必要なもの**:
- PowerShell 5.0以上
- 管理者権限

**インストール手順**:

1. リポジトリまたはリリースをダウンロード
2. PowerShellを**管理者として実行**して起動
3. リポジトリのルートに移動：
   ```powershell
   cd C:\path\to\yume-ime
   ```
4. 登録スクリプトを実行：
   ```powershell
   .\scripts\Register-YumeIME.ps1
   ```
5. Windowsにレジストリ登録されます

**参考**:
- エラーが出た場合は .NET Framework 4.5以上がインストールされているか確認してください

### 方法2: ソースからビルド

**必要なもの**:
- Visual Studio 2022 Community以降（C++ワークロード）
- CMake 3.20以上
- PowerShell 5.0以上
- Git

**ビルド手順**:

1. リポジトリをクローン：
   ```powershell
   git clone https://github.com/cyan-cs/yume-ime.git
   cd yume-ime
   ```

2. ビルドのみ実行（登録なし）：
   ```powershell
   .\scripts\build.ps1
   ```
   - 出力: `./build/Release/YumeIME.dll`

3. ビルド＆登録：
   ```powershell
   # 管理者として実行
   .\scripts\Register-YumeIME.ps1
   ```

### 方法3: 手動登録

スクリプトが動作しない場合の手動登録方法：

```powershell
# 管理者として実行
$imePath = "C:\path\to\yume-ime\build\Release\YumeIME.dll"

# Winodwsレジストリに登録
regsvr32 $imePath
```

## Windows での有効化

インストール後：

1. **設定** → **時刻と言語** → **言語と地域** を開く
2. 「中国語、日本語、または韓国語の入力方式」から **言語を追加** をクリック
3. **日本語** を選択
4. Yume IMEが入力方式一覧に表示されます
5. 必要に応じてデフォルト入力方式に設定

切り替えは Alt+` (グレーブキー) または Ctrl+Shift で可能です。

## アンインストール

Yume IMEを削除する場合：

```powershell
# 管理者として実行
.\scripts\unregister-YumeIME.ps1
```

または手動で：
```powershell
$imePath = "C:\path\to\yume-ime\build\Release\YumeIME.dll"
regsvr32 /u $imePath
```

## トラブルシューティング

### 「アクセスが拒否されました」エラー
- PowerShellが管理者権限で実行されているか確認
- `regsvr32` を使用した手動登録を試してください

### IMEが Windows の設定に表示されない
- ソースから再度ビルドしてDLLが作成されたか確認
- DLLが64ビット (x64) であることを確認
- .NET Framework がインストールされているか確認

### 「パスが見つかりません」エラー
- リポジトリのルートディレクトリにいることを確認
- `scripts/Register-YumeIME.ps1` ファイルが存在することを確認

その他の問題は [TROUBLESHOOTING.md](./troubleshooting_ja.md) を参照してください。

## インストール確認

インストール後、IMEをテストしてみてください：

1. メモ帳などのテキストエディタを開く
2. Yume IMEに切り替え
3. 日本語を入力（ひらがな）：
   - 入力: 「わたし」
   - 候補: [私, 渡し, ...]
4. ↑/↓ で候補を選択し、Space/Enter で確定

## ファイルの場所

- **ユーザーデータ**: `%APPDATA%\Yume IME\` - ユーザーデータディレクトリ
- **ログ**: `%APPDATA%\Yume IME\logs\` - デバッグログ
- **辞書**: `%APPDATA%\Yume IME\settings\` - カスタム辞書保存場所
