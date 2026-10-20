"""Duplicate rule for anything shown to ilana (vote pages, candidate packs).

CLAP cosine alone missed sounds she hears as one (FM Brass 1/2/Wide scored only 0.4-0.9 against
each other), so a draft is also a duplicate when it shares a sound FAMILY with an earlier pick.
"""

SAME = 0.85  # was 0.93
FAMILIES = {  # concept -> family; one pick per family per page/pack
    "fm-brass": ["FM Brass", "FM Brass Wide", "FM Brass Echo", "FM Brass Saturated Lead", "FM Brass Pulse Lead",
                 "Vangelis Brass", "Horn Stab", "Holt Brass Sweep"],
    "low-grit": ["Grit Bass", "Reese Sub", "Acid Low", "Stab Bass"],
}
_FAMILY_OF = {c: f for f, cs in FAMILIES.items() for c in cs}


def family(concept):
    return _FAMILY_OF.get(concept.split("~")[0], concept.split("~")[0])


def is_duplicate(emb, concept, chosen):
    """chosen: list of (embedding, concept). True if too close in CLAP or the same family."""
    fam = family(concept)
    return any(float(emb @ e) >= SAME or family(c) == fam for e, c in chosen)
