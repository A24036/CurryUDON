# FIX-002 タイトル／メインUIのテキストが箱（LastResort）になる問題の修正

## 1. 既存プロジェクト構成の確認（完了）
- 技術スタック: Unreal Engine 5.7 / C++（モジュール `CarryUDON`）
- UI: UMG ウィジェット（`Content/seisaku/BluePrint/*_WBP.uasset`）
  - `Title_WBP`（タイトル画面）
  - `GameMode_WBP` / `GameMode_WBP1` / `GameMode_WBP2`（ゲーム中HUD）
  - `GameOver_WBP`（ゲームオーバー）

## 2. 根本原因
- 画面に出る箱は UE の LastResort フォント（Unicodeブロック名＋コードポイント表示）。指定フォントが1グリフも読めない時に出る。
- コミット `8fc5a7f 細かい調整` で日本語フォント「Yuji Syuku」の資産2つが削除されていた:
  - `Content/seisaku/BluePrint/YujiSyuku-Regular.uasset`（8.4MB, FontFace／ttf実体）
  - `Content/seisaku/BluePrint/YujiSyuku-Regular_Font.uasset`（6.4KB, Font／合成フォント）
- 一方、`Title_WBP` / `GameMode_WBP2` / `GameOver_WBP` は今も `YujiSyuku-Regular_Font` を参照。
- 参照先フォントが存在しないため、実行時に解決できず全テキストが LastResort の箱になる。

**結論:** ウィジェットが参照するフォント資産が削除されたことが原因。削除直前の状態から復元すれば解決する。

## 3. 修正内容
### Phase 1: フォント資産の復元
- 削除直前のコミット `8fc5a7f^`（= `dbd7548`）から以下を復元:
  - `Content/seisaku/BluePrint/YujiSyuku-Regular.uasset`
  - `Content/seisaku/BluePrint/YujiSyuku-Regular_Font.uasset`
- コマンド:
  `git checkout 8fc5a7f^ -- "Content/seisaku/BluePrint/YujiSyuku-Regular.uasset" "Content/seisaku/BluePrint/YujiSyuku-Regular_Font.uasset"`

### Phase 2: 参照解決の静的確認
- 復元ファイル名とウィジェット参照名（`YujiSyuku-Regular_Font`）の一致を確認
- Font（`_Font`）が FontFace（`YujiSyuku-Regular`）を参照していることを確認

## 4. 検証
- Standalone プレビューを再起動し、タイトル／HUD／ゲームオーバーのテキストが正しく表示されることを確認
- パッケージでも同様に表示されること（フォントは各ウィジェットの依存として自動的に Cook 対象）

## 5. 影響範囲
- 追加ファイル: フォント資産2つの復元のみ（コード変更なし）
- ウィジェット自体は無変更。参照が解決されるようになる。
