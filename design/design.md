# Game Design Document

> Working title: **TBD**
> Platform: Nintendo 64 (libdragon + tiny3d)
> Last updated: 2026-11-12

---

## 1. Vision

A 3D action-adventure set in a **90s-style dark fantasy** world. The player is a hero who looks like a villain: brooding, armored, wielding dark magic. Combat is built on sword and shield, and every weapon and shield holds its own dark spell. Those spells are also the player's only tools for exploration and puzzles.

**Tone:** grim, moody, and atmospheric. Dark fantasy, **not** eldritch or cosmic horror.

**Signature mechanic:** *Every weapon is a spell.* Collect weapons and shields; each one changes how you fight *and* where you can go.

---

## 2. Pillars

1. **Weighty melee:** sword, shield, lock-on, and timing.
2. **Dark magic from your gear:** spells come from equipment, not a separate spellbook.
3. **Gear is progression:** new weapons and shields open new paths. There are no separate tools.
4. **Atmosphere over detail:** fog, torchlight, strong silhouettes, a limited palette.

---

## 3. Controls

| Input | Action |
|---|---|
| **Analog stick** | Move |
| **A** | Context action (roll, talk, open, interact) |
| **B (tap)** | Swing equipped weapon. The swing happens on **release** |
| **B (hold past the threshold), then R** | Cast the **weapon's spell** |
| **R (hold)** | Guard with equipped shield (or a weaker **weapon guard** when two-handed) |
| **R (well-timed press as a hit lands)** | Shield **counter** |
| **R (hold), then B** | Cast the **shield's spell** |
| **Z** | Lock-on / target |
| **C-buttons** | Camera control |
| **L** | Use a **magic refill** charge |
| **Start** | Equipment screen (swap weapons and shields) |

### Input rules
- **Hold threshold:** B held longer than **~0.3s** enters weapon-spell stance and cancels the swing. *(Tunable. Try 0.2s if swings feel sluggish.)*
- **Same-frame press:** if B and R are pressed on the exact same frame, **nothing happens**.
- **Input order decides the spell:** whichever button is held *first* determines which item's spell is cast.

### Input state machine (concept)
```
Idle ──B pressed──► B Held ──released before threshold──► Swing ──► Idle
                       └──held past threshold──► Weapon Stance ──R──► Weapon Spell ──► Idle
Idle ──R pressed──► Guarding ──B──► Shield Spell ──► Idle
                       └──hit lands within counter window──► Counter ──► Idle
Idle ──B+R same frame──► (ignored) ──► Idle
```

---

## 4. Camera

- Free third-person camera that follows behind the player.
- C-buttons adjust it.
- **Z-targeting** locks onto an enemy. The player strafes and circles around the target.

---

## 5. Combat & Magic

### Magic bar
- A single **magic bar** (e.g. 0–100) shared by all spells.
- Each spell has a **magic cost**. Casting fails (fizzle sound/effect) if there isn't enough magic.
- **Refill method:** **limited refill items** (name TBD) the player carries.
  - Each use refills the magic bar (amount TBD).
  - The player holds a **limited number of charges**.
  - Charges are **restored at certain locations** (e.g. shrines or rest points, TBD).
  - **Upgrades** to the refill items can be found or earned (e.g. more charges, a bigger refill).
  - Press **L** to use a refill charge.

### Shield counter
- Pressing R starts a short **counter window** (~0.2s, tunable).
- If an enemy hit lands inside the window, it's a **counter**. Otherwise it's a normal block.

### Equipment
- The player has **one equipped weapon** and **one equipped shield**.
- Collected weapons and shields are kept in an inventory and swapped on the equipment screen.

### One-handed vs. two-handed weapons
- Weapons are either **one-handed** or **two-handed**.
- **One-handed** (e.g. Steel Sword): weapon in hand, shield on the arm. Full guard, counter, and shield spell.
- **Two-handed** (e.g. Dark Staff): **the shield can't be used** and is **shown slung on the player's back**.
  - **R (hold) = weapon guard:** block with the weapon itself. It's **weaker than a shield** (blocks less damage) and has **no counter**.
  - **R (hold) then B:** no shield spell (same "no magic" cue as a spell-less shield).
  - **B (hold) then R:** the weapon's spell works normally.
- Swapping weapons on the equipment screen automatically moves the shield between arm and back.

**Art impact:** two-handed weapons need their **own animation set** (idle, run, attack, guard). The shield and weapon models attach to **different bones** (hand bone vs. back bone) depending on what's equipped.

