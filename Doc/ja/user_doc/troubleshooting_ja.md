# トラブルシューティング - Yume IME

## インストール関連の問題

### エラー: 「アクセスが拒否されました」

**原因**: スクリプトが管理者権限を必要とします

**解決方法**:
1. PowerShell を右クリックして「管理者として実行」を選択
2. 登録スクリプトを再度実行：
   ```powershell
   .\scripts\Register-YumeIME.ps1
   ```

### エラー: 「パスが見つかりません」

**原因**: ビルド出力が見つからない（まだビルドされていない）

**解決方法**:
1. まずプロジェクトをビルド：
   ```powershell
   .\scripts\build.ps1 -Configuration Release
   ```
2. `.\build\Release\YumeIME.dll` が存在することを確認
3. その後、登録スクリプトを実行

### エラー: 「.NET Framework がインストールされていない」

**原因**: .NET Framework の依存関係が不足

**解決方法**:
1. [.NET Framework 4.5+](https://dotnet.microsoft.com/download/dotnet-framework) をダウンロード
2. インストール実行
3. コンピューターを再起動
4. 登録スクリプトを再度実行

### Windows の入力方式設定に IME が表示されない

**原因**: DLL が正しく登録されていない、またはアーキテクチャが異なる

**解決方法**:
1. DLL が 64 ビットであることを確認：
   ```powershell
   # ファイルプロパティまたは以下で確認:
   dumpbin /headers .\build\Release\YumeIME.dll | findstr /C:"machine"
   ```
   「machine (x64)」と表示されるはず

2. 管理者権限で手動登録：
   ```powershell
   regsvr32 "C:\full\path\to\build\Release\YumeIME.dll"
   ```

3. それでも表示されない場合はイベントビューアを確認：
   - Windows ログ → システム
   - 「YumeIME」関連のエラーを探す

## 実行時の問題

### IME が応答しない、または入力時にクラッシュする

**原因**: メモリ問題または初期化問題の可能性

**解決方法**:
1. ログでエラーを確認：
   ```powershell
   cat "$env:APPDATA\Yume IME\logs\debug.log" | Select-Object -Last 50
   ```

2. 登録を解除して再度登録：
   ```powershell
   # 管理者として実行
   .\scripts\unregister-YumeIME.ps1
   .\scripts\Register-YumeIME.ps1
   ```

3. それでもクラッシュする場合、ログファイルを付けてバグ報告してください

### 候補ウィンドウが表示されない

**原因**: UI レンダラーの問題またはテーマの非互換性

**解決方法**:
1. Windows のディスプレイスケーリングが 100% に設定されされているか確認
2. 別のテキストエディタ（メモ帳、Word など）を試す
3. システムログを確認：`%APPDATA%\Yume IME\logs\`
4. バグ報告時に OS バージョンとディスプレイ設定を含める

### 入力モード切り替えが働かない（ラテン/ひらがな）

**原因**: ステートマシンバグまたは入力イベント喪失

**解決方法**:
1. IME を再起動：
   - Alt+` で入力方式を切り替えて、再度 Yume IME に戻す
   - または Ctrl+Shift を使用

2. アプリケーションを再起動（メモ帳など）

3. 問題が継続する場合、ログを確認してバグ報告

## 入力・タイピング関連の問題

### スペースキーを押しても候補が生成されない

**原因**: 辞書が読み込まれていない、または候補生成に失敗

**解決方法**:
1. 辞書が存在することを確認：
   ```powershell
   ls "$env:APPDATA\Yume IME\settings\*"
   ```

2. ログで辞書読み込みエラーを確認：
   ```powershell
   cat "$env:APPDATA\Yume IME\logs\debug.log" | Select-String "Dictionary"
   ```

3. ひらがなで入力しているか確認：
   - 「ひらがな」で入力（「Hiragana」ではない）

### ハイフン（-）を押すと問題が発生（todo の既知バグ）

**原因**: 既知の問題 - ハイフンが候補選択を誤ってロック

**回避方法**:
- 一時的にハイフン入力を避ける
- 利用可能な場合は代替区切り文字を使用
- v0.2 での修正予定

### スペースが候補選択を誤ってロック（todo の既知バグ）

**原因**: 現在のバージョンの既知問題

**回避方法**:
- Escape を押してキャンセルして再試行
- または現在の選択肢で確定（受け入れられる場合）
- v0.2 リリースでの修正予定

## パフォーマンス関連の問題

### IME が遅い、またはラグがある

**原因**: 辞書サイズが大きすぎる、またはシステムが高負荷

**解決方法**:
1. システムが高負荷状態かを確認：
   ```powershell
   Get-Process | Sort-Object CPU -Descending | Select-Object -First 5
   ```

2. 古いログファイルをアーカイブ：
   ```powershell
   Remove-Item "$env:APPDATA\Yume IME\logs\*.log" -OlderThan (Get-Date).AddDays(-30)
   ```

3. カスタム辞書が存在する場合はサイズを削減

## アンインストール関連の問題

### IME 登録解除でエラー

**原因**: 権限不足または DLL が使用中

**解決方法**:
1. IME を使用しているすべてのアプリケーションを閉じる
2. PowerShell を管理者として実行
3. アンインストールスクリプトを実行：
   ```powershell
   .\scripts\unregister-YumeIME.ps1
   ```

4. それでも失敗する場合は手動登録解除：
   ```powershell
   regsvr32 /u "C:\full\path\to\build\Release\YumeIME.dll"
   ```

## バグ報告

上記にない問題が発生した場合：

1. **情報を収集**:
   - OS バージョン: `winver`
   - ログファイル: `%APPDATA%\Yume IME\logs\debug.log`
   - 再現手順
   - エラーメッセージ

2. **GitHub Issue を作成** して以下を記載:
   - 明確なタイトル
   - 再現手順
   - 期待される動作と実際の動作
   - システム情報
   - ログの該当部分

3. **設定情報を含める**（該当する場合）:
   - `%APPDATA%\Yume IME\settings\` の内容

## ヘルプを得る

- [インストールガイド](./install_ja.md) を確認
- [アーキテクチャガイド](./architecture_ja.md) でシステム設計を確認
- 既存の [GitHub Issues](https://github.com/cyan-cs/yume-ime/issues) を検索
- [Discussion](https://github.com/cyan-cs/yume-ime/discussions) で質問

---

それでも解決しない場合は、GitHub でイシューを報告してください！
