"""Scores how much each rendered preset sounds like its category and its
name, with the LAION CLAP text-audio model (laion/clap-htsat-unfused, from
Hugging Face; downloaded on first use).

    build/venv/Scripts/python tools/clap_score.py build/critic-full [more folders...]

Reads each folder's index.csv and <index>/note.wav and chord.wav (from
ilanaPresetRender or ilanaRefHost --presets), and writes clap.csv next to
index.csv: category_score (0-1: how clearly the model picks the preset's own
category over the other four) and name_score (cosine similarity between the
audio and "a <name> synthesizer sound"). preset_critic.py reads clap.csv when
it is there.
"""

import csv
import sys
import wave
from pathlib import Path

import numpy as np
import torch
from transformers import ClapModel, ClapProcessor

MODEL = "laion/clap-htsat-unfused"
PROMPTS = {
    "Bass": "a deep synthesizer bass sound",
    "Pad": "a soft sustained synthesizer pad",
    "Lead": "a bright synthesizer lead melody sound",
    "Keys": "a keyboard instrument like an electric piano or organ",
    "Pluck": "a short plucked synthesizer sound",
}


def read_mono(path):
    with wave.open(str(path), "rb") as w:
        channels, width, frames = w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(frames)
    if width == 3:
        data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        ints = data[:, 0].astype(np.int32) | (data[:, 1].astype(np.int32) << 8) | (data[:, 2].astype(np.int32) << 16)
        ints = np.where(ints >= 1 << 23, ints - (1 << 24), ints)
        audio = ints.astype(np.float32) / float(1 << 23)
    else:
        audio = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    audio = audio.reshape(-1, channels).mean(axis=1)
    peak = float(np.max(np.abs(audio))) if len(audio) else 0.0
    return audio / peak if peak > 1e-6 else audio  # CLAP judges timbre, not level


def features(output):
    """Newer transformers return an output object rather than the tensor."""
    if isinstance(output, torch.Tensor):
        return output
    for key in ("text_embeds", "audio_embeds", "pooler_output"):
        value = getattr(output, key, None)
        if value is not None:
            return value
    raise TypeError(f"unexpected CLAP output {type(output)}")


def process_audio(processor, clips):
    try:
        return processor(audio=clips, sampling_rate=48000, return_tensors="pt")
    except TypeError:
        return processor(audios=clips, sampling_rate=48000, return_tensors="pt")


def main():
    folders = [Path(p) for p in sys.argv[1:]]
    if not folders:
        print(__doc__)
        return 1
    torch.set_num_threads(max(1, torch.get_num_threads()))
    model = ClapModel.from_pretrained(MODEL).eval()
    processor = ClapProcessor.from_pretrained(MODEL)

    categories = list(PROMPTS)
    with torch.no_grad():
        text = processor(text=[PROMPTS[c] for c in categories], return_tensors="pt", padding=True)
        category_embeddings = torch.nn.functional.normalize(features(model.get_text_features(**text)), dim=-1)

    for folder in folders:
        with open(folder / "index.csv", newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        out = []
        for row in rows:
            clips = [read_mono(folder / row["index"] / "note.wav"), read_mono(folder / row["index"] / "chord.wav")]
            with torch.no_grad():
                audio = torch.nn.functional.normalize(features(model.get_audio_features(**process_audio(processor, clips))), dim=-1)
                audio = torch.nn.functional.normalize(audio.mean(dim=0, keepdim=True), dim=-1)
                name_text = processor(text=[f"a {row['name']} synthesizer sound"], return_tensors="pt", padding=True)
                name_embedding = torch.nn.functional.normalize(features(model.get_text_features(**name_text)), dim=-1)
            similarities = (audio @ category_embeddings.T)[0]
            probabilities = torch.softmax(similarities * 100.0, dim=0).numpy()
            category = row["category"]
            category_score = float(probabilities[categories.index(category)]) if category in categories else float("nan")
            name_score = float((audio @ name_embedding.T)[0, 0])
            best = categories[int(np.argmax(probabilities))]
            out.append({"index": row["index"], "name": row["name"], "category": category,
                        "category_score": f"{category_score:.4f}", "heard_as": best, "name_score": f"{name_score:.4f}"})
            print(f"{folder.name} {row['index']} {row['name']}: {category} {category_score:.2f} (heard as {best}), name {name_score:.2f}",
                  flush=True)
        with open(folder / "clap.csv", "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(out[0].keys()))
            writer.writeheader()
            writer.writerows(out)
        print(f"wrote {folder / 'clap.csv'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
