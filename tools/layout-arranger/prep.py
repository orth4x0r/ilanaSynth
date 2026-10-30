"""Builds the layout arranger's data from an ilanaSnapshot layout dump.

    ILANA_SNAPSHOT_LAYOUT=1 ILANA_SNAPSHOT_SCALE=1.75 ilanaSnapshot <dump dir>
    python3 tools/layout-arranger/prep.py <dump dir> <out dir> <commit>

For each page: the cards (painted rounded rectangles, found in the picture, or
top-level components in the header and dock), the controls inside them (the
outermost knob, menu, switch, button or display components), and three
pictures: the page as it is, the cards with their controls lifted out, and the
window behind the cards.
"""
import json
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

SCALE = 1.75
PAGE_BG = np.array([15, 16, 20])
CARD_COLOURS = {"15171C"}

PAGES = [
    # (tab label, dump name, what can move)
    ("PLAY", "MAIN", "content"),
    ("VECTOR", "VECTOR", "content"),
    ("MATRIX", "MATRIX", "content"),
    ("FM", "FM", "content"),
    ("SEQ", "ARP-SEQ", "content"),
    ("FX", "FX", "content"),
    ("PATCH", "PATCH", "content"),
    ("ENV pop-up", "POPUP-ENV", "popup"),
    ("LFO pop-up", "POPUP-LFO", "popup"),
    ("Header + dock", "MAIN", "chrome"),
]

# The arranger's starting layout for PLAY (card name: x, y, w, h); y past 720 is the parking strip.
PROPOSAL = {"play": {
    "SUB": [8, 425, 124, 108], "NOISE": [137, 425, 124, 108], "FILTER": [266, 425, 471, 108], "STRINGS": [742, 425, 310, 108],
    "ENVELOPE": [8, 533, 386, 108], "LFO": [399, 533, 351, 108], "BODY": [755, 533, 297, 108],
    "WEST": [8, 728, 344, 84], "ADD OSC ROW": [8, 816, 1044, 67],
}}
PROPOSAL_NOTE = ("Claude's start: the top row follows the sound (SUB + NOISE, FILTER, STRINGS), the bottom row is ENVELOPE, LFO and BODY. "
                 "WEST is parked because it becomes a third pill in FILTER (F1 | F2 | WEST); the ADD OSC row is parked because it "
                 "scrolls under the oscillators. Both rows are 108 px tall, so the oscillator area gets 20 px shorter (it scrolls).")

# Components that only hold other components: walk into them.
CONTAINER = re.compile(r"(Content|ShellTabs|SectionPage|::\w*Page$|Page$|Viewport|AccessibilityIgnoredComponent|SourceCarouselCard"
                       r"|SourcePopover|EnvSection|LfoSection|^juce::Component$|FxLibraryView|ButtonBar|MacroPager)")


def kind_of(cls):
    if re.search(r"KnobControl|StripKnob", cls):
        return "knob"
    if re.search(r"ComboControl|ComboBox|OscPicker", cls):
        return "menu"
    if re.search(r"ToggleControl|ChoiceSwitch|PillToggle|PillChoice", cls):
        return "switch"
    if re.search(r"Display|View|Picture|Diagram|Canvas|Editor|Carousel|Strip|Meter|Bar$|Wave", cls):
        return "graph"
    if re.search(r"Label", cls):
        return "label"
    return "button"


def tree(items):
    """Pre-order items with depth -> each item gets its children list."""
    stack = []
    roots = []
    for item in items:
        item["kids"] = []
        while stack and stack[-1]["d"] >= item["d"]:
            stack.pop()
        (stack[-1]["kids"] if stack else roots).append(item)
        stack.append(item)
    return roots


def words(item):
    if item.get("text"):
        return item["text"]
    for kid in item["kids"]:
        w = words(kid)
        if w:
            return w
    return item.get("name", "")


