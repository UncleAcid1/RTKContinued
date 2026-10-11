#!/usr/bin/env python3
"""Offline balance check (docs/balance_plan.md): reads 5.11's data files and prints the new crystal
prices, the units' food prices and the crystal income the plan's settings give.

usage: python3 tools/balance_sim.py [--items] [--units]
The quests' spread over the player's levels is not known before the campaign maps run (step 4f):
until then the quest income assumes they are spread evenly over levels 1..29."""
import os, re, sys

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../out/assets/android511/res_files/1Original/")
# the plan's settings
PRICE_KEEP, PRICE_CUT = 20, 0.55
UNIT_CRYSTAL_MULT, UNIT_FOOD_PER_CRYSTAL, UNIT_FOOD_MAX = 0.40, 40, 20000
LEVEL_FACTOR_DIV, MAX_LEVEL = 14, 29
QUEST_MAIN, QUEST_SIDE, QUEST_CAMPAIGN, QUEST_DAILY = 1, 1, 5, 1

def read(name): return open(D + name, encoding="utf-8", errors="replace").read()
def attrs(t): return dict(re.findall(r"(\w+)\s*=\s*[\"']([^\"']*)[\"']", t))
def num(v): return int(v) if v and v.lstrip("-").isdigit() else 0
names = {m.group(1).lower(): m.group(2) for m in re.finditer(r'<t i="EN_([^"]+)">([^<]*)<', read("LocalizedStringsEN.xml"))}
def nm(k): return names.get(str(k).lower(), k)

def price(p): return p if p <= PRICE_KEEP else round(PRICE_KEEP + (p - PRICE_KEEP) * (1 - PRICE_CUT))
def factor(level): return 1 + (level - 1) / LEVEL_FACTOR_DIV
def crystals(base, level): return max(1, int(base * factor(level) + 0.5))

items = [a for a in (attrs(m.group(0)) for m in re.finditer(r"<Item\b[^>]*>", read("items.xml"))) if num(a.get("cost2")) > 0]
old = sum(num(i["cost2"]) for i in items)
new = sum(price(num(i["cost2"])) for i in items)
print(f"ITEMS with a crystal price: {len(items)}; one of each {old} -> {new}")
for lo, hi in ((1, 20), (21, 50), (51, 150), (151, 400), (401, 99999)):
    band = sorted(num(i["cost2"]) for i in items if lo <= num(i["cost2"]) <= hi)
    if band: print(f"  {lo:>4}-{hi:<5} {len(band):>3} items  {band[0]}..{band[-1]} -> {price(band[0])}..{price(band[-1])}")
if "--items" in sys.argv:
    for i in sorted(items, key=lambda i: num(i["cost2"])):
        print(f"  {num(i['cost2']):>5} -> {price(num(i['cost2'])):>4}  {nm(i.get('name'))}")

units = [a for a in (attrs(m.group(0)) for m in re.finditer(r"<\w+ id=[^>]*>", read("persons.xml"))) if num(a.get("warrior_cost2")) > 0]
print(f"UNITS with a crystal price: {len(units)} (hired with fewer crystals plus food)")
rows = sorted({(num(u["warrior_cost2"]), nm(u.get("name"))) for u in units})
for c, n in rows if "--units" in sys.argv else rows[:3] + rows[-4:]:
    food = min(UNIT_FOOD_MAX, round(c * UNIT_FOOD_PER_CRYSTAL))
    print(f"  {c:>5} crystals -> {round(c * UNIT_CRYSTAL_MULT):>4} crystals + {food:>6} food ({food / 720:.1f} h of one 12-patch farm)  {n}")

kinds = {"main": 0, "side": 0, "campaign": 0, "daily": 0}
for head, body in re.findall(r"<XTask (.*?)>(.*?)</XTask>", read("tasks.xml"), re.S):
    conds = dict(re.findall(r'<XCondition type="(\w+)" vmin="([^"]*)"', body))
    if conds.get("is_hint") == "1" or conds.get("tutorial_type") == "1": continue
    award = attrs(re.search(r"<XAward([^>]*)>", body).group(1))
    rewarded = any(num(v) for v in award.values()) or "items" in conds
    if 'type="complete_compaign"' in body: kinds["campaign"] += 1
    elif conds.get("daily") == "1": kinds["daily"] += 1
    elif not rewarded: continue
    elif conds.get("sidequest") == "1": kinds["side"] += 1
    else: kinds["main"] += 1
print("QUESTS paying crystals:", kinds)
total = 0
print("INCOME by level (quests spread evenly; cumulative, one-time sources only):")
for level in range(1, MAX_LEVEL + 1):
    q = (kinds["main"] * crystals(QUEST_MAIN, level) + kinds["side"] * crystals(QUEST_SIDE, level) +
         kinds["campaign"] * crystals(QUEST_CAMPAIGN, level)) / MAX_LEVEL
    up = 2 + level // 4 if level > 1 else 0
    total += q + up
    if level in (1, 5, 8, 12, 18, 24, 29):
        print(f"  level {level:>2}: factor {factor(level):.2f}, a quest {crystals(QUEST_MAIN, level)}, level-up {up}, total so far {total:.0f}")
daily = 3 + MAX_LEVEL // 5 + 3 * crystals(QUEST_DAILY, MAX_LEVEL)
print(f"AFTER THE CAMPAIGN: daily reward + 3 daily quests = {daily} a day, before chests, hunts and the arena")
