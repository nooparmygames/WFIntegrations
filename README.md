# WFIntegrations

These are a set of actions and considerations which integrate [WiseFeline Lite](https://www.fab.com/listings/4d4e95e3-4693-4e30-96e8-d0a25c6098e6) and [Wise Feline Ultimate](https://www.fab.com/listings/94f4a41c-6235-4fd7-ab47-321b49562994) with other plugins and AI systems.

This repository holds the plugin source. Precompiled binaries are hosted separately so Blueprint-only
projects without a C++ compiler installed can use the plugin too.

## Downloads

Every download is the fully precompiled plugin - editor binaries plus Development and Shipping
game binaries - so a Blueprint-only project can use it in the editor *and* package a game with no
C++ compiler installed.

| Engine | Download |
|---|---|
| UE 5.8 | [wfint58100.zip](https://www.nooparmygames.com/wfdownloads/wfint58100.zip) |
| UE 5.7 | [wfint57100.zip](https://www.nooparmygames.com/wfdownloads/wfint57100.zip) |
| UE 5.6 | [wfint56100.zip](https://www.nooparmygames.com/wfdownloads/wfint56100.zip) |

Unzip the download into your project's `Plugins` folder (so you end up with `Plugins/WFIntegrations`) and enable the plugin.

Two things worth knowing:

- The precompiled binaries are **Windows (Win64) only**. The plugin supports Linux, Mac, Android and
  iOS as well, but those are built from source, so targeting them needs a C++ project and a compiler.
- Pick the download that matches your engine's **minor** version. Each one works with every hotfix of
  that version, so the 5.7 download is good for any 5.7.x.

## Dependencies

WFIntegrations is a single module that hard-depends on all of the following - they must be installed
and enabled for the plugin to load:

- [Wise Feline](https://www.fab.com/listings/4d4e95e3-4693-4e30-96e8-d0a25c6098e6) (Lite or Ultimate)
- [Wise Feline Influence Maps](https://www.fab.com/listings/de2fe0f1-e173-4646-b8c4-ebe724c41326)
- Smart Objects (engine plugin)
- Gameplay Ability System / GAS (engine plugin)
- Gameplay Behavior Smart Objects (engine plugin)

## What's included

### Gameplay Ability System (GAS)

- **Activate Ability** (action) - activates a Gameplay Ability by class on the controlled actor's AbilitySystemComponent, optionally staying active until the ability ends.
- **Can Activate Ability** (consideration) - scores 1 when a Gameplay Ability by class can currently be activated (granted, off cooldown, cost/tag checks pass), 0 otherwise. Pair with Activate Ability so the brain only picks it when ready.
- **Gameplay Attribute** (consideration) - scores a GAS attribute's current value, normalized to 0-1 either against a fixed max or against another attribute (e.g. Health / MaxHealth).
- **Gameplay Tag** (consideration) - scores 1 when the AbilitySystemComponent owns the configured gameplay tag(s), matching any or all of them, optionally inverted (e.g. gate on "not stunned").

### Smart Objects

- **Smart Object** (consideration) - read-only check for whether a usable Smart Object is available nearby; can score by distance and can write the found slot's location to a blackboard key for a MoveTo action.
- **Claim Smart Object** (action) - finds and reserves a Smart Object slot, storing the claim handle in a blackboard key.
- **Has Claimed Smart Object** (consideration) - scores 1 when the blackboard holds a valid Smart Object claim, so other Smart Object actions/considerations can gate on it.
- **Use Smart Object** (action) - the full reserve -> move -> occupy -> wait -> release lifecycle for a Smart Object, driven by the utility brain.
- **Use GameplayBehavior Smart Object** (action) - claims a slot and runs the engine's `UAITask_UseGameplayBehaviorSmartObject` so the pawn moves to the slot and actually performs the object's GameplayBehavior (animation, montage, etc.).
- **Release Smart Object** (action) - frees a held Smart Object claim.

### Influence Maps

- **Influence Map** (consideration) - searches a Wise Feline Influence Map for a value (centered on the controlled actor or a blackboard location) and writes the result to a blackboard key.

The current source adds a `SearchMode` setting. `FirstMatch` keeps the previous behavior and is
the default. `HighestValue` and `LowestValue` find the area's best value, choosing the cell nearest
the search center when several cells share it. The selected value must still meet `SearchCondition`
and `SearchValue`; if it does not, the consideration returns 0. These modes also work with interest
and self templates.

With Influence Maps 3.2.0, the result written to the blackboard is the cell center. Remove any
half-cell offset you previously added to that result yourself.

The hosted 1.0 binary downloads above predate these source changes. They will be replaced separately;
use the source checkout to build the updated consideration in the meantime.

## Integrations in the main plugin

Some of the integrations which don't require dependencies live in the main plugin.
They include:

- EQS
- Behavior Trees
- Perception System
- Blackboards
- Gameplay Tags