### Data model (concept)
- **Weapon:** name, model, damage, spell, spell magic cost, **one-handed or two-handed**
- **Shield:** name, model, block strength, spell, spell magic cost
- Each item's spell is stored as a **list** (starting with one entry), so more spells per item can be added later.

---

## 6. Weapons

> Weapons are also the game's **tools**. Each spell should have a use in **combat** and a use in **exploration or puzzles**.

| Name | Look | Spell | Combat use | World/puzzle use | Cost |
|---|---|---|---|---|---|
| **Steel Sword** *(starting weapon, one-handed)* | A weaker broadsword | **Energy slash:** a simple arc of dark energy fired forward | Ranged hit on enemies | *Suggestion:* hit out-of-reach switches or targets | *TBD (low)* |
| **Dark Staff** *(two-handed; name changes as it upgrades)* | *TBD* | **Deathblight:** a status effect (blight) on one target | Damage over time | *Suggestion:* wither cursed growth blocking paths | *TBD* |

### Dark Staff upgrade path
The Dark Staff is a **weapon on B**. It **grows stronger at major story milestones**, and **its name changes with each upgrade**. Each milestone adds or upgrades a spell (this fits the "spells are a list" data model).

| Story milestone | Staff name | Upgrade |
|---|---|---|
| *Start* | Dark Staff | **Deathblight:** single-target status effect, no area effect |
| *Milestone 1 (TBD)* | *TBD* | *Idea:* Deathblight gains an area effect (lingering cloud, or rot spreading along the ground) |
| *Milestone 2 (TBD)* | *TBD* | *TBD* |

## 7. Shields

| Name | Look | Spell | Combat use | World/puzzle use | Cost |
|---|---|---|---|---|---|
| **Wooden Shield** *(starting shield)* | Simple wooden shield lined with steel | **None** | Guard and counter only | None | n/a |
| **Shadow Bulwark** *(name may change)* | *TBD* | **Grasp** *(placeholder name)*: a shadow hand rises under the locked-on target and pins it to the ground | Holds one enemy still for a few seconds; **drags flying enemies down** to sword height | Pull down out-of-reach chains, levers, drawbridges, hanging cages | *TBD* |

*Later upgrade idea:* Grasp becomes an area effect, with hands erupting around the player and pinning every nearby enemy.

> **Note:** with a spell-less shield, **R (hold) then B does nothing** (or plays a small "no magic" cue). This also teaches players that shields *can* have spells, before they find one that does.

---

## 8. Protagonist

- A hero who is **dark and brooding** and looks almost like a villain.
- **Silhouette first:** one or two bold shapes (hood, ragged cape, horned pauldrons, distinctive helm…). *TBD*
- **Colors:** dark armor with **one accent color** for his magic. The same color is used for the magic bar and spell effects.
- **Target budget:** ~300–600 triangles.
- **Name:** *TBD*

---

## 9. World

- **Structure:** an open-ish connected world made of **areas joined by short transitions** (to fit N64 memory: 4MB, or 8MB with the Expansion Pak).
- **Atmosphere:** thick distance fog, torchlit interiors (point lights), desaturated greens, browns, and grays.
- **Regions/dungeons:** *TBD*

---

## 10. Art Pipeline

- Models made in **Blender**, exported as **.glb**, converted by tiny3d into `.t3dm`.
- Low poly. Mood comes from **vertex colors** and small textures (32×32 / 64×64).
- N64 texture memory is 4KB, so keep textures small.

---

## 11. Story

*TBD*

---

## 12. Open Questions

- [x] Magic bar refill: limited refill items, restored at certain locations, upgradable
- [ ] Name of the refill item
- [x] L uses a magic refill charge
- [ ] Later: unlock more spells per item, plus a spell-select menu?
- [ ] Game title, protagonist name, setting name
- [x] First items: Steel Sword, Wooden Shield, Dark Staff
- [x] How Deathblight works: starts as a single-target status effect, area effect later
- [ ] Dark Staff upgrade milestones and the names at each stage
- [x] First shield with a spell: Shadow Bulwark (Grasp)
- [ ] Final name for the Grasp spell
- [ ] Deathblight details: how long it lasts, damage per tick, can it stack?

---

## 13. Build Milestones

1. **A model on screen:** tiny3d renders a 3D model in a room
2. **A moving character:** analog movement, character faces its direction
3. **Follow camera:** orbiting camera with C-button control
4. **Collision:** floors and walls
5. **Animation:** idle, run, and attack (skeletal animation)
6. **Lock-on and combat:** Z-targeting, B swing, R guard, counter
7. **Magic system:** magic bar, weapon and shield spells, input state machine
8. **One complete area:** a small field with enemies
9. **Connected areas:** transitions between areas
