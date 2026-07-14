# FIX-001 カレーマテリアルがパッケージで表示されない問題の修正

## 1. 既存プロジェクト構成の確認（完了）
- 技術スタック: Unreal Engine 5.7 / C++（モジュール `CarryUDON`）
- 該当クラス: `AHAND`（`Source/CarryUDON/HAND.h` / `HAND.cpp`）— APawn 派生
- カレー演出: ポストプロセスのブレンダブルとして画面全体に適用
  - 読込: `HAND.cpp` コンストラクタで `CurryScreenMaterial` を `ConstructorHelpers::FObjectFinder` で取得
  - 適用: `BeginPlay` で `UMaterialInstanceDynamic::Create` → `PostProcessComponent->AddOrUpdateBlendable`
  - 制御: `Tick` で `Opacity` スカラーパラメータをフェード

## 2. 根本原因
- C++ 参照パス `HAND.cpp:52` は `"/Game/seisaku/curry.curry"`（ルート直下）を指す。
- ルートの `Content/seisaku/curry.uasset` は **1251 バイトの ObjectRedirector（転送スタブ）** で、実体ではない。
- 実体は `Content/seisaku/Material/curry.uasset`（14697 バイト、`Opacity` パラメータを保持）に移動済み。
- `DefaultGame.ini` の `DirectoriesToAlwaysCook` には実体側の `/Game/seisaku/Material` のみ登録。ルートのリダイレクタは Cook 対象外。
- `/Game/seisaku/curry` を参照するのは本 C++ コードのみ（BP/Map からの参照なし）。

**結論:** エディタではリダイレクタを辿って実体に到達するため動作するが、パッケージではリダイレクタが Cook されず `FObjectFinder` が失敗 → `CurryScreenMaterial` が null → ブレンダブル適用がスキップ → 画面にカレー演出が出ない。

## 3. 修正内容
### Phase 1: C++ 参照パスの修正
- `HAND.cpp:52` の参照先を実体（かつ Cook 対象）へ変更
  - 変更前: `TEXT("/Game/seisaku/curry.curry")`
  - 変更後: `TEXT("/Game/seisaku/Material/curry.curry")`
- `HAND.cpp:51` のコメントを実態に合わせて更新
- 保存は UTF-8 (BOM付き) を維持

### Phase 2: ビルド確認
- モジュールをビルドし、コンパイルエラーがないことを確認

### 任意（別途承認）
- 不要なリダイレクタ `Content/seisaku/curry.uasset` をエディタの「リダイレクタを修正」で除去（削除操作のため未承認では実行しない）

## 4. 検証
- Shipping パッケージを作成し、カレー飛散演出が画面に表示されることを確認

## 5. 影響範囲
- 変更ファイル: `Source/CarryUDON/HAND.cpp`（1行のパス + コメント）のみ
- 挙動変更: パッケージ時にマテリアルが正しくロードされるようになる。エディタ挙動は不変。
