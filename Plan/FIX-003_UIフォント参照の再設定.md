# FIX-003 UIウィジェットのフォント参照を再設定する

## 1. 背景（FIX-002 の続き）
- FIX-002 で削除フォント資産（`YujiSyuku-Regular` / `YujiSyuku-Regular_Font`）をディスクに復元済み。
- しかし Standalone で依然テキストが LastResort の箱のまま。

## 2b. 真因（実行ログで確定・最重要）
Standalone 実行ログに以下が出力された:
- `Title_WBP` / `GameMode_WBP2` → 依存 `/Engine/EditorResources/YujiSyuku-Regular_Font` を解決不可
- `GameOver_WBP` → 依存 `/Engine/EngineFonts/YujiSyuku-Regular_Font` を解決不可

→ ウィジェットが参照するフォントのパスは **プロジェクト内ではなくエンジン側 (`/Engine/...`)**。
元開発者がフォントをエンジンContentに取り込んで参照したため、各PCローカルにしか存在せず
Git管理外。環境が変わると解決できず LastResort の箱になる。
FIX-002 の `/Game/...` 復元は正しい素材だが、ウィジェットの参照先が別（エンジン側）のため
無効だった。エンジンパスへ置くのは不可（EditorResources 等は非Cook、かつワークスペース外）。
**正解は、3ウィジェットのテキスト参照を `/Game/seisaku/BluePrint/YujiSyuku-Regular_Font` へ付け替えること。**
また `get_all_widgets` が空を返したため、列挙を RootWidget からの再帰方式へ変更。

## 2. 根本原因（当初の想定・参考）
- 対象3ウィジェットは、フォント削除コミット `8fc5a7f` より**後**に再保存されていた:
  - `Title_WBP`（→`74551e8`）/ `GameOver_WBP`（→`4ec8ef5`）/ `GameMode_WBP2`（→`cd4a170`）
- フォント不在の状態で再保存されたため、テキストブロックの **FontObject への生きた参照が null 化・破棄**され、裸の名前文字列だけが残存。
- 証拠: 現行 `Title_WBP` にはテクスチャ等のフルパス参照は在るが、`/Game/seisaku/BluePrint/YujiSyuku-Regular_Font` のフルパス参照が**無い**。
- よってアセット復元だけでは参照が繋がらず、箱のまま。

## 3. 修正内容
### Phase 1: フォント参照の再設定（Python一括）
- 対象ウィジェット: `Title_WBP` / `GameMode_WBP2` / `GameOver_WBP`
- 各ウィジェットツリー内の全 `TextBlock` の Font を `YujiSyuku-Regular_Font` に再設定
- Typeface は "Default" を明示設定
- ブループリントをコンパイルして保存
- スクリプト: `Scripts/fix_ui_fonts.py`
- 実行: 開いているエディタの Cmd 欄で `py "C:/CurryUDON/Scripts/fix_ui_fonts.py"`

### 実行前の注意
- スクリプト実行までエディタで「すべて保存」をしない（null 状態の確定を防ぐ）。

## 4. 検証
- スクリプトのログで変更した TextBlock 数を確認
- Standalone プレビューでタイトル／HUD／ゲームオーバーのテキストが正しく表示されることを確認
- 併せてパッケージでも表示されること

## 5. 影響範囲
- 変更: 3ウィジェットのテキストブロックの Font プロパティのみ
- コード変更なし。テキスト内容・レイアウトは不変。
