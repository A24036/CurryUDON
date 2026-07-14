# 001_タイマー0時のリザルト遷移バグ修正

## 概要
ゲーム内タイマーが0になった際、リザルト画面へのシーン遷移が機能しない不具合を修正する。

## Phase 1: プロジェクト構成の確認（完了）

- 技術スタック: Unreal Engine 5.7 / C++（`CarryUDON` モジュール）
- ゲームフローとマップ構成:

| マップ | サイズ | 内容 | MapsToCook |
|--------|--------|------|:---:|
| `start` | 105KB | タイトル（`Title_WBP`）＝起動時マップ | ✅ |
| `Main` | 113KB | ゲーム本編（`GameMode_WBP` HUD・HANDポーン） | ✅ |
| `GameOverMap` | 83KB | リザルト画面（`GameOver_WBP` を表示） | ✅ |
| `NewMap` | 2KB | ほぼ空のスタブマップ。`GameOver_WBP` 参照なし | ❌ 未登録 |

## 原因

`Source/CarryUDON/HAND.cpp` の `Tick` 内、タイマー0時の遷移先マップ名が誤っている。

```cpp
UGameplayStatics::OpenLevel(this, FName("NewMap"));
```

- `NewMap` はほぼ空のスタブマップで、リザルトWidget（`GameOver_WBP`）を持たない。
- `NewMap` は `Config/DefaultGame.ini` の `MapsToCook` に未登録のため、
  パッケージ版では存在せず `OpenLevel` が失敗し、遷移が機能しない。
- 本来のリザルト画面は `GameOverMap`（`GameOver_WBP` を表示・クック対象登録済み）。

タイマー減算・ゲームオーバー判定・スコア保存のロジック自体は正しく動作している。

## Phase 2: 修正の実施

- `Source/CarryUDON/HAND.cpp` の遷移先を `NewMap` → `GameOverMap` に変更（1行のみ）。

```diff
- UGameplayStatics::OpenLevel(this, FName("NewMap"));
+ UGameplayStatics::OpenLevel(this, FName("GameOverMap"));
```

- 保存は UTF-8 (BOM付き)。既存バイトを壊さないバイト保全型置換で対応。
- Blueprint / マップ資産（`.uasset` / `.umap`）には一切変更を加えない。

## Phase 3: ビルド確認

| # | コマンド | 目的 |
|---|---------|------|
| 1 | `Build.bat CarryUDONEditor Win64 Development -Project="C:\CurryUDON\CarryUDON.uproject" -WaitMutex` | C++モジュールのビルド確認 |

ビルド成功をもって作業完了とする。

## 備考

- 別解として「空の `NewMap` を `MapsToCook` に追加しリザルトを作り込む」方法もあるが、
  完成済みの `GameOverMap` が既に存在するため、遷移先修正が最小かつ確実と判断した。
