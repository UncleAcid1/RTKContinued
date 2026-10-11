# Offline balance plan: crystals, prices, units

Status: CONFIRMED by the user on 2026-10-11 (taper, units for 40% of their crystals plus food, the exchange's daily
limit, about a week per top-tier item after the campaign). Not coded yet except where noted. It follows the user's direction of 2026-10-11:
cut the "ridiculous" crystal prices by 55%, scale rewards by level but not too much, gear should
come in the late game and not the early game, units cost 60% fewer crystals plus a large amount of
food. Every number below is a starting value kept in a setting, so tuning never needs code changes.
The original data files are never edited: the port applies the rules when it loads them.

## 1. What the original charges (5.11 data files)

| What | Crystal prices |
|---|---|
| Potions, bottles, small tools | 1-10 (117 entries) |
| Consumables, boosts, cheap gear | 11-50 (145 entries) |
| Mid gear | 51-150 (89 entries) |
| High gear | 151-400 (20 entries) |
| Top gear (Paladin set, Gold Ring, Antique Sword) | 401-999 (19 entries) |
| Houses | 3, 5, 10 |
| Premium decorations | 40 each (6) |
| Units | 20-170 for most, Siege Troll 299, Scarab Queen 1999, Troll King 2000 |
| Speed-ups, Buy all | about 4-8 each use |

One of every crystal-priced item: about 33,500 crystals. The original gives 2 crystals per
level-up and 9 crystals from quests in the whole game; everything else was bought.

Food: a vegetable farm patch makes about 60 food an hour with any crop (carrots 5 per 5 min up
to cabbage 800 per 24 h); a farm has up to 12 patches and the player gets up to 7 farms (levels
5, 8, 10, 12, 18, 25). Storage: one Stockpile at the start, more at levels 7, 15, 20; each holds
1000 / 3000 / 4000 / 6000 by its upgrade (levels 3, 5, 10).

## 2. Prices of items, decorations, buildings

Rule: a price up to 20 crystals stays; above that, only the part over 20 is cut by 55%.

    new = price                      (price <= 20)
    new = 20 + (price - 20) x 0.45   (price > 20), rounded

Why not simply "x 0.45 above some line": that makes a 51-crystal item cheaper than a 50-crystal
one. With the taper nothing changes order, potions and speed-ups stay as they are, and the
expensive things come out at about 46% of their price.

| Original | New | | Original | New |
|---|---|---|---|---|
| 10 | 10 | | 300 | 146 |
| 20 | 20 | | 400 | 191 |
| 35 | 27 | | 600 | 281 |
| 50 | 34 | | 800 | 371 |
| 100 | 56 | | 999 | 461 |
| 150 | 78 | | decorations 40 | 29 |

One of everything: 33,500 -> about 18,300. Houses (3 / 5 / 10) are unchanged.

## 3. Units (hiring and training)

Units cost both (user, 2026-10-11, restated): the crystal price is cut by 60%, and a large amount
of food is needed as well.

    crystals = original x 0.40, rounded
    food     = original x 40, at most 20,000

The food cap is what a level-20 city can store (4 Stockpiles: 24,000), so the two giants need a
fully built storage. The food must be in storage when hiring, as any other cost.

| Unit | Original crystals | New crystals | Food | Roughly, with one 12-patch farm |
|---|---|---|---|---|
| Berserk (level 1) | 25 | 10 | 1,000 | 1.5 hours |
| Sniper | 45 | 18 | 1,800 | 2.5 hours |
| Storm Conjurer (level 4) | 79 | 32 | 3,160 | 4.5 hours |
| Tanker | 129 | 52 | 5,160 | 7 hours |
| Siege Troll | 299 | 120 | 11,960 | 17 hours |
| Scarab Queen / Troll King | 2,000 | 800 | 20,000 | 28 hours (3 nights of corn) |

With several farms the time divides accordingly, which is the reward for building up the city.
Units that cost only gold keep their gold price.

## 4. Crystal income

Level factor, used by every source ("by level, not too much": 1x at level 1, 3x at level 29):

    factor = 1 + (player level - 1) / 14

| Source | Crystals | When |
|---|---|---|
| Main quest with a reward (211) | 1 x factor | once each |
| Side quest with a reward (61) | 1 x factor | once each |
| "Complete the campaign" quest (7) | 5 x factor | once each |
| The 4 quests that paid crystals in 5.11 | their own 1 / 5 / 2 / 1, added on top | once each |
| Level-up | 2 (as 5.11) + level / 4 | 28 times, about 150 in all |
| Daily quest | 1 x factor | each day it is offered |
| Daily reward | 3 + level / 5 | once a day |
| Chest | small 1-2, medium 3-5, large 8-12 | as found |
| Hunt / boss / raid finished | 2 x factor | each time, proposed when those are ported |
| Arena win | 1 x factor, first 10 wins of a day | proposed when the arena is ported |

Hints and tutorial pop-ups give nothing. Results are rounded to whole crystals, at least 1.

### What that adds up to (estimate)

| Stage | Levels | One-time crystals in the stage | Running total | What it buys |
|---|---|---|---|---|
| Early | 1-8 | about 70 | 70 | speed-ups, potions, a house; at most one cheap piece of gear |
| Middle | 9-18 | about 300 | 370 | a handful of mid pieces (45-80 each) or a few units |
| Late | 19-29 | about 600 | 970 | a full mid set, or one or two top pieces (280-460) |
| After the campaign | 29 | 50-80 a day from dailies, daily reward, chests, hunts, arena | | a top piece about every week |

So the early game stays lean and gear arrives late, as asked. The stage numbers are estimates:
which quests fall at which level is only known once the campaign maps run (step 4f). See section 7.

## 5. Things that would break the plan if left alone

1. **The gold-for-crystals exchange.** Taxes produce gold without limit, so at the placeholder
   rate (400 gold a crystal) an idle city could buy everything. Decided: a daily limit of
   10 + level crystals from the exchange. Crystals-for-gold stays unlimited.
2. **Waiting to hand in quests at a higher level** pays a little more. With a 1x-3x factor the
   gain is small; accepted.
3. **Item level locks.** The shop shows "Level N required" for some items. If those locks turn out
   to be loose, early gear is still held back by price alone; if they are strict the plan gets
   safer. To be read when the item shop is ported (milestone 5).
4. **Storage.** The food prices assume the Stockpiles' spaces add up. If the limit turns out to be
   per building, the food cap drops to the largest single Stockpile (6,000) and the formula is
   scaled to match.

## 6. Unchanged

Speed-up and Buy-all prices, potion prices, the crystal reward shown in the level-up window (it
only grows), gold prices of everything, quest gold and XP rewards, drop tables.

## 7. How it will be checked before and after coding

1. `tools/balance_sim.py` (to write first): reads the data files, applies sections 2-4 and prints
   every new price, the income by level and how many days each tier takes. The tables above are
   replaced by its output.
2. The rules go in as settings (`port_price_keep`, `port_price_cut`, `port_unit_crystal_mult`,
   `port_unit_food_per_crystal`, `port_unit_food_max`, `port_quest_crystals_*`,
   `port_level_factor_div`, `port_exchange_daily_base`), each marked PORT in the code.
3. Where each part lands: quest and level-up crystals in 4c part 2 (now); unit prices with the
   barracks and tavern (milestone 5); item prices with the item shop (milestone 5); chests, daily
   reward, hunts, arena with their systems (milestone 5).
4. After the campaign maps are in (4f) the simulation is run again with the real quest-to-level
   spread and the numbers are tuned once.