def controls_in(roots, region):
    out = []

    def visit(item):
        x, y, w, h = item["x"], item["y"], item["w"], item["h"]
        cx, cy = x + w / 2, y + h / 2
        inside = region[0] <= cx < region[2] and region[1] <= cy < region[3]
        if CONTAINER.search(item["cls"]) or not inside:
            for kid in item["kids"]:
                visit(kid)
            return
        if item["cls"].startswith("juce::ResizableCorner"):
            return
        clip = item.get("clip")
        if clip:
            x, y, w, h = clip
        if w < 4 or h < 4:
            return
        out.append({"kind": kind_of(item["cls"]), "cls": item["cls"].replace("(anonymous namespace)::", ""),
                    "label": words(item).replace("\n", " ").strip()[:40], "param": item.get("param", ""),
                    "x": x, "y": y, "w": w, "h": h})

    for root in roots:
        visit(root)
    return out


def find_cards(img, region, min_area=300):
    """Rounded cards: connected areas that are not the page background, inside the region (logical px)."""
    a = np.asarray(img.convert("RGB")).astype(int)
    diff = np.abs(a - PAGE_BG).sum(axis=2) > 6
    x0, y0, x1, y1 = [int(round(v * SCALE)) for v in region]
    mask = np.zeros_like(diff)
    mask[y0:y1, x0:x1] = diff[y0:y1, x0:x1]
    labels, n = ndimage.label(mask)
    boxes = []
    for i, sl in enumerate(ndimage.find_objects(labels)):
        if sl is None:
            continue
        ys, xs = sl
        w = (xs.stop - xs.start) / SCALE
        h = (ys.stop - ys.start) / SCALE
        if w * h < min_area or w < 12 or h < 12:
            continue
        boxes.append([int(np.floor(xs.start / SCALE)), int(np.floor(ys.start / SCALE)),
                      int(np.ceil(xs.stop / SCALE)), int(np.ceil(ys.stop / SCALE))])
    # Merge boxes inside other boxes (a picture's dark well inside its card).
    boxes.sort(key=lambda b: -(b[2] - b[0]) * (b[3] - b[1]))
    kept = []
    for b in boxes:
        if any(k[0] <= b[0] + 1 and k[1] <= b[1] + 1 and k[2] >= b[2] - 1 and k[3] >= b[3] - 1 for k in kept):
            continue
        kept.append(b)
    return [{"x": b[0], "y": b[1], "w": b[2] - b[0], "h": b[3] - b[1]} for b in kept]


def painted_cards(items, region):
    """The cards the page paints: big rounded fills in the card colour (the dump records them), outermost only."""
    found = set()
    for i in items:
        if re.search(r"PatchCableView|Diagram|Canvas|Display", i["cls"]):
            continue  # (boxes drawn inside a picture are not cards)
        for x, y, w, h, colour, rounded in i.get("fills", []):
            cx, cy = x + w / 2, y + h / 2
            if rounded and colour in CARD_COLOURS and w >= 60 and h >= 30 and region[0] <= cx < region[2] and region[1] <= cy < region[3]:
                found.add((x, y, w, h))
    boxes = sorted(found, key=lambda b: -b[2] * b[3])
    kept = []
    for b in boxes:
        if any(k[0] <= b[0] + 2 and k[1] <= b[1] + 2 and k[0] + k[2] >= b[0] + b[2] - 2 and k[1] + k[3] >= b[1] + b[3] - 2 for k in kept):
            continue
        kept.append(b)
    return [{"x": b[0], "y": b[1], "w": b[2], "h": b[3]} for b in kept]


def body_colour(a, x, y, w, h):
    """The card colour around a rectangle (median of a ring just outside it), full-res array."""
    X0, Y0 = max(0, int(x * SCALE) - 2), max(0, int(y * SCALE) - 2)
    X1, Y1 = min(a.shape[1] - 1, int((x + w) * SCALE) + 2), min(a.shape[0] - 1, int((y + h) * SCALE) + 2)
    ring = np.concatenate([a[Y0, X0:X1], a[Y1, X0:X1], a[Y0:Y1, X0], a[Y0:Y1, X1]])
    return np.median(ring, axis=0).astype(np.uint8)


def fill(a, x, y, w, h, colour):
    X0, Y0 = max(0, int(np.floor(x * SCALE))), max(0, int(np.floor(y * SCALE)))
    X1, Y1 = int(np.ceil((x + w) * SCALE)), int(np.ceil((y + h) * SCALE))
    a[Y0:Y1, X0:X1] = colour


