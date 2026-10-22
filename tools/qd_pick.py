#!/usr/bin/env python3
"""Picks the next audition page from a quality-diversity archive.

    python tools/qd_pick.py <run folder under build/gen> <out folder> [--per Lead=5,Pad=4,...]

Renders the archive's elites, scores them (CLAP, taste model from the vote dataset), and keeps per category the best
ones that are new to ilana: below 0.85 CLAP cosine to every earlier page's preset and below 0.75 to the ones she
dropped, and 0.85 from each other. Writes clips/, presets/ and pack.json like tools/qd_search.py's pages.
"""
import argparse, csv, json, shutil, subprocess, sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import qd_search as q  # noqa: E402
from taste_pairs import clip, write  # noqa: E402
from taste_round2 import load  # noqa: E402

BASE = Path("/mnt/project-files/ilanasynth")
HEARD = [("search-winners", "qd"), ("search-winners-2", "qd2"), ("search-winners-3", "qd3")]
RENDERS = ["factory-render", "drafts-render", "drafts3m-render", "drafts3-render", "drafts3b-render", "drafts4-render",
           "drafts5-render", "drafts6-render", "drafts7-render", "drafts8-render", "drafts9b-render"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("out")
    ap.add_argument("--per", default="Lead=5,Pad=4,Pluck=4,Bass=4,Keys=4")
    ap.add_argument("--skip", default="", help="a run folder to leave out of the heard list (the run being picked)")
    a = ap.parse_args()
    G = ROOT / "build" / "gen"
    run = G / a.run
    final = run / "final"
    shutil.rmtree(final, ignore_errors=True)
    final.mkdir()
    for k, v in json.load(open(run / "archive.json")).items():
        shutil.copy(v["preset"], final / f"{k.replace('/', '_')}.ilanapreset")
    q.render(final, run / "final-render", keep=True)
    subprocess.run([sys.executable, str(ROOT / "tools/clap_score.py"), str(run / "final-render")], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, str(ROOT / "tools/taste_model.py"), "score", str(BASE / "taste/dataset.jsonl"),
                    str(run / "final-render")] + [str(G / r) for r in RENDERS], check=True, stdout=subprocess.DEVNULL)
    R = run / "final-render"
    rows, emb = load(R)
    ts = {r["name"]: float(r["taste"]) for r in csv.DictReader(open(R / "taste_score.csv"))}
    rep = {r["name"]: r for r in csv.DictReader(open(R / "report.csv"))}
    arr = np.array(list(ts.values()))
    tz = {k: (x - arr.mean()) / (arr.std() + 1e-9) for k, x in ts.items()}
    heard, dropped = [], []
    for pack, folder in HEARD:
        pj = BASE / pack / "pack.json"
        if folder == a.run or not pj.exists():
            continue
        r1, e1 = load(G / folder / "final-render")
        idx = {r["name"]: i for i, r in enumerate(r1)}
        for p in json.load(open(pj)):
            e = e1[idx[p["source"]]]
            heard.append(e)
            if p.get("vote") == "Drop":
                dropped.append(e)
    quota = {k: int(v) for k, v in (x.split("=") for x in a.per.split(","))}
    out = Path(a.out)
    shutil.rmtree(out, ignore_errors=True)
    (out / "clips").mkdir(parents=True)
    (out / "presets").mkdir()
    chosen, pack = [], []
    for cat, want in quota.items():
        ids = [i for i, r in enumerate(rows) if r["category"] == cat and clip(R, r["index"], cat) is not None
               and all(float(emb[i] @ e) < 0.85 for e in heard) and all(float(emb[i] @ e) < 0.75 for e in dropped)]
        ids.sort(key=lambda i: -(tz[rows[i]["name"]] + 0.02 * float(rep[rows[i]["name"]]["score"])))
        n = 0
        for i in ids:
            if n >= want:
                break
            if any(float(emb[i] @ e) >= 0.85 for e in chosen):
                continue
            r = rows[i]
            n += 1
            chosen.append(emb[i])
            pid = f"r3-{cat.lower()}-{n}"
            write(out / "clips" / f"{pid}.wav", clip(R, r["index"], cat))
            shutil.copy(final / f"{r['name']}.ilanapreset", out / "presets" / f"{pid}.ilanapreset")
            pack.append({"id": pid, "name": f"{cat} {n}", "category": cat, "source": r["name"], "preset": f"presets/{pid}.ilanapreset"})
        print(cat, n, "of", want, "new candidates", len(ids))
    json.dump(pack, open(out / "pack.json", "w"), indent=1)


if __name__ == "__main__":
    main()
