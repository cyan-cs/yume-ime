# ビルド方法

## ビルドだけする方法
PowerShellでこのリポジトリのルートへ移動します。

1. `build.ps1` を実行
```powershell
.\scripts\build.ps1
```

## レジストリーに登録しビルドする場合
管理者用 PowerShellでこのリポジトリのルートへ移動します。

1. `Register-YumeIME.ps1` を実行
```bash
.\scripts\register-YumeIME.ps1
```

## `Register-YumeIME.ps1` を使いレジストリーに登録したものをレジストリーから解除する方法
管理者用 PowerShellでこのリポジトリのルートへ移動します。

1. `Register-YumeIME.ps1` を実行
```bash
.\scripts\unregister-YumeIME.ps1
```