def name_card(card, ctls, play_cards, idx):
    if card.get("label"):
        return pretty(card["label"])
    for name, (x, y, w, h) in play_cards.items():
        ix = min(card["x"] + card["w"], x + w) - max(card["x"], x)
        iy = min(card["y"] + card["h"], y + h) - max(card["y"], y)
        if ix > 0 and iy > 0 and ix * iy > 0.6 * w * h:
            return name
    if any("SegmentBar" in c["cls"] or "WaveDisplay" in c["cls"] for c in ctls):
        return "OSC"
    skip = {"ON", "EDIT", "+", "×", "X", "<", ">", "‹", "›", "PREV", "NEXT"}
    texts = [c for c in ctls if c["label"] and c["label"].upper() not in skip and c["kind"] in ("button", "switch", "label", "menu")]
    texts.sort(key=lambda c: (c["y"], c["x"]))
    if texts:
        return texts[0]["label"].upper()[:22]
    texts = [c for c in ctls if c["label"] and c["label"].upper() not in skip]
    if texts:
        return texts[0]["label"].upper()[:22]
    return "CARD %d" % (idx + 1)


def pretty(label):
    label = re.sub(r"^.*::", "", label)
    label = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", label)
    return label.upper()[:24]


def main():
    dump, out, commit = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    (out / "img").mkdir(parents=True, exist_ok=True)
    pages = []
    main_png = Image.open(dump / "layout-MAIN.png").convert("RGB")
    for label, name, what in PAGES:
        meta = json.loads((dump / f"layout-{name}.json").read_text())
        items = meta["items"]
        roots = tree(items)
        img = Image.open(dump / f"layout-{name}.png").convert("RGB")
        W, H = meta["w"], meta["h"]
        section = next(i for i in items if i["cls"] == "SectionPage")
        content = (section["x"], section["y"], section["x"] + section["w"], section["y"] + section["h"])
        play_cards = {}
        for i in items:
            for part in i.get("cards", "").split(";"):
                bits = part.split(",")
                if len(bits) == 5:
                    # (the page's own coordinates)
                    x, y, w, h = (int(v) for v in bits[1:])
                    play_cards[bits[0]] = (x + i["x"], y + i["y"], w, h)

        if what == "content":
            region = content
            cards = painted_cards(items, region)
            ctls = controls_in(roots, region)
            base_src = img
        elif what == "popup":
            pop = next(i for i in items if i["cls"].endswith("SourcePopover"))
            region = (pop["x"], pop["y"], pop["x"] + pop["w"], pop["y"] + pop["h"])
            cards = [{"x": pop["x"], "y": pop["y"], "w": pop["w"], "h": pop["h"], "label": label.replace("pop-up", "EDIT").upper()}]
            ctls = controls_in([pop], region)
            base_src = main_png
        else:  # header + dock: each top-level part outside the page is a card
            cards = []
            for i in roots[0]["kids"] if roots and roots[0]["cls"].endswith("Content") else roots:
                pass
            top = [i for i in items if i["d"] == 1]
            for i in top:
                cy = i["y"] + i["h"] / 2
                if content[1] <= cy < content[3] or i["cls"].endswith("ShellTabs"):
                    continue
                cards.append({"x": i["x"], "y": i["y"], "w": i["w"], "h": i["h"], "label": words(i) or pretty(i["cls"]), "cls": i["cls"]})
            tabs = next(i for i in items if i["cls"].endswith("ButtonBar"))
            cards.append({"x": tabs["x"], "y": tabs["y"], "w": max(t["x"] + t["w"] for t in items if t["cls"] == "juce::TabBarButton") - tabs["x"],
                          "h": tabs["h"], "label": "PAGE TABS", "cls": "tabs"})
            ctls = []
            for k in sorted(cards, key=lambda k: k["w"] * k["h"]):
                outer = [o for o in cards if o is not k and o["x"] <= k["x"] + 1 and o["y"] <= k["y"] + 1
                         and o["x"] + o["w"] >= k["x"] + k["w"] - 1 and o["y"] + o["h"] >= k["y"] + k["h"] - 1]
                if outer:
                    outer.sort(key=lambda o: o["w"] * o["h"])
                    ctls.append({"kind": "button", "cls": k["cls"], "label": k["label"], "param": "", "x": k["x"], "y": k["y"], "w": k["w"], "h": k["h"], "card": None})
                    k["gone"] = True
            cards = [k for k in cards if not k.get("gone")]
            base_src = img

        # Each control belongs to the smallest card holding its centre; cards without controls stay (graph-only cards).
        for c in ctls:
            cx, cy = c["x"] + c["w"] / 2, c["y"] + c["h"] / 2
            holders = [k for k in cards if k["x"] <= cx < k["x"] + k["w"] and k["y"] <= cy < k["y"] + k["h"]]
            holders.sort(key=lambda k: k["w"] * k["h"])
            c["card"] = holders[0] if holders else None
        for c in ctls:
            if c["card"] is None and what != "chrome":
                loose = {"x": c["x"], "y": c["y"], "w": c["w"], "h": c["h"], "label": c["label"] or c["cls"], "cls": c["cls"], "loose": True}
                cards.append(loose)
                c["card"] = loose
        ctls = [c for c in ctls if c["card"] is not None and not c["card"].get("loose")]

        a = np.asarray(img).copy()
        card_img = a.copy()
        for c in ctls:
            fill(card_img, c["x"], c["y"], c["w"], c["h"], body_colour(a, c["x"], c["y"], c["w"], c["h"]))
        base = np.asarray(base_src).copy()
        if what == "chrome":
            for k in cards:
                fill(base, k["x"], k["y"], k["w"], k["h"], body_colour(a, k["x"], k["y"], k["w"], k["h"]))
        elif what == "content":
            for k in cards:
                fill(base, k["x"], k["y"], k["w"], k["h"], PAGE_BG.astype(np.uint8))

        slug = re.sub(r"[^a-z0-9]+", "-", label.lower()).strip("-")
        Image.fromarray(a).save(out / "img" / f"{slug}.png", optimize=True)
        Image.fromarray(card_img).save(out / "img" / f"{slug}-cards.png", optimize=True)
        Image.fromarray(base).save(out / "img" / f"{slug}-base.png", optimize=True)

        card_list = []
        for n, k in enumerate(sorted(cards, key=lambda k: (k["y"], k["x"]))):
            mine = [c for c in ctls if c["card"] is k]
            nm = name_card(k, mine, play_cards, n)
            if nm == "OSC":
                osc_count = sum(1 for c in card_list if c["name"].startswith("OSC ")) + 1
                nm = "OSC %d" % osc_count
            if "OscDropTile" in k.get("cls", ""):
                nm = "ADD OSC ROW"
            card_list.append({"id": f"{slug}-c{n}", "name": (nm or "CARD").upper()[:24], "x": k["x"], "y": k["y"], "w": k["w"], "h": k["h"],
                              "bg": "#%02x%02x%02x" % tuple(body_colour(a, k["x"] + 8, k["y"] + 22, 1, 1))})
            k["id"] = card_list[-1]["id"]
        ctl_list = []
        for n, c in enumerate(ctls):
            ctl_list.append({"id": f"{slug}-k{n}", "card": c["card"]["id"], "kind": c["kind"], "cls": c["cls"], "label": c["label"],
                             "param": c["param"], "x": c["x"], "y": c["y"], "w": c["w"], "h": c["h"]})
        pages.append({"id": slug, "label": label, "kind": what, "w": W, "h": H, "scale": SCALE,
                      "img": f"img/{slug}.png", "bgimg": f"img/{slug}-cards.png", "base": f"img/{slug}-base.png",
                      "cards": card_list, "controls": ctl_list})
        print(label, len(card_list), "cards", len(ctl_list), "controls")

    data = {"commit": commit, "pages": pages, "proposal": PROPOSAL, "proposalNote": PROPOSAL_NOTE,
            "today": {"note": "gap between cards 5 px, window margin 7 px, card header 20 px, PLAY knob pitch 33-45 px (PlayPage.h)"}}
    (out / "data").mkdir(exist_ok=True)
    (out / "data" / "pages.json").write_text(json.dumps(data, separators=(",", ":")))


if __name__ == "__main__":
    main()
