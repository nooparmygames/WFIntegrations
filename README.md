# WFIntegrations

These are a set of actions and considerations which integrate [WiseFeline Lite](https://www.fab.com/listings/4d4e95e3-4693-4e30-96e8-d0a25c6098e6) and [Wise Feline Ultimate](https://www.fab.com/listings/94f4a41c-6235-4fd7-ab47-321b49562994) with other plugins and AI systems.

This repository holds the plugin source. Precompiled binaries are hosted separately so Blueprint-only
projects without a C++ compiler installed can use the plugin too.

## Downloads

The WFIntegrations 3.5.0 builds require Wise Feline Utility AI 3.5.0 and
Influence Maps 3.5.0. Choose the ZIP for your engine and Utility AI edition.
Ultimate and Lite binaries cannot be interchanged.

| Unreal Engine | Ultimate (Win64) | Lite (Win64) |
| --- | --- | --- |
| UE 5.8 | [wfint58350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfint58350.zip) | [wfintlite58350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfintlite58350.zip) |
| UE 5.7 | [wfint57350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfint57350.zip) | [wfintlite57350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfintlite57350.zip) |
| UE 5.6 | [wfint56350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfint56350.zip) | [wfintlite56350.zip](https://downloads.nooparmygames.com/releases/3.5.0/integrations/wfintlite56350.zip) |

Extract the ZIP into your project's `Plugins` folder so you have
`Plugins/WFIntegrations`, then enable the plugin and restart Unreal.
The Win64 ZIPs include Editor, Development and Shipping build products for
Blueprint-only projects. For other platforms, compile the plugin from source
against the matching dependencies.

See [what changed in 3.5.0](WFIntegrations/README.md#350).

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

- **Move to Influence Map Value** (action) - selects a destination from an influence map once, then follows a complete NavMesh path. [Setup and fallback behavior](WFIntegrations/README.md#move-to-an-influence-map-result).
- **Influence Map** (consideration) - searches a Wise Feline Influence Map for a value (centered on the controlled actor or a blackboard location) and writes the result to a blackboard key.

The `SearchMode` setting controls which cell is selected. `FirstMatch` keeps the previous behavior and is
the default. `HighestValue` and `LowestValue` find the area's best value, choosing the cell nearest
the search center when several cells share it. The selected value must still meet `SearchCondition`
and `SearchValue`; if it does not, the consideration returns 0. These modes also work with interest
and self templates.

With Influence Maps 3.2.0, the result written to the blackboard is the cell center. Remove any
half-cell offset you previously added to that result yourself.

## Integrations in the main plugin

Some of the integrations which don't require dependencies live in the main plugin.
They include:

- EQS
- Behavior Trees
- Perception System
- Blackboards
- Gameplay Tags
