# -*- coding: utf-8 -*-
"""
FIX-003c: UIウィジェットのテキスト部品のフォント参照を、壊れたエンジンパス参照
(/Engine/EditorResources/... , /Engine/EngineFonts/...) から、プロジェクト内の
正しいフォント /Game/seisaku/BluePrint/YujiSyuku-Regular_Font へ付け替える。

UE5.1+ では WidgetTree が WidgetBlueprint ではなく生成クラス側にあるため、
生成クラス / CDO 経由でツリーを取得する。

実行: 開いているUEエディタの Cmd 欄で
      py "C:/CurryUDON/Scripts/fix_ui_fonts.py"
"""
import unreal

PROJECT_FONT = "/Game/seisaku/BluePrint/YujiSyuku-Regular_Font"
TYPEFACE = "Default"

WIDGET_PATHS = [
    "/Game/seisaku/BluePrint/Title_WBP",
    "/Game/seisaku/BluePrint/GameMode_WBP2",
    "/Game/seisaku/BluePrint/GameOver_WBP",
]


def log(msg):
    unreal.log("[fix_ui_fonts] " + msg)


def get_widget_tree(bp):
    """生成クラス / CDO / ブループリント本体 の順で WidgetTree を探す。"""
    candidates = []
    gc = None
    try:
        gc = bp.generated_class()
    except Exception:
        gc = None
    if gc is not None:
        candidates.append(("generated_class", gc))
        try:
            candidates.append(("cdo", unreal.get_default_object(gc)))
        except Exception:
            pass
    candidates.append(("blueprint", bp))

    for src_name, obj in candidates:
        for prop in ("widget_tree", "WidgetTree"):
            try:
                t = obj.get_editor_property(prop)
            except Exception:
                t = None
            if t is not None:
                log("   WidgetTree 取得元: %s" % src_name)
                return t
    return None


def collect_widgets(tree):
    # まず get_all_widgets、だめなら RootWidget から再帰
    try:
        ws = list(tree.get_all_widgets())
        if ws:
            return ws
    except Exception:
        pass
    out = []
    try:
        root = tree.get_editor_property("root_widget")
    except Exception:
        root = None

    def walk(w):
        if w is None:
            return
        out.append(w)
        if isinstance(w, unreal.PanelWidget):
            for i in range(w.get_children_count()):
                walk(w.get_child_at(i))
    walk(root)
    return out


def try_set_font(widget, font):
    try:
        fi = widget.get_editor_property("font")
    except Exception:
        return False
    if fi is None:
        return False
    try:
        fi.set_editor_property("font_object", font)
        fi.set_editor_property("typeface_font_name", TYPEFACE)
        widget.set_editor_property("font", fi)
        return True
    except Exception as e:
        unreal.log_error("[fix_ui_fonts]   font設定失敗 %s: %s" % (widget.get_name(), e))
        return False


def main():
    font = unreal.load_asset(PROJECT_FONT)
    if font is None:
        unreal.log_error("[fix_ui_fonts] フォントが見つかりません: %s" % PROJECT_FONT)
        return
    log("フォント読込OK: %s" % PROJECT_FONT)

    total_changed = 0
    for path in WIDGET_PATHS:
        bp = unreal.load_asset(path)
        if bp is None:
            unreal.log_warning("[fix_ui_fonts] ウィジェット未検出: %s" % path)
            continue

        tree = get_widget_tree(bp)
        if tree is None:
            unreal.log_error("[fix_ui_fonts] %s : WidgetTree を取得できませんでした" % path)
            continue

        widgets = collect_widgets(tree)
        log("%s : ウィジェット %d 個を検出" % (path, len(widgets)))
        changed = 0
        for w in widgets:
            try:
                cls = w.get_class().get_name()
            except Exception:
                cls = "?"
            did = try_set_font(w, font)
            mark = " <= フォント設定" if did else ""
            log("   - %s [%s]%s" % (w.get_name(), cls, mark))
            if did:
                changed += 1

        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as e:
            unreal.log_warning("[fix_ui_fonts] compile失敗(継続): %s" % e)
        saved = unreal.EditorAssetLibrary.save_asset(path, False)
        log("%s : %d 件設定, save=%s" % (path, changed, saved))
        total_changed += changed

    log("完了。合計 %d 件のテキストにフォントを再設定しました。" % total_changed)


main()
